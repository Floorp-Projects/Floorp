/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)utils.c	10.46 (Sleepycat) 5/31/98";
#endif /* not lint */

/*
 * This file is divided up into 4 sets of functions:
 * 1. The dbopen command and its support functions.
 * 2. The dbwidget and dbcursor commands.
 * 3. The db support functions (e.g. get, put, del)
 * 4. The cursor support functions (e.g. get put, del)
 */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#endif
#include <tcl.h>

#include "db_int.h"
#include "db_page.h"
#include "hash.h"
#include "dbtest.h"
#include "test_ext.h"

/* Internal functions */
int db_del_cmd __P((Tcl_Interp *interp, int argc, char *argv[], DB *dbp));
int db_get_cmd __P((Tcl_Interp *interp, int argc, char *argv[], DB *dbp));
int db_getbin_cmd __P((Tcl_Interp *interp, int argc, char *argv[], DB *dbp));
int db_put_cmd __P((Tcl_Interp *interp, int argc, char *argv[], DB *dbp));
int db_putbin_cmd __P((Tcl_Interp *interp, int argc, char *argv[], DB *dbp));
int dbc_del_cmd __P((Tcl_Interp *interp, int argc, char *argv[], DBC *dbc));
int dbc_get_cmd __P((Tcl_Interp *interp, int argc, char *argv[], DBC *dbc));
int dbc_getbin_cmd __P((Tcl_Interp *interp, int argc, char *argv[], DBC *dbc));
int dbc_put_cmd __P((Tcl_Interp *interp, int argc, char *argv[], DBC *dbc));
int dbt_from_file __P((Tcl_Interp *interp, char *file, DBT *dbt));
int dbt_to_file __P((Tcl_Interp *interp, char *file, DBT *dbt));
u_int8_t *list_to_numarray __P((Tcl_Interp *, char *));
void set_get_result __P((Tcl_Interp *interp, DBT *dbt));

/*
 * dbopen_cmd --
 *	Implements dbopen for dbtest.  Dbopen creates a widget that
 * implements all the commands found off the DB structure.
 */

#define DBOPEN_USAGE "dbopen file flags mode type [options]\n\toptions:\n\t"

int
dbopen_cmd(notused, interp, argc, argv)
	ClientData notused;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	static int db_number = 0;
	static struct {
		const char *str;
		DBTYPE type;
	} list[] = {
		{"DB_UNKNOWN",	DB_UNKNOWN},
		{"db_unknown",	DB_UNKNOWN},
		{"UNKNOWN",	DB_UNKNOWN},
		{"unknown",	DB_UNKNOWN},
		{"DB_BTREE",	DB_BTREE},
		{"db_btree",	DB_BTREE},
		{"BTREE",	DB_BTREE},
		{"btree",	DB_BTREE},
		{"DB_HASH",	DB_HASH},
		{"db_hash",	DB_HASH},
		{"HASH",	DB_HASH},
		{"hash",	DB_HASH},
		{"DB_RECNO",	DB_RECNO},
		{"db_recno",	DB_RECNO},
		{"RECNO",	DB_RECNO},
		{"recno",	DB_RECNO},
		{ 0 }
	}, *lp;
	DB *dbp;
	DBTYPE type;
	DB_ENV *env;
	DB_INFO *openinfo;
	u_int32_t flags;
	int mode, ret, tclint;
	char dbname[50], *name;

	notused = NULL;

	/* Check number of arguments. */
	USAGE_GE(argc, 5, DBOPEN_USAGE, DO_INFO);

	/* Check flags and mode. */
	if (Tcl_GetInt(interp, argv[2], &tclint) != TCL_OK)
		goto usage;
	flags = (u_int32_t)tclint;
	if (Tcl_GetInt(interp, argv[3], &mode) != TCL_OK) {
usage:		Tcl_AppendResult(interp,
		    "\nUsage: ", DBOPEN_USAGE, DB_INFO_FLAGS, NULL);
		return (TCL_OK);
	}

	name = strcmp(argv[1], "NULL") == 0 ? NULL : argv[1];

	process_am_options(interp, argc - 5, &argv[5], &openinfo);
	process_env_options(interp, argc - 5, &argv[5], &env);

	/* Figure out type. */
	COMPQUIET(type, DB_HASH);
	for (lp = list; lp->str != NULL; ++lp)
		if (strcmp(argv[4], lp->str) == 0) {
			type = lp->type;
			break;
		}
	if (lp->str == NULL) {
		Tcl_SetResult(interp, "Invalid type", TCL_STATIC);
		return (TCL_ERROR);
	}

	if (openinfo == NULL && type != DB_UNKNOWN) {
		Tcl_AppendResult(interp, "Usage: ", DBOPEN_USAGE,
		    DB_INFO_FLAGS, NULL);
		return (TCL_ERROR);
	}

	debug_check();

	/* Call dbopen. */
	ret = db_open(name, type, flags, mode, env, openinfo, &dbp);
	if (openinfo && openinfo->re_source)
		free(openinfo->re_source);
	if (openinfo)
		free(openinfo);
	if (ret != 0) {
		Tcl_SetResult(interp, "dbopen: ", TCL_STATIC);
		errno = ret;
		Tcl_AppendResult(interp, Tcl_PosixError(interp), NULL);
		return (TCL_OK);
	}

	/* Create widget command. */
	/* Create new command name. */
	sprintf(&dbname[0], "db%d", db_number);
	db_number++;

	Tcl_CreateCommand(interp, dbname, dbwidget_cmd, (ClientData)dbp, NULL);
	Tcl_SetResult(interp, dbname, TCL_VOLATILE);
	return (TCL_OK);
}

/*
 * process_am_options --
 *	Read the options in the command line and create a DB_INFO structure
 *	to pass to db_open.
 */
