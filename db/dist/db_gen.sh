#!/bin/sh -
#
# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)db_gen.sh	10.17 (Sleepycat) 5/2/98
#

# This script generates all the log, print, and read routines for the db
# package logging. It also generates the template for the recovery
# functions (these must be hand-written, but are highly stylized so that
# the initial template gets you a fair way along the path).  For a given
# file prefix.src, we generate a file prefix_auto.c.  This file contains
# PUBLIC declarations that will help us generate the appropriate extern
# declarations.  It also creates a file prefix_auto.h that contains:
# 	defines for the physical record types (logical types are
#		defined in eacy subsystem manually)
#	structures to contain the data unmarshalled from the log.
PROG=db_gen.sh
if [ $# -ne 2 ]; then
	echo "Usage: db_gen.sh <name>.src dir"
	exit 1
fi

ifile=$1
dir=$2
dname=`basename $ifile .src`
hfile=../include/$dname"_auto.h"
cfile=$dir/$dname"_auto.c"
rfile=template/"rec_"$dname

# Create the files as automagically generated files
echo "Building $hfile, $cfile, $rfile"
msg="/* Do not edit: automatically built by dist/db_gen.sh. */"
echo "$msg" > $hfile; chmod 444 $hfile
echo "$msg" > $cfile; chmod 444 $cfile
echo "$msg" > $rfile; chmod 444 $rfile

# Start processing the input file
awk  '
/^[ 	]*PREFIX/ {
	prefix = $2

	# Write Cfile headers

	printf("#include \"config.h\"\n\n") >> CFILE
	printf("#ifndef NO_SYSTEM_INCLUDES\n") >> CFILE
	printf("#include <ctype.h>\n") >> CFILE
	printf("#include <errno.h>\n") >> CFILE
	printf("#include <stddef.h>\n") >> CFILE
	printf("#include <stdlib.h>\n") >> CFILE
	printf("#include <string.h>\n") >> CFILE
	printf("#endif\n\n") >> CFILE
	printf("#include \"db_int.h\"\n") >> CFILE
	printf("#include \"shqueue.h\"\n") >> CFILE
	printf("#include \"db_page.h\"\n") >> CFILE
	printf("#include \"db_dispatch.h\"\n") >> CFILE
	if (prefix != "db")
		printf("#include \"%s.h\"\n", DIR) >> CFILE
	printf("#include \"db_am.h\"\n", DIR) >> CFILE

	# Write Recover file headers
	cmd = sprintf("sed -e s/PREF/%s/ < \
	    template/rec_htemp > %s", prefix, RFILE);
	system(cmd);

	num_funcs = 0;
	printf("#ifndef %s_AUTO_H\n#define %s_AUTO_H\n", prefix, prefix) \
	    >> HFILE;
}
/^[ 	]*BEGIN/ {
	if (in_begin) {
		print "Invalid format: missing END statement"
		error++;
	}
	in_begin = 1;
	is_sync = $3;
	structs++;
	nvars=0;
	funcname=sprintf("%s_%s", prefix, $2);
	thisfunc = $2;
	funcs[num_funcs] = funcname;
	num_funcs++;
	is_dbt = 0;
}
/^[ 	]*(ARG|DBT|POINTER)/ {
	vars[nvars] = $2;
	types[nvars] = $3;
	atypes[nvars] = $1;
	modes[nvars] = $1;
	formats[nvars] = $NF;
	for (i = 4; i < NF; i++)
		types[nvars] = sprintf("%s %s", types[nvars], $i);

	if ($1 == "ARG")
		sizes[nvars] = sprintf("sizeof(%s)", $2);
	else if ($1 == "POINTER")
		sizes[nvars] = sprintf("sizeof(*%s)", $2);
	else { # DBT
		sizes[nvars] = \
		    sprintf("sizeof(u_int32_t) + (%s == NULL ? 0 : %s->size)", \
		    $2, $2);
		is_dbt = 1;
	}
	nvars++;
}
/^[ 	]*END/ {
	if (!in_begin) {
		print "Invalid format: missing BEGIN statement"
		next;
	}

	# Make the entire H file conditional
	# Define the record type in the H file
	printf("\n#define\tDB_%s\t(DB_%s_BEGIN + %d)\n\n", \
	    funcname, prefix, structs) >> HFILE

	# structure declarations
	printf("typedef struct _%s_args {\n", funcname) >> HFILE

	# Here are the required fields for every structure
	printf("\tu_int32_t type;\n\tDB_TXN *txnid;\n") >> HFILE
	printf("\tDB_LSN prev_lsn;\n") >>HFILE

	# Here are the specified fields.
	for (i = 0; i < nvars; i++) {
		t = types[i];
		if (modes[i] == "POINTER") {
			ndx = index(t, "*");
			t = substr(types[i], 0, ndx - 1);
		}
		printf("\t%s\t%s;\n", t, vars[i]) >> HFILE
	}
	printf("} __%s_args;\n\n", funcname) >> HFILE

# Now write the log function out to the cfile
	# PUBLIC declaration
	printf("/*\n * PUBLIC: ") >> CFILE;
	printf("int __%s_log\n * PUBLIC:     __P((", funcname) >> CFILE;
	printf("DB_LOG *, DB_TXN *, DB_LSN *, u_int32_t") >> CFILE;
	for (i = 0; i < nvars; i++) {
		printf(",") >> CFILE;
		if ((i % 4) == 0)
			printf("\n * PUBLIC:     ") >> CFILE;
		else
			printf(" ") >> CFILE;
		if (modes[i] == "DBT")
			printf("const ") >> CFILE;
		printf("%s", types[i]) >> CFILE;
		if (modes[i] == "DBT")
			printf(" *") >> CFILE;
	}
	printf("));\n */\n") >> CFILE;

	# Function declaration
	printf("int __%s_log(logp, txnid, ret_lsnp, flags", funcname) >> CFILE;
	for (i = 0; i < nvars; i++) {
		printf(",") >> CFILE;
		if ((i % 6) == 0)
			printf("\n\t") >> CFILE;
		else
			printf(" ") >> CFILE;
		printf("%s", vars[i]) >> CFILE;
	}
	printf(")\n") >> CFILE;

	# Now print the parameters
	printf("\tDB_LOG *logp;\n") >> CFILE;
	printf("\tDB_TXN *txnid;\n\tDB_LSN *ret_lsnp;\n") >> CFILE;
	printf("\tu_int32_t flags;\n") >> CFILE;
	for (i = 0; i < nvars; i++) {
		if (modes[i] == "DBT")
			printf("\tconst %s *%s;\n", types[i], vars[i]) >> CFILE;
		else
			printf("\t%s %s;\n", types[i], vars[i]) >> CFILE;
	}

	# Function body and local decls
	printf("{\n") >> CFILE;
	printf("\tDBT logrec;\n") >> CFILE;
	printf("\tDB_LSN *lsnp, null_lsn;\n") >> CFILE;
	if (is_dbt == 1)
		printf("\tu_int32_t zero;\n") >> CFILE;
	printf("\tu_int32_t rectype, txn_num;\n") >> CFILE;
	printf("\tint ret;\n") >> CFILE;
	printf("\tu_int8_t *bp;\n\n") >> CFILE;

	# Initialization
	printf("\trectype = DB_%s;\n", funcname) >> CFILE;
	printf("\ttxn_num = txnid == NULL ? 0 : txnid->txnid;\n") >> CFILE;
	printf("\tif (txnid == NULL) {\n") >> CFILE;
	printf("\t\tnull_lsn.file = 0;\n\t\tnull_lsn.offset = 0;\n") >> CFILE;
	printf("\t\tlsnp = &null_lsn;\n") >> CFILE;
	printf("\t} else\n\t\tlsnp = &txnid->last_lsn;\n") >> CFILE;

	# MALLOC
	printf("\tlogrec.size = sizeof(rectype) + ") >> CFILE;
	printf("sizeof(txn_num) + sizeof(DB_LSN)") >> CFILE;
	for (i = 0; i < nvars; i++)
		printf("\n\t    + %s", sizes[i]) >> CFILE;
	printf(";\n\tif ((logrec.data = (void *)") >> CFILE;
	printf("__db_malloc(logrec.size)) == NULL)\n") >> CFILE;
	printf("\t\treturn (ENOMEM);\n\n") >> CFILE;

	# Copy args into buffer

	printf("\tbp = logrec.data;\n") >> CFILE;
	printf("\tmemcpy(bp, &rectype, sizeof(rectype));\n") >> CFILE;
	printf("\tbp += sizeof(rectype);\n") >> CFILE;
	printf("\tmemcpy(bp, &txn_num, sizeof(txn_num));\n") >> CFILE;
	printf("\tbp += sizeof(txn_num);\n") >> CFILE;
	printf("\tmemcpy(bp, lsnp, sizeof(DB_LSN));\n") >> CFILE;
	printf("\tbp += sizeof(DB_LSN);\n") >> CFILE;

	for (i = 0; i < nvars; i ++) {
		if (modes[i] == "ARG") {
			printf("\tmemcpy(bp, &%s, %s);\n", \
			    vars[i], sizes[i]) >> CFILE;
			printf("\tbp += %s;\n", sizes[i]) >> CFILE;
		} else if (modes[i] == "DBT") {
			printf("\tif (%s == NULL) {\n", vars[i]) >> CFILE;
			printf("\t\tzero = 0;\n") >> CFILE;
			printf("\t\tmemcpy(bp, &zero, sizeof(u_int32_t));\n") \
				>> CFILE;
			printf("\t\tbp += sizeof(u_int32_t);\n") >> CFILE;
			printf("\t} else {\n") >> CFILE;
			printf("\t\tmemcpy(bp, &%s->size, ", vars[i]) >> CFILE;
			printf("sizeof(%s->size));\n", vars[i]) >> CFILE;
			printf("\t\tbp += sizeof(%s->size);\n", vars[i]) \
			    >> CFILE;
			printf("\t\tmemcpy(bp, %s->data, %s->size);\n", \
			    vars[i], vars[i]) >> CFILE;
			printf("\t\tbp += %s->size;\n\t}\n", vars[i]) >> CFILE;
		} else { # POINTER
			printf("\tif (%s != NULL)\n", vars[i]) >> CFILE;
			printf("\t\tmemcpy(bp, %s, %s);\n", vars[i], \
			    sizes[i]) >> CFILE;
			printf("\telse\n") >> CFILE;
			printf("\t\tmemset(bp, 0, %s);\n", sizes[i]) >> CFILE;
			printf("\tbp += %s;\n", sizes[i]) >> CFILE;
		}
	}

	# Error checking
	printf("#ifdef DIAGNOSTIC\n") >> CFILE;
	printf("\tif ((u_int32_t)(bp - (u_int8_t *)logrec.data) != logrec.size)") >> CFILE;
	printf("\n\t\tfprintf(stderr, \"%s\");\n", \
	    "Error in log record length") >> CFILE;
	printf("#endif\n") >> CFILE;

	# Issue log call
	# The logging system cannot call the public log_put routine
	# due to mutual exclusion constraints.  So, if we are
	# generating code for the log subsystem, use the internal
	# __log_put.
	if (prefix == "log")
		printf("\tret = __log_put\(logp, ret_lsnp, ") >> CFILE;
	else
		printf("\tret = log_put(logp, ret_lsnp, ") >> CFILE;
	printf("(DBT *)&logrec, flags);\n") >> CFILE;

	# Update the transactions last_lsn
	printf("\tif (txnid != NULL)\n") >> CFILE;
	printf("\t\ttxnid->last_lsn = *ret_lsnp;\n") >> CFILE;

	# Free and return
	printf("\t__db_free(logrec.data);\n") >> CFILE;
	printf("\treturn (ret);\n}\n\n") >> CFILE;

# Write the print function
	# PUBLIC declaration
	printf("/*\n * PUBLIC: int __%s_print\n * PUBLIC:", funcname) >> CFILE;
	printf("    __P((DB_LOG *, DBT *, DB_LSN *, int, void *));\n") >> CFILE;
	printf(" */\n") >> CFILE;

	# Function declaration
	printf("int\n__%s_print(notused1, ", funcname) >> CFILE;
	printf("dbtp, lsnp, notused2, notused3)\n") >> CFILE;
	printf("\tDB_LOG *notused1;\n\tDBT *dbtp;\n") >> CFILE;
	printf("\tDB_LSN *lsnp;\n") >> CFILE;
	printf("\tint notused2;\n\tvoid *notused3;\n{\n") >> CFILE;

	# Locals
	printf("\t__%s_args *argp;\n", funcname) >> CFILE;
	printf("\tu_int32_t i;\n\tu_int ch;\n\tint ret;\n\n") >> CFILE;

	# Get rid of complaints about unused parameters.
	printf("\ti = 0;\n\tch = 0;\n") >> CFILE;
	printf("\tnotused1 = NULL;\n") >> CFILE;
	printf("\tnotused2 = 0;\n\tnotused3 = NULL;\n\n") >> CFILE;

	# Call read routine to initialize structure
	printf("\tif ((ret = __%s_read(dbtp->data, &argp)) != 0)\n", \
	    funcname) >> CFILE;
	printf("\t\treturn (ret);\n") >> CFILE;

	# Print values in every record
	printf("\tprintf(\"[%%lu][%%lu]%s: ", funcname) >> CFILE;
	printf("rec: %%lu txnid %%lx ") >> CFILE;
	printf("prevlsn [%%lu][%%lu]\\n\",\n") >> CFILE;
	printf("\t    (u_long)lsnp->file,\n") >> CFILE;
	printf("\t    (u_long)lsnp->offset,\n") >> CFILE;
	printf("\t    (u_long)argp->type,\n") >> CFILE;
	printf("\t    (u_long)argp->txnid->txnid,\n") >> CFILE;
	printf("\t    (u_long)argp->prev_lsn.file,\n") >> CFILE;
	printf("\t    (u_long)argp->prev_lsn.offset);\n") >> CFILE;

	# Now print fields of argp
	for (i = 0; i < nvars; i ++) {
		printf("\tprintf(\"\\t%s: ", vars[i]) >> CFILE;

		if (modes[i] == "DBT") {
			printf("\");\n") >> CFILE;
			printf("\tfor (i = 0; i < ") >> CFILE;
			printf("argp->%s.size; i++) {\n", vars[i]) >> CFILE;
			printf("\t\tch = ((u_int8_t *)argp->%s.data)[i];\n", \
			    vars[i]) >> CFILE;
			printf("\t\tif (isprint(ch) || ch == 0xa)\n") >> CFILE;
			printf("\t\t\tputchar(ch);\n") >> CFILE;
			printf("\t\telse\n") >> CFILE;
			printf("\t\t\tprintf(\"%%#x \", ch);\n") >> CFILE;
			printf("\t}\n\tprintf(\"\\n\");\n") >> CFILE;
		} else if (types[i] == "DB_LSN *") {
			printf("[%%%s][%%%s]\\n\",\n", \
			    formats[i], formats[i]) >> CFILE;
			printf("\t    (u_long)argp->%s.file,", \
			    vars[i]) >> CFILE;
			printf(" (u_long)argp->%s.offset);\n", \
			    vars[i]) >> CFILE;
		} else {
			if (formats[i] == "lx")
				printf("0x") >> CFILE;
			printf("%%%s\\n\", ", formats[i]) >> CFILE;
			if (formats[i] == "lx" || formats[i] == "lu")
				printf("(u_long)") >> CFILE;
			if (formats[i] == "ld")
				printf("(long)") >> CFILE;
			printf("argp->%s);\n", vars[i]) >> CFILE;
		}
	}
	printf("\tprintf(\"\\n\");\n") >> CFILE;
	printf("\t__db_free(argp);\n") >> CFILE;
	printf("\treturn (0);\n") >> CFILE;
	printf("}\n\n") >> CFILE;

# Now write the read function out to the cfile
	printf("/*\n * PUBLIC: int __%s_read __P((void *, ", funcname) >> CFILE;
	printf("__%s_args **));\n */\n", funcname) >> CFILE;

	# Function declaration
	printf("int\n__%s_read(recbuf, argpp)\n", funcname) >> CFILE;

	# Now print the parameters
	printf("\tvoid *recbuf;\n") >> CFILE;
	printf("\t__%s_args **argpp;\n", funcname) >> CFILE;

	# Function body and local decls
	printf("{\n\t__%s_args *argp;\n", funcname) >> CFILE;
	printf("\tu_int8_t *bp;\n") >> CFILE;

	printf("\n\targp = (__%s_args *)__db_malloc(sizeof(", \
	    funcname) >> CFILE;
	printf("__%s_args) +\n\t    sizeof(DB_TXN));\n", funcname) >> CFILE;
	printf("\tif (argp == NULL)\n\t\treturn (ENOMEM);\n") >> CFILE;

	# Set up the pointers to the txnid and the prev lsn
	printf("\targp->txnid = (DB_TXN *)&argp[1];\n") >> CFILE;

	# First get the record type, prev_lsn, and txnid fields.

	printf("\tbp = recbuf;\n") >> CFILE;
	printf("\tmemcpy(&argp->type, bp, sizeof(argp->type));\n") >> CFILE;
	printf("\tbp += sizeof(argp->type);\n") >> CFILE;
	printf("\tmemcpy(&argp->txnid->txnid,  bp, ") >> CFILE;
	printf("sizeof(argp->txnid->txnid));\n") >> CFILE;
	printf("\tbp += sizeof(argp->txnid->txnid);\n") >> CFILE;
	printf("\tmemcpy(&argp->prev_lsn, bp, sizeof(DB_LSN));\n") >> CFILE;
	printf("\tbp += sizeof(DB_LSN);\n") >> CFILE;

	# Now get rest of data.

	for (i = 0; i < nvars; i ++) {
		if (modes[i] == "DBT") {
			printf("\tmemcpy(&argp->%s.size, ", vars[i]) >> CFILE;
			printf("bp, sizeof(u_int32_t));\n") >> CFILE;
			printf("\tbp += sizeof(u_int32_t);\n") >> CFILE;
			printf("\targp->%s.data = bp;\n", vars[i]) >> CFILE;
			printf("\tbp += argp->%s.size;\n", vars[i]) >> CFILE;
		} else if (modes[i] == "ARG") {
			printf("\tmemcpy(&argp->%s, bp, %s%s));\n", \
			    vars[i], "sizeof(argp->", vars[i]) >> CFILE;
			printf("\tbp += sizeof(argp->%s);\n", vars[i]) >> CFILE;
		} else { # POINTER
			printf("\tmemcpy(&argp->%s, bp, ", vars[i]) >> CFILE;
			printf(" sizeof(argp->%s));\n", vars[i]) >> CFILE;
			printf("\tbp += sizeof(argp->%s);\n", vars[i]) >> CFILE;
		}
	}

	# Free and return
	printf("\t*argpp = argp;\n") >> CFILE;
	printf("\treturn (0);\n}\n\n") >> CFILE;

# Recovery template

	cmd = sprintf("sed -e s/PREF/%s/ -e s/FUNC/%s/ < \
	    template/rec_ctemp >> %s", prefix, thisfunc, RFILE)
	system(cmd);

	# Done writing stuff, reset and continue.
	in_begin = 0;
}
END {
	if (error || in_begin)
		print "Unsuccessful"

# Print initialization routine
	# Write PUBLIC declaration
	printf("/*\n * PUBLIC: ") >> CFILE;
	printf("int __%s_init_print __P((DB_ENV *));\n", prefix) >> CFILE;
	printf(" */\n") >> CFILE;

	# Create the routine to call db_add_recovery(print_fn, id)
	printf("int\n__%s_init_print(dbenv)\n", prefix) >> CFILE;
	printf("\tDB_ENV *dbenv;\n{\n\tint ret;\n\n") >> CFILE;
	for (i = 0; i < num_funcs; i++) {
		printf("\tif ((ret = __db_add_recovery(dbenv,\n") >> CFILE;
		printf("\t    __%s_print, DB_%s)) != 0)\n", \
		    funcs[i], funcs[i]) >> CFILE;
		printf("\t\treturn (ret);\n") >> CFILE;
	}
	printf("\treturn (0);\n}\n\n") >> CFILE;

# Recover initialization routine
	printf("/*\n * PUBLIC: ") >> CFILE;
	printf("int __%s_init_recover __P((DB_ENV *));\n", prefix) >> CFILE;
	printf(" */\n") >> CFILE;

	# Create the routine to call db_add_recovery(func, id)
	printf("int\n__%s_init_recover(dbenv)\n", prefix) >> CFILE;
	printf("\tDB_ENV *dbenv;\n{\n\tint ret;\n\n") >> CFILE;
	for (i = 0; i < num_funcs; i++) {
		printf("\tif ((ret = __db_add_recovery(dbenv,\n") >> CFILE;
		printf("\t    __%s_recover, DB_%s)) != 0)\n", \
		    funcs[i], funcs[i]) >> CFILE;
		printf("\t\treturn (ret);\n") >> CFILE;
	}
	printf("\treturn (0);\n}\n\n") >> CFILE;

	# End the conditional for the HFILE
	printf("#endif\n") >> HFILE;

}
' CFILE=$cfile DIR=$dname HFILE=$hfile RFILE=$rfile < $ifile
