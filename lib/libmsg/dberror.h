/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef DBERROR_H
#define DBERROR_H

#include "errcode.h"

/* General errors */
MsgError (msgErrorDb, eFAILURE,					0xFFFF)	/* -1	Unqualified DB failure */
MsgError (msgErrorDb, eBAD_PARAMETER,			0xFFFE)	/* -2	Null pointer, empty string, etc. */
MsgError (msgErrorDb, eMORE,					0xFFFD)	/* -3	More work to do. */
MsgError (msgErrorDb, eNYI,						0xFFFC)	/* -4	Not yet implemented */
MsgError (msgErrorDb, eEXCEPTION,				0xFFFB)	/* -5	An unexpected exception occurred */
MsgError (msgErrorDb, eBAD_VIEW_INTF,			0xFFFA)	/* -6	COM intf to view layer is out of sync */
MsgError (msgErrorDb, eBAD_DB_INTF,				0xFFF9)	/* -7 	COM intf to db is out of sync */
MsgError (msgErrorDb, eID_NOT_FOUND,			0xFFF8) /* -8   Message id not found */
MsgError (msgErrorDb, eDBEndOfList,				0xFFF7) /* -9   iterator reached end of db */
MsgError (msgErrorDb, eBAD_URL,					0xFFF5) /* -11  Unable to parse URL */
/* NewsRC errors */
MsgError (msgErrorDb, eNewsRCError,				0xFFF4) /* -10   General news rc error */
/* News errors */
MsgError (msgErrorDb, eXOverParseError,			0xFFF3) /* -13  Error parsing XOver data */
/* Internal db errors */
MsgError (msgErrorDb, eDBExistsNot,				0xFFF2)  /* -14 db doesn't exist */
MsgError (msgErrorDb, eDBNotOpen,			0xFFF1)  /* -15 our db is not open (or ptr NULL) */
MsgError (msgErrorDb, eErrorOpeningDB,		0xFFF0) /* -16 Error opening internal db */
MsgError (msgErrorDb, eOldSummaryFile,			0xFFE0) /* -32 Attempt to oepn old summary file format */
MsgError (msgErrorDb, eNullView,				0xFFE1) /* -31 View Null */
MsgError (msgErrorDb, eNotThread,				0xFFE2) /* -30 operation requires a thread */
MsgError (msgErrorDb, eNoSummaryFile,			0xFFE3)  /* -31 summary file was gone */
MsgError (msgErrorDb, eBuildViewInBackground,	0xFFE4)  /* -32 db is huge - build view in background */
MsgError (msgErrorDb, eCorruptDB,				0xFFE5) /* -33 db is corrupt - throw it away */
/* Platform System Errors */
 			
MsgError (msgErrorDb, eOUT_OF_MEMORY,			0xFF7F)	/* -129	Out of memory for buffer or file-level objects */

/* file errors */
MsgError (msgErrorDb, eWRITE_ERROR,				0xFFC0) /* -64 Error writing to disk */
#endif