int
process_am_options(interp, argc, argv, openinfo)
	Tcl_Interp *interp;
	int argc;
	char *argv[];
	DB_INFO **openinfo;
{
	DB_INFO *oi;
	int err, tclint;
	char *option;

	COMPQUIET(option, NULL);

	err = TCL_OK;
	oi = (DB_INFO *)calloc(sizeof(DB_INFO), 1);

	while (argc > 1) {
		if (**argv != '-') {		/* Make sure it's an option */
			argc--;
			argv++;
			continue;
		}
		/* Set option to first character after "-" */
		option = argv[0];
		option++;

		if (strcmp(option, "flags") == 0) {
	/* Contains flags for all access methods */
	    		if ((err =
	    		    Tcl_GetInt(interp, argv[1], &tclint)) != TCL_OK)
				break;
			oi->flags = (u_int32_t)tclint;
		} else if (strcmp(option, "psize") == 0) {
	    		if ((err =
	    		    Tcl_GetInt(interp, argv[1], &tclint)) != TCL_OK)
				break;
			oi->db_pagesize = (size_t)tclint;
		} else if (strcmp(option, "order") == 0) {
	    		if ((err =
	    		    Tcl_GetInt(interp, argv[1], &tclint)) != TCL_OK)
				break;
			oi->db_lorder = (int)tclint;
		} else if (strcmp(option, "cachesize") == 0) {
	    		if ((err =
	    		    Tcl_GetInt(interp, argv[1], &tclint)) != TCL_OK)
				break;
			oi->db_cachesize = (size_t)tclint;
		} else if (strcmp(option, "minkey") == 0) {
	/* Btree flags */
	    		if ((err =
	    		    Tcl_GetInt(interp, argv[1], &tclint)) != TCL_OK)
				break;
			oi->bt_minkey = (u_int32_t)tclint;
		} else if (strcmp(option, "compare") == 0) {
			/* Not sure how to handle this. */
			err = TCL_ERROR;
		} else if (strcmp(option, "prefix") == 0) {
			/* Not sure how to handle this. */
			err = TCL_ERROR;
		} else if (strcmp(option, "ffactor") == 0) {
	/* Hash flags */
	    		if ((err =
	    		    Tcl_GetInt(interp, argv[1], &tclint)) != TCL_OK)
				break;
			oi->h_ffactor = (u_int32_t)tclint;
		} else if (strcmp(option, "nelem") == 0) {
	    		if ((err =
	    		    Tcl_GetInt(interp, argv[1], &tclint)) != TCL_OK)
				break;
			oi->h_nelem = (u_int32_t)tclint;
		} else if (strcmp(option, "hash") == 0) {
	    		if ((err =
	    		    Tcl_GetInt(interp, argv[1], &tclint)) != TCL_OK)
				break;
			switch (tclint) {
				case 2: oi->h_hash = __ham_func2; break;
				case 3: oi->h_hash = __ham_func3; break;
				case 4: oi->h_hash = __ham_func4; break;
				case 5: oi->h_hash = __ham_func5; break;
			}
			if (oi->h_hash == NULL) {
				err = TCL_ERROR;
				break;
			}
		} else if (strcmp(option, "recdelim") == 0) {
	/* Recno flags */
	    		if ((err =
	    		    Tcl_GetInt(interp, argv[1], &tclint)) != TCL_OK)
				break;
			oi->re_delim = (int)tclint;
		} else if (strcmp(option, "recpad") == 0) {
	    		if ((err =
	    		    Tcl_GetInt(interp, argv[1], &tclint)) != TCL_OK)
				break;
			oi->re_pad = (int)tclint;
		} else if (strcmp(option, "reclen") == 0) {
	    		if ((err =
	    		    Tcl_GetInt(interp, argv[1], &tclint)) != TCL_OK)
				break;
			oi->re_len = (u_int32_t)tclint;
		} else if (strcmp(option, "recsrc") == 0)
			oi->re_source = (char *)strdup(argv[1]);

		argc -= 2;
		argv += 2;
	}
	if (err != TCL_OK) {
		Tcl_AppendResult(interp,
		    "\nInvalid ", option, " value: ", argv[1], "\n", NULL);
		free(oi);
		oi = NULL;
	}
	*openinfo = oi;
	return (oi ? 0 : 1);
}

/*
 * process_env_options --
 *	Read the options in the command line and create a DB_ENV structure
 *	to pass to db_open.
 */
int
process_env_options(interp, argc, argv, envinfo)
	Tcl_Interp *interp;
	int argc;
	char *argv[];
	DB_ENV **envinfo;
{
	DB_ENV *env;
	Tcl_CmdInfo info;
	u_int32_t flags;
	int err, nconf, tclint;
	char *option, *db_home, **config;

	COMPQUIET(option, NULL);
	err = TCL_OK;
	flags = 0;
	db_home = Tcl_GetVar(interp, "testdir", 0);
	config = NULL;

	env = (DB_ENV *)calloc(sizeof(DB_ENV), 1);
	while (argc > 1) {
		if (**argv != '-') {		/* Make sure it's an option */
			argc--;
			argv++;
			continue;
		}
		/* Set option to first character after "-" */
		option = argv[0];
		option++;

		if (strcmp(option, "dbenv") == 0) {
	/* environment already set up. */
			if (Tcl_GetCommandInfo(interp, argv[1], &info) == 0) {
				Tcl_SetResult(interp,
				    "Invalid environment: ", TCL_STATIC);
				Tcl_AppendResult(interp, argv[1], 0);
				return (TCL_ERROR);
			}
			free(env);
			*envinfo = (DB_ENV *)info.clientData;
			return (TCL_OK);
		} else if (strcmp(option, "dbflags") == 0) {
	/* db_appinit parameters */
	    		if ((err =
	    		    Tcl_GetInt(interp, argv[1], &tclint)) != TCL_OK)
				break;
			flags = (u_int32_t)tclint;
			/*
			 * Don't specify DB_THREAD if the architecture can't
			 * do spinlocks.
			 */
#ifndef HAVE_SPINLOCKS
			LF_CLR(DB_THREAD);
#endif
		} else if (strcmp(option, "dbhome") == 0) {
			db_home = argv[1];
		} else if (strcmp(option, "dbconfig") == 0) {
			err = Tcl_SplitList(interp, argv[1], &nconf, &config);
			if (err)
				break;
		} else if (strcmp(option, "maxlocks") == 0) {
	/* Lock flags */
	    		if ((err =
	    		    Tcl_GetInt(interp, argv[1], &tclint)) != TCL_OK)
				break;
			env->lk_max = (u_int32_t)tclint;
		} else if (strcmp(option, "nmodes") == 0) {
	    		if ((err =
	    		    Tcl_GetInt(interp, argv[1], &tclint)) != TCL_OK)
				break;
			env->lk_modes = (u_int32_t)tclint;
		} else if (strcmp(option, "detect") == 0) {
	    		if ((err =
	    		    Tcl_GetInt(interp, argv[1], &tclint)) != TCL_OK)
				break;
			env->lk_detect = (u_int32_t)tclint;
		} else if (strcmp(option, "conflicts") == 0) {
			env->lk_conflicts = list_to_numarray(interp, argv[1]);
			if (env->lk_conflicts == NULL)
				break;
		} else if (strcmp(option, "maxsize") == 0) {
	/* Log flags */
	    		if ((err =
	    		    Tcl_GetInt(interp, argv[1], &tclint)) != TCL_OK)
				break;
			env->lg_max = (u_int32_t)tclint;
		} else if (strcmp(option, "cachesize") == 0) {
	/* Mpool flags */
	    		if ((err =
	    		    Tcl_GetInt(interp, argv[1], &tclint)) != TCL_OK)
				break;
			env->mp_size = (size_t)tclint;
		} else if (strcmp(option, "maxtxns") == 0) {
	/* Txn flags */
	    		if ((err =
	    		    Tcl_GetInt(interp, argv[1], &tclint)) != TCL_OK)
				break;
			env->tx_max = (u_int32_t)tclint;
		} else if (strcmp(option, "rinit") == 0) {
	/* Region init */
			if ((err = db_value_set(1, DB_REGION_INIT)) != 0) {
				if (err == EINVAL) {
					Tcl_SetResult(interp,
					    "EINVAL", TCL_STATIC);
					return (TCL_ERROR);
				}
				Tcl_SetResult(interp,
				    "env/rinit:", TCL_STATIC);
				errno = err;
				Tcl_AppendResult(interp,
				    Tcl_PosixError(interp), 0);
				return (TCL_ERROR);
			}
		} else if (strcmp(option, "shmem") == 0) {
	/* Shared memory specification */
			if (strcmp(argv[1], "anon") == 0) {
				if ((err =
				    db_value_set(1, DB_REGION_ANON)) != 0) {
					if (err == EINVAL) {
						Tcl_SetResult(interp,
						    "EINVAL", TCL_STATIC);
						return (TCL_ERROR);
					}
					Tcl_SetResult(interp,
					    "env/shmem:", TCL_STATIC);
					errno = err;
					Tcl_AppendResult(interp,
					    Tcl_PosixError(interp), 0);
					return (TCL_ERROR);
				}
			} else if (strcmp(argv[1], "named") == 0) {
				if ((err =
				    db_value_set(1, DB_REGION_NAME)) != 0) {
					if (err == EINVAL) {
						Tcl_SetResult(interp,
						    "EINVAL", TCL_STATIC);
						return (TCL_ERROR);
					}
					Tcl_SetResult(interp,
					    "env/shmem:", TCL_STATIC);
					errno = err;
					Tcl_AppendResult(interp,
					    Tcl_PosixError(interp), 0);
					return (TCL_ERROR);
				}
			} else {
				Tcl_SetResult(interp,
				    "Invalid shmem option", TCL_STATIC);
				Tcl_AppendResult(interp, argv[1], NULL);
				return (TCL_OK);
			}
		}

		argc -= 2;
		argv += 2;
	}
	if (err != TCL_OK) {
		Tcl_AppendResult(interp, "\nInvalid ", option, " value: ",
		    argv[1], "\n", NULL);
		if (env->lk_conflicts)
			free(env->lk_conflicts);
		free(env);
		env = NULL;
	}

	/*
	 * Set up error stuff.
	 */

	if (env) {
		env->db_errfile = stderr;
		env->db_errpfx = "dbtest";
		env->db_errcall = NULL;
		env->db_verbose = 0;

		if ((errno = db_appinit(db_home, config, env, flags)) != 0) {
			if (config)
				free(config);
			if (env->lk_conflicts)
				free(env->lk_conflicts);
			free(env);
			env = NULL;
		}
	}

	*envinfo = env;
	return (env ? 0 : 1);
}

