/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _nsMsgComposeStringBundle_H_
#define _nsMsgComposeStringBundle_H_

#include "nscore.h"

//
// The defines needed for error conditions. The corresponding strings
// are defined in composebe.properties
// 
#define	NS_MSG_UNABLE_TO_OPEN_FILE                  12500
#define NS_MSG_UNABLE_TO_OPEN_TMP_FILE              12501
#define NS_MSG_UNABLE_TO_SAVE_TEMPLATE              12502
#define NS_MSG_UNABLE_TO_SAVE_DRAFT                 12503
#define NS_MSG_LOAD_ATTACHMNTS                      12504
#define NS_MSG_LOAD_ATTACHMNT                       12505
#define NS_MSG_COULDNT_OPEN_FCC_FILE                12506
#define NS_MSG_CANT_POST_TO_MULTIPLE_NEWS_HOSTS     12507
#define NS_MSG_ASSEMB_DONE_MSG                      12508
#define NS_MSG_ASSEMBLING_MSG                       12509
#define NS_MSG_NO_SENDER                            12510
#define NS_MSG_NO_RECIPIENTS                        12511

// We want error codes to be negative #s...then we can be
// extra nifty and pass these values in as nsresults and they
// will be properly detected as failure codes.

#define NS_MSG_ERROR_WRITING_FILE                   12512
#define NS_ERROR_COULD_NOT_LOGIN_TO_SMTP_SERVER     -12513
#define NS_ERROR_SENDING_FROM_COMMAND               12514
#define NS_ERROR_SENDING_RCPT_COMMAND               12515
#define NS_ERROR_SENDING_DATA_COMMAND               12516
#define NS_ERROR_SENDING_MESSAGE                    12517
#define NS_ERROR_SERVER_ERROR                       12518
#define NS_ERROR_QUEUED_DELIVERY_FAILED             12519
#define NS_ERROR_SEND_FAILED                        12520
#define SMTP_DELIV_MAIL                             12521
#define SMTP_MESSAGE_SENT_WAITING_MAIL_REPLY        12522
#define SMTP_PROGRESS_MAILSENT                      12523
#define SMTP_SERVER_ERROR                           12524

NS_BEGIN_EXTERN_C

PRUnichar     *ComposeGetStringByID(PRInt32 stringID);

NS_END_EXTERN_C

#endif /* _nsMsgComposeStringBundle_H_ */
