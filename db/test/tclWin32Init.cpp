/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998
 *	Sleepycat Software.  All rights reserved.
 */

#ifndef lint
static const char sccsid[] = "@(#)tclWin32Init.cpp	10.2 (Sleepycat) 5/2/98";
#endif /* not lint */

#include <stdio.h>
#include <tcl.h>
#include <eh.h>

extern "C" {
#include "config.h"
#include "db_int.h"
#include "dbtest.h"
}

/*
 * The default exit handler on WIN32 brings up a dialog box.  This is quite
 * inconvenient when we want to run all the tests without user intervention,
 * so tie this behavior to the debug_on var.
 */
void win32_terminate()
{
	fprintf(stderr, "\nFAIL: dbtest exited unexpectedly\n");
	if (debug_on) {
		/* Dialog box with option to go into debugger */
		abort();
	}
	else {
		/* Immediate exit */
		exit(1);
	}
	// should not return
}


extern "C" void setup_terminate()
{
	(void)set_terminate(win32_terminate);
}

extern "C" int main2(int argc, char *argv[]);

#define IS_WNT ((GetVersion() & 0x80000000) == 0)

int
main(int argc, char *argv[])
{
	int argoff = 0;
	int debug = 0;

	if (!IS_WNT) {
		db_value_set(1, DB_REGION_NAME);
	}
	setup_terminate();
	if (argc > 1 && strcmp(argv[1], "-debug") == 0) {
		debug++;
		argoff++;
	}

	// do not catch errors, let errors bring up dialog box
	if (debug) {
		return main2(argc-argoff, &argv[argoff]);
	}

	try {
		return main2(argc-argoff, &argv[argoff]);
	}
	catch (...)
	{
		win32_terminate();
	}

	// should not be reached.
	return 99;
}