/*
 * dbwidget --
 *	Since dbopen creates a widget, we need a command that then
 * handles all the widget commands.  This is that command.  If we
 * ever add new "methods" we add new widget commands here.
 */
#define DBWIDGET_USAGE "dbN option ?arg arg ...?"
#define DBCLOSE_USAGE "dbN close"
#define DBCURS_USAGE "dbN cursor txn"
#define DBFD_USAGE "dbN fd"
#define DBSYNC_USAGE "dbN sync flags"
#define	DBLOCKER_USAGE "dbN locker"

int
dbwidget_cmd(cd_dbp, interp, argc, argv)
	ClientData cd_dbp;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	static int curs_id = 0;
	DB *dbp;
	DBC *cursor;
	DB_ENV *env;
	DB_TXN *txnid;
	Tcl_CmdInfo info;
	u_int32_t flags;
	int fd, ret, tclint;
	char cursname[128];

	dbp = (DB *)cd_dbp;

	USAGE_GE(argc, 2, DBWIDGET_USAGE, 0);

	Tcl_SetResult(interp, "0", TCL_STATIC);

	if (strcmp(argv[1], "close") == 0) {
		USAGE(argc, 2, DBCLOSE_USAGE, 0);
		env = dbp->dbenv;
		debug_check();
#ifdef STATISTICS
		if (dbp->stat != NULL)
			(void)dbp->stat(dbp, stdout);
#endif
		ret = dbp->close(dbp, 0);
		if (env && !F_ISSET(env, DB_ENV_STANDALONE)) {
			(void)db_appexit(env);
			if (env->lk_conflicts)
				free(env->lk_conflicts);
			free(env);
		}
		(void)Tcl_DeleteCommand(interp, argv[0]);
		if (ret < 0)
			Tcl_SetResult(interp, "1", TCL_STATIC);
		else if (ret > 0) {
			Tcl_SetResult(interp, "db_close:", TCL_STATIC);
			errno = ret;
			Tcl_AppendResult(interp, Tcl_PosixError(interp), 0);
		} else
			Tcl_SetResult(interp, "0", TCL_STATIC);
		return (TCL_OK);
	} else if (strcmp(argv[1], "cursor") == 0) {
		USAGE(argc, 3, DBCURS_USAGE, 0);
		sprintf(&cursname[0], "%s.cursor%d", argv[0], curs_id);
		curs_id++;
		if (argv[2][0] == '0' && argv[2][1] == '\0')
			txnid = NULL;
		else {
			if (Tcl_GetCommandInfo(interp, argv[2], &info) == 0) {
				Tcl_SetResult(interp,
				    "db_del: Invalid argument ", TCL_STATIC);
				Tcl_AppendResult(interp, argv[2],
				    " not a transaction.", 0);
				return (TCL_ERROR);
			}
			txnid = (DB_TXN *)(info.clientData);
		}
		debug_check();
		if ((ret = dbp->cursor(dbp, txnid, &cursor)) == 0) {
			Tcl_CreateCommand(interp, cursname, dbcursor_cmd,
			    (ClientData)cursor, NULL);
			Tcl_SetResult(interp, cursname, TCL_VOLATILE);
		} else {
			Tcl_SetResult(interp, "db_cursor:", TCL_STATIC);
			errno = ret;
			Tcl_AppendResult(interp, Tcl_PosixError(interp), 0);
		}
		return (TCL_OK);
	} else if (strcmp(argv[1], "del") == 0) {
		return (db_del_cmd(interp, argc, argv, dbp));
	} else if (strcmp(argv[1], "fd") == 0) {
		USAGE(argc, 2, DBFD_USAGE, 0);
		Tcl_ResetResult(interp);
		debug_check();
		(void)dbp->fd(dbp, &fd);
		sprintf(interp->result, "%d", fd);
		return (TCL_OK);
	} else if (strcmp(argv[1], "get") == 0) {
		return (db_get_cmd(interp, argc, argv, dbp));
	} else if (strcmp(argv[1], "getn") == 0) {
		return (db_get_cmd(interp, argc, argv, dbp));
	} else if (strcmp(argv[1], "getbin") == 0) {
		return (db_getbin_cmd(interp, argc, argv, dbp));
	} else if (strcmp(argv[1], "getbinkey") == 0) {
		return (db_getbin_cmd(interp, argc, argv, dbp));
	} else if (strcmp(argv[1], "locker") == 0) {
		USAGE(argc, 2, DBLOCKER_USAGE, 0);
		sprintf(cursname, "%lu", (u_long)dbp->locker);
		Tcl_SetResult(interp, cursname, TCL_VOLATILE);
		return (TCL_OK);
	} else if (strcmp(argv[1], "put") == 0) {
		return (db_put_cmd(interp, argc, argv, dbp));
	} else if (strcmp(argv[1], "putn") == 0) {
		return (db_put_cmd(interp, argc, argv, dbp));
	} else if (strcmp(argv[1], "putbin") == 0) {
		return (db_putbin_cmd(interp, argc, argv, dbp));
	} else if (strcmp(argv[1], "putbinkey") == 0) {
		return (db_putbin_cmd(interp, argc, argv, dbp));
	} else if (strcmp(argv[1], "put0") == 0) {
		return (db_put_cmd(interp, argc, argv, dbp));
	} else if (strcmp(argv[1], "sync") == 0) {
		USAGE(argc, 3, DBSYNC_USAGE, 0);
		if (Tcl_GetInt(interp, argv[2], &tclint) != TCL_OK)
			return (TCL_ERROR);
		flags = (u_int32_t)tclint;
		debug_check();
		if ((ret = dbp->sync(dbp, flags)) < 0)
			Tcl_SetResult(interp, "1", TCL_STATIC);
		else if (ret > 0) {
			Tcl_SetResult(interp, "db_sync:", TCL_STATIC);
			errno = ret;
			Tcl_AppendResult(interp, Tcl_PosixError(interp), 0);
		} else
			Tcl_SetResult(interp, "0", TCL_STATIC);
		return (TCL_OK);
	} else {
		Tcl_SetResult(interp, DBWIDGET_USAGE, TCL_STATIC);
		return (TCL_ERROR);
	}
}

