/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	"@(#)test_ext.h	8.24 (Sleepycat) 4/10/98"
 */

/* Macros for error handling. */
#define	DO_ENV	0x1
#define	DO_INFO	0x2
#define USAGE(AC, R, STR, F)					\
if (AC != R) {							\
	Tcl_AppendResult(interp, "Usage: ", STR, NULL);		\
	if ((F) & DO_ENV)					\
		Tcl_AppendResult(interp, DB_ENV_FLAGS, NULL);	\
	if ((F) & DO_INFO)					\
		Tcl_AppendResult(interp, DB_INFO_FLAGS, NULL);	\
	return (TCL_ERROR);					\
}

#define USAGE_GE(AC, DAC, STR, F)				\
if (AC < DAC) {							\
	Tcl_AppendResult(interp, "Usage: ", STR, NULL);		\
	if ((F) & DO_ENV)					\
		Tcl_AppendResult(interp, DB_ENV_FLAGS, NULL);	\
	if ((F) & DO_INFO)					\
		Tcl_AppendResult(interp, DB_INFO_FLAGS, NULL);	\
	return (TCL_ERROR);					\
}


#define USAGE_RANGE(AC, RL, RH, STR, F)				\
if (AC < RL || AC > RH) {					\
	Tcl_AppendResult(interp, "Usage: ", STR, NULL);		\
	if ((F) & DO_ENV)					\
		Tcl_AppendResult(interp, DB_ENV_FLAGS, NULL);	\
	if ((F) & DO_INFO)					\
		Tcl_AppendResult(interp, DB_INFO_FLAGS, NULL);	\
	return (TCL_ERROR);					\
}

#define E_ERROR(S)                                                           \
	 return (Tcl_AppendResult(interp, (S), ": ", Tcl_PosixError(interp), \
             NULL), TCL_ERROR)

/*
 * XXX
 * These should all be Tcl_CmdProcs, but this doesn't seem to work
 * under the SunOS Release 4.1.4 (LBL) #57 compiler.
 */
int args_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int dbcursor_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int dbwidget_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int debugcheck_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int dbenv_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int dbm_delete_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int dbm_fetch_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int dbm_first_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int dbm_next_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int dbm_store_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int dbminit_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int dbopen_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int dbversion_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int envwidget_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
void envwidget_delcmd __P((ClientData clientData));
int hcreate_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int hdestroy_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int hsearch_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int lock_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int lockmgr_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int lockunlink_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int lockwidget_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int log_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int logunlink_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int logwidget_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int memp_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int mempunlink_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int mpf_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int mpwidget_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int mutex_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int mutexunlink_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int mutexwidget_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int ndbmopen_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int ndbmwidget_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int pgwidget_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int rand_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int randomint_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int srand_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int stamp_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int txn_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int txnmgr_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int txnunlink_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));
int txnwidget_cmd
    __P((ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]));

