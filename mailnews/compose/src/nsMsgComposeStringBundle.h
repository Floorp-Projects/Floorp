/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef _nsMsgComposeStringBundle_H_
#define _nsMsgComposeStringBundle_H_

#include "nscore.h"
#include "msgCore.h"

//
// The defines needed for error conditions and status. The corresponding strings
// are defined in composeMsgs.properties
// 

#define NS_MSG_UNABLE_TO_OPEN_FILE                  NS_MSG_GENERATE_FAILURE(12500)
#define NS_MSG_UNABLE_TO_OPEN_TMP_FILE              NS_MSG_GENERATE_FAILURE(12501)
#define NS_MSG_UNABLE_TO_SAVE_TEMPLATE              NS_MSG_GENERATE_FAILURE(12502)
#define NS_MSG_UNABLE_TO_SAVE_DRAFT                 NS_MSG_GENERATE_FAILURE(12503)
#define NS_MSG_LOAD_ATTACHMNTS                      NS_MSG_GENERATE_SUCCESS(12504)
#define NS_MSG_LOAD_ATTACHMNT                       NS_MSG_GENERATE_SUCCESS(12505)
#define NS_MSG_COULDNT_OPEN_FCC_FOLDER		    	NS_MSG_GENERATE_FAILURE(12506)
#define NS_MSG_CANT_POST_TO_MULTIPLE_NEWS_HOSTS     NS_MSG_GENERATE_FAILURE(12507)
#define NS_MSG_ASSEMB_DONE_MSG                      NS_MSG_GENERATE_SUCCESS(12508)
#define NS_MSG_ASSEMBLING_MSG                       NS_MSG_GENERATE_SUCCESS(12509)
#define NS_MSG_NO_SENDER                            NS_MSG_GENERATE_FAILURE(12510)
#define NS_MSG_NO_RECIPIENTS                        NS_MSG_GENERATE_FAILURE(12511)
#define NS_MSG_ERROR_WRITING_FILE                   NS_MSG_GENERATE_FAILURE(12512)
#define NS_ERROR_COULD_NOT_LOGIN_TO_SMTP_SERVER     NS_MSG_GENERATE_FAILURE(12513)
#define NS_ERROR_SENDING_FROM_COMMAND               NS_MSG_GENERATE_FAILURE(12514)
#define NS_ERROR_SENDING_RCPT_COMMAND               NS_MSG_GENERATE_FAILURE(12515)
#define NS_ERROR_SENDING_DATA_COMMAND               NS_MSG_GENERATE_FAILURE(12516)
#define NS_ERROR_SENDING_MESSAGE                    NS_MSG_GENERATE_FAILURE(12517)
#define NS_ERROR_SERVER_ERROR                       NS_MSG_GENERATE_FAILURE(12518)
#define NS_ERROR_QUEUED_DELIVERY_FAILED             NS_MSG_GENERATE_FAILURE(12519)
#define NS_ERROR_SEND_FAILED                        NS_MSG_GENERATE_FAILURE(12520)
#define SMTP_DELIV_MAIL                             NS_MSG_GENERATE_SUCCESS(12521)
#define SMTP_MESSAGE_SENT_WAITING_MAIL_REPLY        NS_MSG_GENERATE_SUCCESS(12522)
#define SMTP_PROGRESS_MAILSENT                      NS_MSG_GENERATE_SUCCESS(12523)
#define NS_ERROR_SMTP_SERVER_ERROR                  NS_MSG_GENERATE_FAILURE(12524)
#define NS_MSG_UNABLE_TO_SEND_LATER		    		NS_MSG_GENERATE_FAILURE(12525)
#define NS_ERROR_COMMUNICATIONS_ERROR		    	NS_MSG_GENERATE_FAILURE(12526)
#define NS_ERROR_BUT_DONT_SHOW_ALERT		    	NS_MSG_GENERATE_FAILURE(12527)
#define NS_ERROR_TCP_READ_ERROR			    		NS_MSG_GENERATE_FAILURE(12528)
#define NS_ERROR_COULD_NOT_GET_USERS_MAIL_ADDRESS   NS_MSG_GENERATE_FAILURE(12529)
#define NS_ERROR_SMTP_PASSWORD_UNDEFINED 	    	NS_MSG_GENERATE_FAILURE(12530)
#define NS_ERROR_MIME_MPART_ATTACHMENT_ERROR        NS_MSG_GENERATE_FAILURE(12531)
#define NS_MSG_FAILED_COPY_OPERATION        		NS_MSG_GENERATE_FAILURE(12532)
#define NS_MSG_NO_SUBJECT_WAS_SPECIFIED        		NS_MSG_GENERATE_SUCCESS(12533)

// For message sending operations...
#define NS_MSG_ASSEMBLING_MESSAGE        		        NS_MSG_GENERATE_SUCCESS(12534)
#define NS_MSG_GATHERING_ATTACHMENT                 NS_MSG_GENERATE_SUCCESS(12535)
#define NS_MSG_CREATING_MESSAGE                     NS_MSG_GENERATE_SUCCESS(12536)
#define NS_MSG_FAILURE_ON_OBJ_EMBED                 NS_MSG_GENERATE_FAILURE(12537)
#define NS_MSG_START_COPY_MESSAGE                   NS_MSG_GENERATE_SUCCESS(12538)
#define NS_MSG_START_COPY_MESSAGE_COMPLETE          NS_MSG_GENERATE_SUCCESS(12539)
#define NS_MSG_START_COPY_MESSAGE_FAILED            NS_MSG_GENERATE_FAILURE(12540)
#define NS_MSG_LARGE_MESSAGE_WARNING                NS_MSG_GENERATE_FAILURE(12541)

#define NS_SMTP_PASSWORD_PROMPT_TITLE  NS_MSG_GENERATE_SUCCESS(12542)
#define NS_SMTP_PASSWORD_PROMPT        NS_MSG_GENERATE_SUCCESS(12543)
#define NS_ERROR_SMTP_USERNAME_UNDEFINED NS_MSG_GENERATE_FAILURE(12544)
#define NS_SMTP_CONNECTING_TO_SERVER   NS_MSG_GENERATE_SUCCESS(12545)

#define NS_MSG_SENDING_MESSAGE                      NS_MSG_GENERATE_SUCCESS(12550)
#define NS_MSG_POSTING_MESSAGE                      NS_MSG_GENERATE_SUCCESS(12551)
#define NS_MSG_ATTACHMENT_TYPE_MISMATCH             NS_MSG_GENERATE_FAILURE(12552)

NS_BEGIN_EXTERN_C

PRUnichar     *ComposeGetStringByID(PRInt32 stringID);

NS_END_EXTERN_C

#endif /* _nsMsgComposeStringBundle_H_ */