#define DBCURSOR_USAGE "dbN.cursorM ?arg arg ...?"
#define DBCCLOSE_USAGE "dbN.cursorM close"
int
dbcursor_cmd(cd_dbc, interp, argc, argv)
	ClientData cd_dbc;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	DBC *dbc;

	dbc = (DBC *)cd_dbc;
	Tcl_SetResult(interp, "0", TCL_STATIC);

	if (strcmp(argv[1], "close") == 0) {
		USAGE(argc, 2, DBCCLOSE_USAGE, 0);
		debug_check();
		(void)dbc->c_close(dbc);
		return (Tcl_DeleteCommand(interp, argv[0]));
	} else if (strcmp(argv[1], "del") == 0) {
		return (dbc_del_cmd(interp, argc, argv, dbc));
	} else if (strcmp(argv[1], "get") == 0) {
		return (dbc_get_cmd(interp, argc, argv, dbc));
	} else if (strcmp(argv[1], "getn") == 0) {
		return (dbc_get_cmd(interp, argc, argv, dbc));
	} else if (strcmp(argv[1], "getbin") == 0) {
		return (dbc_getbin_cmd(interp, argc, argv, dbc));
	} else if (strcmp(argv[1], "getbinkey") == 0) {
		return (dbc_getbin_cmd(interp, argc, argv, dbc));
	} else if (strcmp(argv[1], "put") == 0) {
		return (dbc_put_cmd(interp, argc, argv, dbc));
	} else if (strcmp(argv[1], "putn") == 0) {
		return (dbc_put_cmd(interp, argc, argv, dbc));
	} else {
		Tcl_SetResult(interp, DBCURSOR_USAGE, TCL_STATIC);
		return (TCL_ERROR);
	}
	/* NOTREACHED */
}

/* Widget support commands. */

#define DBDEL_USAGE "dbN del txn key flags"
int
db_del_cmd(interp, argc, argv, dbp)
	Tcl_Interp *interp;
	int argc;
	char *argv[];
	DB *dbp;
{
	DBT key;
	DB_TXN *txnid;
	Tcl_CmdInfo info;
	db_recno_t rkey;
	u_int32_t flags;
	int ret, tclint;

	USAGE(argc, 5, DBDEL_USAGE, 0);

	if (Tcl_GetInt(interp, argv[4], &tclint) != TCL_OK) {
		Tcl_AppendResult(interp, "\n", DBDEL_USAGE, NULL);
		return (TCL_ERROR);
	}
	flags = (u_int32_t)tclint;

	memset(&key, 0, sizeof(key));
	if (dbp->type == DB_RECNO) {
		if (Tcl_GetInt(interp, argv[3], &tclint) != TCL_OK)
			return (TCL_ERROR);
		rkey = (db_recno_t)tclint;
		key.data = &rkey;
		key.size = sizeof(db_recno_t);
	} else {
		key.data = argv[3];
		key.size = strlen(argv[3]) + 1;		/* Add NULL on end. */
	}

	if (argv[2][0] == '0' && argv[2][1] == '\0')
		txnid = NULL;
	else {
		if (Tcl_GetCommandInfo(interp, argv[2], &info) == 0) {
			Tcl_SetResult(interp,
			    "db_del: Invalid argument ", TCL_STATIC);
			Tcl_AppendResult(interp, argv[2],
			    " not a transaction.", 0);
			return (TCL_ERROR);
		}
		txnid = (DB_TXN *)(info.clientData);
	}

	debug_check();

	if ((ret = dbp->del(dbp, txnid, &key, flags)) == 0)
		return (TCL_OK);
	else if (ret == DB_NOTFOUND) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Key ", argv[3], " not found.", NULL);
		return (TCL_OK);
	} else {
		Tcl_SetResult(interp, "-1", TCL_STATIC);
		return (TCL_OK);
	}
}

#define DBGET_USAGE "dbN get txn key flags [beg len]"
int
db_get_cmd(interp, argc, argv, dbp)
	Tcl_Interp *interp;
	int argc;
	char *argv[];
	DB *dbp;
{
	DBT data, key;
	DB_TXN *txnid;
	Tcl_CmdInfo info;
	db_recno_t ikey;
	u_int32_t flags;
	int ret, tclint;

	USAGE_GE(argc, 5, DBGET_USAGE, 0);

	if (Tcl_GetInt(interp, argv[4], &tclint) != TCL_OK) {
		Tcl_AppendResult(interp, "\n", DBGET_USAGE, NULL);
		return (TCL_ERROR);
	}
	flags = (u_int32_t)tclint;

	memset(&key, 0, sizeof(key));
	if (dbp->type == DB_RECNO || strcmp(argv[1], "getn") == 0) {
		if (Tcl_GetInt(interp, argv[3], &tclint) != TCL_OK)
			return (TCL_ERROR);
		ikey = (db_recno_t)tclint;
		key.data = &ikey;
		key.size = sizeof(db_recno_t);
	} else {
		key.data = argv[3];
		key.size = strlen(argv[3]) + 1;	/* Add Null on end */
	}

	memset(&data, 0, sizeof(data));
	if (flags == DB_DBT_PARTIAL) {
		USAGE(argc, 7, DBGET_USAGE, 0);
		if (Tcl_GetInt(interp, argv[5], &tclint) != TCL_OK)
			return (TCL_ERROR);
		data.doff = (size_t)tclint;
		if (Tcl_GetInt(interp, argv[6], &tclint) != TCL_OK)
			return (TCL_ERROR);
		data.dlen = (size_t)tclint;
		data.flags = DB_DBT_PARTIAL;
		flags = 0;
	}

	if (argv[2][0] == '0' && argv[2][1] == '\0')
		txnid = NULL;
	else {
		if (Tcl_GetCommandInfo(interp, argv[2], &info) == 0) {
			Tcl_SetResult(interp,
			    "db_get: Invalid argument ", TCL_STATIC);
			Tcl_AppendResult(interp, argv[2],
			    " not a transaction.", 0);
			return (TCL_ERROR);
		}
		txnid = (DB_TXN *)(info.clientData);
	}

	debug_check();

	if (F_ISSET(dbp, DB_AM_THREAD))
		F_SET(&data, DB_DBT_MALLOC);
	if ((ret = dbp->get(dbp, txnid, &key, &data, flags)) == 0) {
		Tcl_ResetResult(interp);
		set_get_result(interp, &data);
		if (F_ISSET(&data, DB_DBT_MALLOC))
			free(data.data);
		return (TCL_OK);
	} else if (ret == DB_NOTFOUND) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Key ", argv[3], " not found.", NULL);
		return (TCL_OK);
	} else {
		Tcl_SetResult(interp, "-1", TCL_STATIC);
		return (TCL_OK);
	}
}

/*
 * Used for getting and verifying the data associated with a binary
 * data field in the database.  These are big and tend to crash tcl.
 * We do the handling of the big stuff in C.  The path is the file
 * into which to write the large key/data element.
 */
