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

/*
 * shr_str.h
 * ---------
 *
 * Error codes that are shared by more than one module of
 * Netscape Navigator. This file is probably a temporary
 * workaround until we can get a more sophisticated sharing
 * mechanism in place.
 *
 * Codes are represented as fixed numbers, not offsets off
 * of a base, to avoid namespace collision.
 * 
 */

#ifndef SHR_STR_H
#define SHR_STR_H

#define MK_OUT_OF_MEMORY                -207
#define MK_UNABLE_TO_OPEN_FILE          -223
#define MK_DISK_FULL                    -250
#define MK_UNABLE_TO_OPEN_TMP_FILE      -253
#define MK_MIME_NO_RECIPIENTS           -267
#define MK_NNTP_SERVER_NOT_CONFIGURED   -307
#define MK_UNABLE_TO_DELETE_FILE        -327

#define MK_MSG_DELIV_MAIL               15412
#define MK_MSG_DELIV_NEWS               15414
#define MK_MSG_SAVE_AS                  15483
#define MK_MSG_NO_HEADERS               15528
#define MK_MSG_MIME_MAC_FILE            15530
#define MK_MSG_CANT_OPEN                15540
#define XP_MSG_UNKNOWN                  15572
#define XP_EDIT_NEW_DOC_NAME            15629

#define XP_ALERT_TITLE_STRING           -7956
#define XP_SEC_SHOWCERT                 -7963
#define XP_SEC_SHOWORDER                -7962


#ifdef XP_WIN

/* for Winsock defined error codes */
#include "winsock.h"

#define XP_ERRNO_ECONNREFUSED           WSAECONNREFUSED
#define XP_ERRNO_EIO                    WSAECONNREFUSED
#define XP_ERRNO_EISCONN                WSAEISCONN
#define XP_ERRNO_EWOULDBLOCK            EWOULDBLOCK

#endif /* XP_WIN */

#endif
