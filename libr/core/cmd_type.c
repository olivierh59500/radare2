/* radare - LGPL - Copyright 2009-2014 - pancake, Anton Kochkov, Jody Frankowski */

static void show_help(RCore *core) {
	const char * help_message[] = {
		"Usage: t", "",    "# cparse types commands",
		"t",   "",         "List all loaded types",
		"t",   " <type>",  "Show type in 'pf' syntax",
		"t*",  "",         "List types info in r2 commands",
		"t-",  " <name>",  "Delete types by its name",
		"t-*", "",         "Remove all types",
		//"t-!", "",          "Use to open $EDITOR",
		"td",  " <string>","Load types from string",
		"td-", "<name>",   "Undefine type by name",
		"tf",  " <addr>",  "View linked type at given address",
		"tl",  "[?]",      "Show/Link type to a address",
		//"to",  "",         "List opened files",
		"to",  " -",       "Open cfg.editor to load types",
		"to",  " <path>",  "Load types from C header file",
		"ts",  " <k>=<v>", "Set fields at curseek linked type",
		//"| ts k=v k=v @ link.addr set fields at given linked type\n"
		NULL
	};
	r_core_cmd_help (core, help_message);
}

static int sdbforcb (void *p, const char *k, const char *v) {
	r_cons_printf ("%s=%s\n", k, v);
	return 1;
}

static int cmd_type(void *data, const char *input) {
	RCore *core = (RCore*)data;

	switch (input[0]) {
	// t [typename] - show given type in C syntax
	case 's':
	{
		char *q, *p, *o, *e;
		p = o = strdup (input+1);
		for (;;) {
			q = strchr (p, ' ');
			if (q) *q = 0;
			if (!*p) {
				p++;
				continue;
			}
			e = strchr (p, '=');
			if (e) {
				*e = 0;
				r_anal_type_set (core->anal, core->offset,
					p, r_num_math (core->num, e+1));
			} else eprintf ("TODO: implement get\n");
			if (!q) break;
			p = q+1;
		}
		free (o);
	}
		break;
	case ' ':
	{
		char *fmt = r_anal_type_format (core->anal, input +1);
		if (fmt) {
			r_cons_printf ("pf %s\n", fmt);
			free (fmt);
		} else eprintf ("Cannot find '%s' type\n", input+1);
	}
		break;
#if 0
	// t* - list all types in 'pf' syntax
	case '*':
		r_anal_type_list (core->anal, R_ANAL_TYPE_ANY, 1);
		break;
#endif
	case 0:
		// TODO: use r_cons here
		//sdb_list (core->anal->sdb_types);
		sdb_foreach (core->anal->sdb_types, sdbforcb, core);
		break;
	case 'o':
		if (input[1] == ' ') {
			const char *filename = input + 2;
			if (!strcmp (filename, "-")) {
				char *out, *tmp;
				tmp = r_core_editor (core, "");
				if (tmp) {
					out = r_parse_c_string (tmp);
					if (out) {
						r_cons_strcat (out);
						sdb_query_lines (core->anal->sdb_types, out);
						free (out);
					}
					free (tmp);
				}
			} else {
				char *out = r_parse_c_file (filename);
				if (out) {
					r_cons_strcat (out);
					sdb_query_lines (core->anal->sdb_types, out);
					free (out);
				}
				//r_anal_type_loadfile (core->anal, filename);
			}
		}
		break;
	// td - parse string with cparse engine and load types from it
	case 'd':
		if (input[1] == '?') {
			const char * help_message[] = {
				"Usage:", "td[...]", "",
				"td", "[string]", "Load types from string",
				NULL
			 };

			r_core_cmd_help(core, help_message);
		} else
		if (input[1] == '-') {
			const char *arg = strchr (input+1, ' ');
			if (arg) arg++; else arg = input+2;
			r_anal_type_del (core->anal, arg);
		} else
		if (input[1] == ' ') {
			char tmp[256];
			snprintf (tmp, sizeof (tmp), "%s;", input+2);
			//const char *string = input + 2;
			//r_anal_str_to_type (core->anal, string);
			char *out = r_parse_c_string (tmp);
			if (out) {
				r_cons_strcat (out);
				sdb_query_lines (core->anal->sdb_types, out);
				free (out);
			}
		} else {
			eprintf ("Invalid use of td. See td? for help\n");
		}
		break;
	// tl - link a type to an address
	case 'l':
		if (input[1]=='?') {
			const char * help_message[] = {
				"Usage: tl", " [typename|addr] ([addr])@[addr|function]", "",
				NULL
			 };

			r_core_cmd_help(core, help_message);
		} else if (input[1]) {
			ut64 addr = r_num_math (core->num, input+2);
			char *ptr = strchr (input + 2, ' ');
			if (ptr) {
				addr = r_num_math (core->num, ptr + 1);
				*ptr = '\0';
			} else addr = core->offset;
			r_anal_type_link (core->anal, input+2, addr);
		} else {
			r_core_cmd0 (core, "t~^link");
		}
		break;
	case '-':
		if (input[1] == '?') {
			const char * help_message[] = {
				"Usage: t-", " <type>", "Delete type by its name",
				NULL
			 };

			r_core_cmd_help(core, help_message);
		} else
		if (input[1]=='*') {
			eprintf ("TODO\n");
		} else {
			const char *name = input + 1;
			if (*name==' ') name++;
			if (*name) {
				r_anal_type_del (core->anal, name);
			} else eprintf ("Invalid use of t- . See t-? for help.\n");
		}
		break;
	// tv - get/set type value linked to a given address
	case 'f':
		 {
			ut64 addr;
			char *fmt, key[128];
			const char *type;
			if (input[1]) {
				addr = r_num_math (core->num, input+1);
			} else addr = core->offset;
			snprintf (key, sizeof (key), "link.%08"PFMT64x, addr);
			type = sdb_const_get (core->anal->sdb_types, key, 0);
			if (type) {
				fmt = r_anal_type_format (core->anal, type);
				r_cons_printf ("struct %s {\n", type);
				if (fmt) {
					r_core_cmdf (core, "pf %s @ 0x%08"PFMT64x"\n", fmt, addr);
					free (fmt);
				}// else eprintf ("Cannot find '%s' type\n", input+1);
				r_cons_printf ("}\n");
			} else eprintf ("Cant find type at 0x%llx\n", addr);
		 }
		break;
	case '?':
		if (input[1]) {
			sdb_query (core->anal->sdb_types, input+1);
		} else show_help(core);

		break;
	}
	return R_TRUE;
}