#define DBGETBIN_USAGE "dbN getbin path txn key flags"
int
db_getbin_cmd(interp, argc, argv, dbp)
	Tcl_Interp *interp;
	int argc;
	char *argv[];
	DB *dbp;
{
	DBT data, key;
	DB_TXN *txnid;
	Tcl_CmdInfo info;
	db_recno_t rkey;
	u_int32_t flags;
	int ret, tclint;

	USAGE(argc, 6, DBGETBIN_USAGE, 0);

	if (Tcl_GetInt(interp, argv[5], &tclint) != TCL_OK) {
		Tcl_AppendResult(interp, "\n", DBGET_USAGE, NULL);
		return (TCL_ERROR);
	}
	flags = (u_int32_t)tclint;

	memset(&key, 0, sizeof(key));
	if (strcmp(argv[1], "getbinkey") == 0) {
		if (dbt_from_file(interp, argv[4], &key) != TCL_OK)
			return (TCL_ERROR);
	} else if (dbp->type == DB_RECNO) {
		if (Tcl_GetInt(interp, argv[4], &tclint) != TCL_OK)
			return (TCL_ERROR);
		rkey = (db_recno_t)tclint;
		key.data = &rkey;
		key.size = sizeof(db_recno_t);
	} else {
		key.data = argv[4];
		key.size = strlen(argv[4]) + 1;	/* Add Null on end */
	}

	memset(&data, 0, sizeof(data));
	if (strcmp(argv[1], "getbinkey") != 0)
		F_SET(&data, DB_DBT_MALLOC);

	if (argv[3][0] == '0' && argv[3][1] == '\0')
		txnid = NULL;
	else {
		if (Tcl_GetCommandInfo(interp, argv[3], &info) == 0) {
			Tcl_SetResult(interp,
			    "db_getbin: Invalid argument ", TCL_STATIC);
			Tcl_AppendResult(interp, argv[3],
			    " not a transaction.", 0);
			return (TCL_ERROR);
		}
		txnid = (DB_TXN *)(info.clientData);
	}

	debug_check();

	if ((ret = dbp->get(dbp, txnid, &key, &data, flags)) == DB_NOTFOUND) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Key ", argv[3], " not found.", NULL);
		return (TCL_OK);
	} else if (ret != 0) {
		Tcl_SetResult(interp, "-1", TCL_STATIC);
		return (TCL_OK);
	}

	/* Got item; write it to file. */
	if (strcmp(argv[1], "getbinkey") == 0) {
		free(key.data);
		Tcl_SetResult(interp, data.data, TCL_VOLATILE);
	} else {
		if (dbt_to_file(interp, argv[2], &data) != TCL_OK)
			return (TCL_ERROR);
		Tcl_SetResult(interp, argv[2], TCL_VOLATILE);
	}

	return (TCL_OK);
}

#define DBPUT_USAGE "dbN put txn key data flags"
#define DBPPUT_USAGE "dbN put txn key data flags doff dlen"
int
db_put_cmd(interp, argc, argv, dbp)
	Tcl_Interp *interp;
	int argc;
	char *argv[];
	DB *dbp;
{
	DBT data, key;
	DB_TXN *txnid;
	Tcl_CmdInfo info;
	db_recno_t ikey;
	u_int32_t flags;
	int ret, tclint;
	char numbuf[16];

	USAGE_GE(argc, 6, DBPUT_USAGE, 0);

	if (Tcl_GetInt(interp, argv[5], &tclint) != TCL_OK) {
		Tcl_AppendResult(interp, "\n", DBPUT_USAGE, NULL);
		return (TCL_ERROR);
	}
	flags = (u_int32_t)tclint;

	memset(&key, 0, sizeof(key));
	if (strcmp(argv[1], "putn") == 0 || dbp->type == DB_RECNO) {
		if (Tcl_GetInt(interp, argv[3], &tclint) != TCL_OK)
			return (TCL_ERROR);
		ikey = (db_recno_t)tclint;
		key.data = &ikey;
		key.size = sizeof(db_recno_t);
	} else {
		key.data = argv[3];
		key.size = strlen(argv[3]) + 1;	/* Add Null on end */
	}

	memset(&data, 0, sizeof(data));
	data.data = argv[4];
	data.size = strlen(argv[4]) + (strcmp(argv[1], "put0") == 0 ? 0 : 1);

	/*
	 * If the partial flag is set, then arguments 6 and 7 are the
	 * offset and length respectively.
	 */
	if (flags == DB_DBT_PARTIAL) {
		USAGE_GE(argc, 8, DBPPUT_USAGE, 0);
		if (Tcl_GetInt(interp, argv[6], &tclint) != TCL_OK)
			return (TCL_ERROR);
		data.doff = (size_t)tclint;
		if (Tcl_GetInt(interp, argv[7], &tclint) != TCL_OK)
			return (TCL_ERROR);
		data.dlen = (size_t)tclint;
		data.flags = DB_DBT_PARTIAL;
		data.size--;			/* Remove the NULL */
		flags = 0;
	}

	if (argv[2][0] == '0' && argv[2][1] == '\0')
		txnid = NULL;
	else {
		if (Tcl_GetCommandInfo(interp, argv[2], &info) == 0) {
			Tcl_SetResult(interp,
			    "db_put: Invalid argument ", TCL_STATIC);
			Tcl_AppendResult(interp, argv[2],
			    " not a transaction.", 0);
			return (TCL_ERROR);
		}
		txnid = (DB_TXN *)(info.clientData);
	}
	debug_check();

	if ((ret = dbp->put(dbp, txnid, &key, &data, flags)) == 0) {
		if (LF_ISSET(DB_APPEND)) {
			if (key.size != sizeof(db_recno_t)) {
				Tcl_SetResult(interp,
				    "Error: key not integer", TCL_STATIC);
				return (TCL_ERROR);
			}
			memcpy(&ikey, key.data, sizeof(db_recno_t));
			sprintf(numbuf, "%ld", (long)ikey);
			Tcl_SetResult(interp, numbuf, TCL_VOLATILE);
		} else {
			Tcl_SetResult(interp, "0", TCL_STATIC);
		}
	} else if (ret < 0) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Error on put of key ",
		    argv[3], " either not found or bad flag values.", NULL);
	} else {
		Tcl_SetResult(interp, "-1", TCL_STATIC);
	}
	return (TCL_OK);
}

/*
 * Used for putting very large data items into a db file.  Instead of
 * passing the data in a DB, we'll pass a file name and read the data
 * from the file and put it in the db.  This can be checked using the
 * getbin command.
 */
#define DBPUTBIN_USAGE "dbN put txn key data flags"
int
db_putbin_cmd(interp, argc, argv, dbp)
	Tcl_Interp *interp;
	int argc;
	char *argv[];
	DB *dbp;
{
	DBT data, key;
	DB_TXN *txnid;
	Tcl_CmdInfo info;
	db_recno_t rkey;
	u_int32_t flags;
	int ret, tclint;
	void *p;

	USAGE(argc, 6, DBPUTBIN_USAGE, 0);

	if (Tcl_GetInt(interp, argv[5], &tclint) != TCL_OK) {
		Tcl_AppendResult(interp, "\n", DBPUT_USAGE, NULL);
		return (TCL_ERROR);
	}
	flags = (u_int32_t)tclint;

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	if (strcmp(argv[1], "putbinkey") == 0) {
		/* Get data from file. */
		if (dbt_from_file(interp, argv[3], &key) != TCL_OK)
			return (TCL_ERROR);
		data.data = argv[4];
		data.size = strlen(argv[4]) + 1;	/* Add Null on end */
		p = key.data;
	} else {
		if (dbt_from_file(interp, argv[4], &data) != TCL_OK)
			return (TCL_ERROR);
		p = data.data;

		if (dbp->type == DB_RECNO) {
			if (Tcl_GetInt(interp, argv[3], &tclint) != TCL_OK)
				return (TCL_ERROR);
			rkey = (db_recno_t)tclint;
			key.data = &rkey;
			key.size = sizeof(db_recno_t);
		} else {
			key.data = argv[3];
			key.size = strlen(argv[3]) + 1;	/* Add Null on end */
		}
	}

	if (argv[2][0] == '0' && argv[2][1] == '\0')
		txnid = NULL;
	else {
		if (Tcl_GetCommandInfo(interp, argv[2], &info) == 0) {
			Tcl_SetResult(interp,
			    "db_putbin: Invalid argument ", TCL_STATIC);
			Tcl_AppendResult(interp, argv[2],
			    " not a transaction.", 0);
			return (TCL_ERROR);
		}
		txnid = (DB_TXN *)(info.clientData);
	}
	debug_check();

	ret = dbp->put(dbp, txnid, &key, &data, flags);
	free(p);

	if (ret == 0)
		return (TCL_OK);
	else if (ret < 0) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Key ", argv[3], " not found.", NULL);
		return (TCL_OK);
	} else {
		Tcl_SetResult(interp, "-1", TCL_STATIC);
		return (TCL_OK);
	}
}

