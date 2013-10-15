/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
	Error messages for JSShell. See js.msg for format.
*/

MSG_DEF(JSSMSG_NOT_AN_ERROR,             0, 0, JSEXN_NONE, "<Error #0 is reserved>")
MSG_DEF(JSSMSG_CANT_OPEN,                1, 2, JSEXN_NONE, "can't open {0}: {1}")
MSG_DEF(JSSMSG_TRAP_USAGE,               2, 0, JSEXN_NONE, "usage: trap [fun] [pc] expr")
MSG_DEF(JSSMSG_LINE2PC_USAGE,            3, 0, JSEXN_NONE, "usage: line2pc [fun] line")
MSG_DEF(JSSMSG_FILE_SCRIPTS_ONLY,        4, 0, JSEXN_NONE, "only works on JS scripts read from files")
MSG_DEF(JSSMSG_UNEXPECTED_EOF,           5, 1, JSEXN_NONE, "unexpected EOF in {0}")
MSG_DEF(JSSMSG_DOEXP_USAGE,              6, 0, JSEXN_NONE, "usage: doexp obj id")
MSG_DEF(JSSMSG_SCRIPTS_ONLY,             7, 0, JSEXN_NONE, "only works on scripts")
MSG_DEF(JSSMSG_NOT_ENOUGH_ARGS,          8, 1, JSEXN_NONE, "{0}: not enough arguments")
MSG_DEF(JSSMSG_TOO_MANY_ARGS,            9, 1, JSEXN_NONE, "{0}: too many arguments")
MSG_DEF(JSSMSG_ASSERT_EQ_FAILED,        10, 2, JSEXN_NONE, "Assertion failed: got {0}, expected {1}")
MSG_DEF(JSSMSG_ASSERT_EQ_FAILED_MSG,    11, 3, JSEXN_NONE, "Assertion failed: got {0}, expected {1}: {2}")
MSG_DEF(JSSMSG_INVALID_ARGS,            12, 1, JSEXN_NONE, "{0}: invalid arguments")
MSG_DEF(JSSMSG_BAD_ALIGNMENT,           13, 0, JSEXN_NONE, "serialized data must be 8-byte-aligned")
MSG_DEF(JSSMSG_BAD_ENV_VAR,             14, 1, JSEXN_NONE, "unable to read getenv({0})")
MSG_DEF(JSSMSG_NESTED_FAIL,             15, 0, JSEXN_NONE, "error executing nested JS shell")
