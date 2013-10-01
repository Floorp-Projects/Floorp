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