/* Cursor support commands. */

#define DBCDEL_USAGE "cursorN del flags"
int
dbc_del_cmd(interp, argc, argv, dbc)
	Tcl_Interp *interp;
	int argc;
	char *argv[];
	DBC *dbc;
{
	u_int32_t flags;
	int ret, tclint;

	USAGE(argc, 3, DBCDEL_USAGE, 0);

	if (Tcl_GetInt(interp, argv[2], &tclint) != TCL_OK) {
		Tcl_AppendResult(interp, "\n", DBCDEL_USAGE, NULL);
		return (TCL_ERROR);
	}
	flags = (u_int32_t)tclint;

	debug_check();

	if ((ret = dbc->c_del(dbc, flags)) == 0)
		return (TCL_OK);
	else if (ret < 0) {
		Tcl_SetResult(interp, "Uninitialized Cursor", TCL_STATIC);
		return (TCL_OK);
	} else {
		Tcl_SetResult(interp, "-1", TCL_STATIC);
		return (TCL_OK);
	}
}

#define DBCGET_USAGE "cursorN get key flags [beg len]"
int
dbc_get_cmd(interp, argc, argv, dbc)
	Tcl_Interp *interp;
	int argc;
	char *argv[];
	DBC *dbc;
{
	DBT data, key;
	db_recno_t rkey;
	u_int32_t flags;
	int ret, tclint;
	char numbuf[16];

	USAGE_GE(argc, 4, DBCGET_USAGE, 0);

	if (Tcl_GetInt(interp, argv[3], &tclint) != TCL_OK) {
		Tcl_AppendResult(interp, "\n", DBCGET_USAGE, NULL);
		return (TCL_ERROR);
	}
	flags = (u_int32_t)tclint;

	memset(&key, 0, sizeof(key));
	if (dbc->dbp->type == DB_RECNO || strcmp(argv[1], "getn") == 0) {
		if (Tcl_GetInt(interp, argv[2], &tclint) != TCL_OK)
			return (TCL_ERROR);
		rkey = (db_recno_t)tclint;
		key.data = &rkey;
		key.size = sizeof(db_recno_t);
	} else {
		key.data = argv[2];
		key.size = strlen(argv[2]) + 1;	/* Add Null on end */
	}

	memset(&data, 0, sizeof(data));
	if (LF_ISSET(DB_DBT_PARTIAL)) {
		USAGE(argc, 6, DBCGET_USAGE, 0);
		if (Tcl_GetInt(interp, argv[4], &tclint) != TCL_OK)
			return (TCL_ERROR);
		data.doff = (size_t)tclint;
		if (Tcl_GetInt(interp, argv[5], &tclint) != TCL_OK)
			return (TCL_ERROR);
		data.dlen = (size_t)tclint;
		data.flags = DB_DBT_PARTIAL;
		LF_CLR(DB_DBT_PARTIAL);
	}

	debug_check();

	if (F_ISSET(dbc->dbp, DB_AM_THREAD)) {
		F_SET(&data, DB_DBT_MALLOC);
		switch(flags) {
		case DB_FIRST:
		case DB_LAST:
		case DB_NEXT:
		case DB_PREV:
		case DB_CURRENT:
		case DB_SET_RANGE:
		case DB_SET_RECNO:
		case DB_GET_RECNO:
			F_SET(&key, DB_DBT_MALLOC);
			break;
		/* case DB_SET: Do nothing */
		}
	}
	if ((ret = dbc->c_get(dbc, &key, &data, flags)) == 0) {
		if (dbc->dbp->type == DB_RECNO) {
			if (key.size != sizeof(db_recno_t)) {
				Tcl_SetResult(interp,
				    "Error: key not integer", TCL_STATIC);
				if (F_ISSET(&data, DB_DBT_MALLOC))
					free(&data.data);
				if (F_ISSET(&key, DB_DBT_MALLOC))
					free(&key.data);
				return (TCL_ERROR);
			}
			memcpy(&rkey, key.data, sizeof(db_recno_t));
			sprintf(numbuf, "%ld", (long)rkey);
			Tcl_SetResult(interp, numbuf, TCL_VOLATILE);
		} else
			Tcl_SetResult(interp, key.data, TCL_VOLATILE);

		Tcl_AppendResult(interp, " {", NULL);
		set_get_result(interp, &data);
		Tcl_AppendResult(interp, "} ", NULL);
		if (F_ISSET(&data, DB_DBT_MALLOC))
			free(&data.data);
		if (F_ISSET(&key, DB_DBT_MALLOC))
			free(&key.data);
		return (TCL_OK);
	} else if (ret < 0) {	/* End/Beginning of file. */
		Tcl_ResetResult(interp);
		return (TCL_OK);
	} else {
		Tcl_SetResult(interp, "-1", TCL_STATIC);
		return (TCL_OK);
	}
}

#define DBCGETBIN_USAGE "cursorN getbin file key flags"
int
dbc_getbin_cmd(interp, argc, argv, dbc)
	Tcl_Interp *interp;
	int argc;
	char *argv[];
	DBC *dbc;
{
	DBT data, key, *dbt;
	db_recno_t rkey;
	u_int32_t flags;
	int ret, tclint;
	char nbuf[32];

	USAGE(argc, 5, DBCGET_USAGE, 0);

	if (Tcl_GetInt(interp, argv[4], &tclint) != TCL_OK) {
		Tcl_AppendResult(interp, "\n", DBCGET_USAGE, NULL);
		return (TCL_ERROR);
	}
	flags = (u_int32_t)tclint;

	memset(&key, 0, sizeof(key));
	if (flags == DB_SET || flags == DB_KEYFIRST || flags == DB_KEYLAST)
		if (strcmp(argv[1], "getbinkey") == 0) {
			if (dbt_from_file(interp, argv[3], &key) != TCL_OK)
				return (TCL_ERROR);
		} else if (dbc->dbp->type == DB_RECNO) {
			if (Tcl_GetInt(interp, argv[3], &tclint) != TCL_OK)
				return (TCL_ERROR);
			rkey = (db_recno_t)tclint;
			key.data = &rkey;
			key.size = sizeof(db_recno_t);
		} else {
			key.data = argv[3];
			key.size = strlen(argv[3]) + 1;	/* Add Null on end */
		}

	memset(&data, 0, sizeof(data));
	if (strcmp(argv[1], "getbinkey") != 0 && dbc->dbp->type != DB_RECNO)
		F_SET(&data, DB_DBT_MALLOC);

	debug_check();

	ret = dbc->c_get(dbc, &key, &data, flags);
	if (ret < 0) {	/* End/Beginning of file. */
		Tcl_ResetResult(interp);
		return (TCL_OK);
	} else if (ret != 0) {
		Tcl_SetResult(interp, "-1", TCL_STATIC);
		return (TCL_OK);
	}

	/* Got key; write it or its data to a file. */
	if (strcmp(argv[1], "getbinkey") == 0) {
		dbt = &key;
		Tcl_SetResult(interp, data.data, TCL_VOLATILE);
	} else if (dbc->dbp->type == DB_RECNO) {
		dbt = &data;
		memcpy(&rkey, key.data, sizeof(db_recno_t));
		sprintf(nbuf, "%ld", (long)rkey);
		Tcl_SetResult(interp, nbuf, TCL_VOLATILE);
	} else {
		dbt = &data;
		Tcl_SetResult(interp, key.data, TCL_VOLATILE);
	}

	if (dbt_to_file(interp, argv[2], dbt) != TCL_OK)
		return (TCL_ERROR);

	Tcl_AppendElement(interp, argv[2]);
	return (TCL_OK);
}

