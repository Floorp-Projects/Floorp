/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


/* merrors.h --- error codes for netlib. */


#ifndef _MERRORS_H_
#define _MERRORS_H_

/*
 * Return codes
 */

#define MK_INTERRUPTED			-201

#define MK_UNABLE_TO_CONVERT		-208

#define MK_UNABLE_TO_LOGIN		-210

#define MK_NO_NEWS_SERVER		-224
#define MK_USE_FTP_INSTEAD		-225
#define MK_USE_COPY_FROM_CACHE		-226
#define MK_EMPTY_NEWS_LIST              -227

#define MK_MAILTO_NOT_READY		-228

#define MK_OBJECT_NOT_IN_CACHE		-239

#define MK_UNABLE_TO_LISTEN_ON_SOCKET   -244

#define MK_WAITING_FOR_LOOKUP		-248	/* response for async dns */
#define MK_DO_REDIRECT			-249	/* tells mkgeturl to redirect */

#define MK_MIME_NEED_B64		-270	/* used internally */
#define MK_MIME_NEED_QP			-271	/* used internally */
#define MK_MIME_NEED_TEXT_CONVERTER	-272	/* used internally */
#define MK_MIME_NEED_PS_CONVERTER	-273	/* used internally */

#define MK_IMAGE_LOSSAGE		-277

#define MK_TOO_MANY_OPEN_FILES		-310

#define MK_FILE_WRITE_ERROR		-350

#define MK_GET_REST_OF_PARTIAL_FILE_FROM_NETWORK	-399

#define MK_MULTIPART_MESSAGE_COMPLETED	-437

#define MK_OFFLINE -438

/* success codes */
#define MK_DATA_LOADED		1
#define MK_NO_DATA		2
#define MK_NO_ACTION		3
#define MK_CHANGING_CONTEXT	4


#endif /* _MERRORS_H_ */