#define DBCPUT_USAGE "cursorN put key data flags"
int
dbc_put_cmd(interp, argc, argv, dbc)
	Tcl_Interp *interp;
	int argc;
	char *argv[];
	DBC *dbc;
{
	DBT data, key;
	db_recno_t ikey;
	u_int32_t flags;
	int ret, tclint;

	USAGE(argc, 5, DBCPUT_USAGE, 0);

	if (Tcl_GetInt(interp, argv[4], &tclint) != TCL_OK) {
		Tcl_AppendResult(interp, "\n", DBCPUT_USAGE, NULL);
		return (TCL_ERROR);
	}
	flags = (u_int32_t)tclint;

	/*
	 * If this is a DB_CURRENT, we should never look at the key, so
	 * send it a NULL.
	 */
	memset(&key, 0, sizeof(key));
	if (flags != DB_CURRENT)
		if (strcmp(argv[1], "putn") == 0 ||
		    dbc->dbp->type == DB_RECNO) {
			if (Tcl_GetInt(interp, argv[2], &tclint) != TCL_OK)
				return (TCL_ERROR);
			ikey = (db_recno_t)tclint;
			key.data = &ikey;
			key.size = sizeof(db_recno_t);
		} else {
			key.data = argv[2];
			key.size = strlen(argv[2]) + 1;	/* Add Null on end */
		}

	memset(&data, 0, sizeof(data));
	data.data = argv[3];
	data.size = strlen(argv[3]) + 1;

	debug_check();

	if ((ret = dbc->c_put(dbc, &key, &data, flags)) == 0)
		return (TCL_OK);
	else if (ret < 0) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Key ", argv[2], " not found.", NULL);
		return (TCL_OK);
	} else {
		Tcl_SetResult(interp, "-1", TCL_STATIC);
		return (TCL_OK);
	}
}

u_int8_t *
list_to_numarray(interp, str)
	Tcl_Interp *interp;
	char *str;
{
	int argc, i, tmp;
	u_int8_t *nums, *np;
	char **argv, **ap;

	if (Tcl_SplitList(interp, str, &argc, &argv) != TCL_OK)
		return (NULL);
	if ((nums = (u_int8_t *)calloc(argc, sizeof(u_int8_t))) == NULL) {
		Tcl_SetResult(interp, Tcl_PosixError(interp), TCL_STATIC);
		return (NULL);
	}

	for (np = nums, ap = argv, i = 0; i < argc; i++, ap++, np++) {
		if (Tcl_GetInt(interp, *ap, &tmp) != TCL_OK) {
			free (nums);
			return (NULL);
		}
		*np = (u_int8_t)tmp;		/* XXX: Possible overflow. */
	}
#ifndef _WIN32
	/*
	 * XXX
	 * This currently traps on Windows/NT, probably due to a mismatch of
	 * malloc/free implementations between the TCL library and this module.
	 * Sidestep the issue for now.
	 */
	free(argv);
#endif
	return (nums);
}

#define STAMP_USAGE "timestamp [-r]"
int
stamp_cmd(notused, interp, argc, argv)
	ClientData notused;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	static time_t start;
	struct tm *tp;
	time_t elapsed, now;
	static char buf[25];

	notused = NULL;

	USAGE_GE(argc, 1, STAMP_USAGE, 0);
	(void)time(&now);
	if (now == (time_t)-1)
		goto err;
	if (argc > 1 && strcmp(argv[1], "-r") == 0) {
		sprintf(buf, "%lu", (u_long)now);
		Tcl_SetResult(interp, buf, TCL_STATIC);
		return (TCL_OK);
	}
	if ((tp = localtime(&now)) == NULL)
		goto err;

	if (start == 0)
		start = now;
	elapsed = now - start;
	(void)sprintf(buf, "%02d:%02d:%02d (%02d:%02d:%02d)",
	    tp->tm_hour, tp->tm_min, tp->tm_sec, (int)(elapsed / 3600),
	    (int)((elapsed % 3600) / 60), (int)(((elapsed % 3600) % 60)));
	start = now;

	Tcl_SetResult(interp, buf, TCL_STATIC);
	return (TCL_OK);

err:	Tcl_SetResult(interp, Tcl_PosixError(interp), TCL_STATIC);
	return (TCL_ERROR);
}

int debug_stop;				/* Stop on each iteration. */

void
debug_check()
{
	if (debug_on == 0)
		return;

	if (debug_print != 0) {
		printf("\r%6d:", debug_on);
		fflush(stdout);
	}

	if (debug_on++ == debug_test || debug_stop)
		__db_loadme();
}

int
dbt_from_file(interp, file, dbt)
	Tcl_Interp *interp;
	char *file;
	DBT *dbt;
{
	size_t len, size;
	ssize_t nr;
	u_int32_t mbytes, bytes;
	int fd;

	/* Get data from file. */
	fd = -1;
	len = strlen(file) + 1;
	if ((errno = __db_open(file, DB_RDONLY, DB_RDONLY, 0, &fd)) != 0)
		goto err;
	if ((errno = __db_ioinfo(file, fd, &mbytes, &bytes, NULL)) != 0)
		goto err;
	size = mbytes * MEGABYTE + bytes;
	if ((dbt->data = (void *)malloc(size + len)) == NULL)
		goto err;
	if ((errno =
	    __db_read(fd, ((u_int8_t *)dbt->data) + len, size, &nr)) != 0)
		goto err;
	if (nr != (ssize_t)size) {
		errno = EIO;
		goto err;
	}
	if ((errno =__db_close(fd)) != 0) {
err:		if (fd != -1)
			close(fd);
		Tcl_SetResult(interp, Tcl_PosixError(interp), TCL_STATIC);
		return (TCL_ERROR);
	}
	strcpy((char *)(dbt->data), file);
	dbt->size = size + (u_int32_t)len;
	dbt->flags = 0;
	return (TCL_OK);
}

/*
 * file is the file we are creating.  ofile is the original file
 * name so we know how many characters to drop off the end.
 */
int
dbt_to_file(interp, file, dbt)
	Tcl_Interp *interp;
	char *file;
	DBT *dbt;
{
	size_t len;
	ssize_t nw;
	int fd;
	char *p;

	/* Got key; write it to file. */
	for (len = 1, p = (char *)(dbt->data); *p != '\0'; len++, p++)
		;
	p++;

	fd = -1;
	if ((errno = __db_open(file, DB_CREATE | DB_TRUNCATE,
	    DB_CREATE | DB_TRUNCATE, 0644, &fd)) != 0 ||
	    (errno = __db_write(fd, p, dbt->size - len, &nw) != 0) ||
	    nw != (ssize_t)(dbt->size - len) ||
	    (errno = __db_close(fd)) != 0) {
		if (errno == 0)
			errno = EIO;
		if (fd == -1)
			(void)__db_close(fd);
		Tcl_SetResult(interp, Tcl_PosixError(interp), TCL_STATIC);
		return (TCL_ERROR);
	}
	if (F_ISSET(dbt, DB_DBT_MALLOC))
		free(dbt->data);
	return (TCL_OK);
}

void
set_get_result(interp, dbt)
	Tcl_Interp *interp;
	DBT *dbt;
{
	size_t i;
	u_int8_t *p;
	char numbuf[32], sprbuf[128], *outbuf;

	for (i = 0, p = dbt->data; i < dbt->size && *p == '\0'; i++, p++)
		;
	/*
	 * If this is a partial get, we need to make sure that the last
	 * character is a NUL so that tcl can handle it.
	 */
	if (F_ISSET(dbt, DB_DBT_PARTIAL)) {
		if (dbt->size == 0)
			((char *)dbt->data)[0] = '\0';
		else
			((char *)dbt->data)[dbt->size - 1] = '\0';
	}

	if (i == 0) {
		if (((u_int8_t *)dbt->data)[dbt->size - 1] != '\0') {
			outbuf = (char *)malloc(dbt->size + 1);
			memcpy(outbuf, dbt->data, dbt->size);
			outbuf[dbt->size] = '\0';
		} else
			outbuf = dbt->data;
		Tcl_AppendResult(interp, outbuf, NULL);
		if (outbuf != dbt->data)
			free(outbuf);
	} else {
		sprintf(&numbuf[0], "%lu", (u_long)i);
		sprintf(&sprbuf[0], " %%.%ds", dbt->size - i);
		outbuf = (char *)malloc(dbt->size - i + 5);
		sprintf(outbuf, sprbuf, p);
		Tcl_AppendResult(interp, numbuf, outbuf, NULL);
		free(outbuf);
	}
}

#define SRAND_USAGE "srand seed"
int
srand_cmd(notused, interp, argc, argv)
	ClientData notused;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	int ival;

	notused = NULL;
	USAGE(argc, 2, SRAND_USAGE, 0);
	if (Tcl_GetInt(interp, argv[1], &ival) != TCL_OK)
		return (TCL_ERROR);

	srand((u_int)ival);
	Tcl_SetResult(interp, "0", TCL_STATIC);
	return (TCL_OK);
}

#define RAND_USAGE "rand"
int
rand_cmd(notused, interp, argc, argv)
	ClientData notused;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	char retbuf[50];

	notused = NULL;
	argv = NULL;
	USAGE(argc, 1, RAND_USAGE, 0);

	sprintf(retbuf, "%ld", (long)rand());
	Tcl_SetResult(interp, retbuf, TCL_VOLATILE);
	return (TCL_OK);
}

#define RANDOMINT_USAGE "random_int lo hi"
int
randomint_cmd(notused, interp, argc, argv)
	ClientData notused;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	long t;
	int lo, hi, ret;
	char retbuf[50];

	notused = NULL;
	USAGE(argc, 3, RANDOMINT_USAGE, 0);
	if (Tcl_GetInt(interp, argv[1], &lo) != TCL_OK ||
	    Tcl_GetInt(interp, argv[2], &hi) != TCL_OK)
		return (TCL_ERROR);

#ifndef	RAND_MAX
#define	RAND_MAX	0x7fffffff
#endif
	t = rand();
	if (t > RAND_MAX)
		printf("Max random is higher than %ld\n", (long)RAND_MAX);
	ret = (int)(((double)t / ((double)(RAND_MAX) + 1)) * (hi - lo + 1));
	ret += lo;
	sprintf(retbuf, "%d", ret);
	Tcl_SetResult(interp, retbuf, TCL_VOLATILE);
	return (TCL_OK);
}

#define VERSION_USAGE "db_version"
int
dbversion_cmd(notused, interp, argc, notused2)
	ClientData notused;
	Tcl_Interp *interp;
	int argc;
	char *notused2[];
{
	char *str;

	notused = NULL;
	notused2 = NULL;
	USAGE(argc, 1, VERSION_USAGE, 0);
	str = db_version(NULL, NULL, NULL);
	if (str == NULL) {
		Tcl_SetResult(interp, "db_version: ", TCL_STATIC);
		Tcl_AppendResult(interp, Tcl_PosixError(interp), 0);
		return (TCL_ERROR);
	}
	Tcl_SetResult(interp, str, TCL_DYNAMIC);
	return (TCL_OK);
}

/*
 * Create an environment that we can pass around between different
 * calls.
 */
#define DBENV_USAGE "dbenv "
int
dbenv_cmd(notused, interp, argc, argv)
	ClientData notused;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	static int env_number = 0;
	DB_ENV *env;
	char envname[50];

	debug_check();
	notused = NULL;

	/* Check number of arguments. */
	USAGE_GE(argc, 1, DBENV_USAGE, DO_ENV);

	process_env_options(interp, argc - 1, &argv[1], &env);
	if (env == NULL) {
		Tcl_SetResult(interp, "NULL", TCL_STATIC);
		return (TCL_OK);
	}

	F_SET(env, DB_ENV_STANDALONE);

	/* Create new command name. */
	sprintf(&envname[0], "env%d", env_number);
	env_number++;

	Tcl_CreateCommand(interp, envname, envwidget_cmd, (ClientData)env,
	    envwidget_delcmd);
	Tcl_SetResult(interp, envname, TCL_VOLATILE);
	return (TCL_OK);
}

void
envwidget_delcmd(cd)
	ClientData cd;
{
	DB_ENV *env;

	debug_check();
	env = (DB_ENV *)cd;

	(void)db_appexit(env);
	if (env->lk_conflicts)
		free(env->lk_conflicts);
	free(env);
}

#define	ENVWIDGET_USAGE "envN"
#define	ENVWIDGET_SIMPLEDUP "envN simpledup"
int
envwidget_cmd(cd, interp, argc, argv)
	ClientData cd;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	static int nenv_number = 0;
	DB_ENV *env, *newenv;
	char **p, nenvname[50];

	argv = argv;
	env = (DB_ENV *)cd;

	if (argc == 1) {
		USAGE(argc, 1, ENVWIDGET_USAGE, 0);

		Tcl_ResetResult(interp);
		if (env->db_home != NULL)
			Tcl_AppendResult(interp, " Home: ", env->db_home, 0);
		if ((p = env->db_data_dir) != NULL)
			for (; *p != NULL; ++p)
				Tcl_AppendResult(interp, " Data: ", *p, 0);
		if (env->db_log_dir != NULL)
			Tcl_AppendResult(interp, " Log: ", env->db_log_dir, 0);
		if (env->db_tmp_dir != NULL)
			Tcl_AppendResult(interp, " Tmp: ", env->db_tmp_dir, 0);
	} else if (strcmp(argv[1], "simpledup") == 0) {
		USAGE(argc, 2, ENVWIDGET_SIMPLEDUP, 0);
		/*
		 * Copy the env and then NULL out the log, lock and
		 * transaction info pointers so that we only share an
		 * mpool.
		 */
		newenv = (DB_ENV *)malloc(sizeof(*newenv));
		*newenv = *env;
		newenv->lg_info = NULL;
		newenv->lk_info = NULL;
		newenv->tx_info = NULL;

		/* Create new command name. */
		sprintf(&nenvname[0], "nenv%d", nenv_number);
		nenv_number++;

		Tcl_CreateCommand(interp, nenvname, envwidget_cmd,
		    (ClientData)newenv, NULL);
		Tcl_SetResult(interp, nenvname, TCL_VOLATILE);
	} else {
		Tcl_SetResult(interp, "Invalid command", TCL_STATIC);
		return (TCL_ERROR);
	}

	return (TCL_OK);
}

#define ARGS_USAGE "args"
int
args_cmd(cd, interp, argc, argv)
	ClientData cd;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	cd = cd;
	argv = argv;

	USAGE(argc, 1, ARGS_USAGE, 0);
	printf("Legal environment options are: %s\n", DB_ENV_FLAGS);
	printf("Legal access method options are: %s\n", DB_INFO_FLAGS);
	Tcl_ResetResult(interp);
	return (TCL_OK);
}

#define DEBUG_CHECK_USAGE "debug_check"
int
debugcheck_cmd(notused1, interp, argc, notused2)
	ClientData notused1;
	Tcl_Interp *interp;
	int argc;
	char *notused2[];
{
	USAGE(argc, 1, DEBUG_CHECK_USAGE, 0);
	notused1 = notused1;
	notused2 = notused2;
	debug_check();
	Tcl_ResetResult(interp);
	return (TCL_OK);
}
