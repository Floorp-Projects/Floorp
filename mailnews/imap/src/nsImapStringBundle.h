/*
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
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */ 

#ifndef _nsImapStringBundle_H__
#define _nsImapStringBundle_H__

NS_BEGIN_EXTERN_C

PRUnichar     *IMAPGetStringByID(PRInt32 stringID);

NS_END_EXTERN_C



#define	IMAP_OUT_OF_MEMORY                                 -1000
#define	IMAP_STATUS_SELECTING_MAILBOX                                  5000
#define	IMAP_STATUS_CREATING_MAILBOX                                  5001
#define	IMAP_STATUS_DELETING_MAILBOX                                  5002
#define	IMAP_STATUS_RENAMING_MAILBOX                                  5003
#define	IMAP_STATUS_LOOKING_FOR_MAILBOX                                  5004
#define	IMAP_STATUS_SUBSCRIBE_TO_MAILBOX                                  5005
#define	IMAP_STATUS_UNSUBSCRIBE_MAILBOX                                  5006
#define	IMAP_STATUS_SEARCH_MAILBOX                                  5007
#define	IMAP_STATUS_MSG_INFO                                  5008
#define	IMAP_STATUS_CLOSE_MAILBOX                                  5009
#define	IMAP_STATUS_EXPUNGING_MAILBOX                                  5010
#define	IMAP_STATUS_LOGGING_OUT                                  5011
#define	IMAP_STATUS_CHECK_COMPAT                                  5012
#define	IMAP_STATUS_SENDING_LOGIN                                  5013
#define	IMAP_STATUS_SENDING_AUTH_LOGIN                                  5014
#define	IMAP_DOWNLOADING_MESSAGE                                  5015
#define	IMAP_CREATE_FOLDER_BUT_NO_SUBSCRIBE                                  5016
#define	IMAP_DELETE_FOLDER_BUT_NO_UNSUBSCRIBE                                  5017
#define	IMAP_RENAME_FOLDER_BUT_NO_SUBSCRIBE                                  5018
#define	IMAP_RENAME_FOLDER_BUT_NO_UNSUBSCRIBE                                  5019
#define	IMAP_STATUS_GETTING_NAMESPACE                                  5020
#define	IMAP_UPGRADE_NO_PERSONAL_NAMESPACE                                  5021
#define	IMAP_UPGRADE_TOO_MANY_FOLDERS                                  5022
#define	IMAP_UPGRADE_PROMPT_USER                                  5023
#define	IMAP_UPGRADE_PROMPT_USER_2                                  5024
#define	IMAP_UPGRADE_PROMPT_QUESTION                                  5025
#define	IMAP_UPGRADE_CUSTOM                                  5026
#define	IMAP_UPGRADE_WAIT_WHILE_UPGRADE                                  5027
#define	IMAP_UPGRADE_SUCCESSFUL                                  5028
#define	IMAP_GETTING_ACL_FOR_FOLDER                                  5029
#define	IMAP_GETTING_SERVER_INFO                                  5030
#define	IMAP_GETTING_MAILBOX_INFO                                  5031
#define	IMAP_EMPTY_MIME_PART                                  5032
#define	IMAP_UNABLE_TO_SAVE_MESSAGE                                  5033
#define	IMAP_NO_ONLINE_FOLDER                                  5034
#define	IMAP_LOGIN_FAILED                                  5035
#define	IMAP_RECEIVING_MESSAGE_HEADERS_OF                                  5036
#define	IMAP_RECEIVING_MESSAGE_FLAGS_OF                                  5037
#define	IMAP_DELETING_MESSAGES                                  5038
#define	IMAP_DELETING_MESSAGE                                  5039
#define	IMAP_MOVING_MESSAGES_TO                                  5040
#define	IMAP_MOVING_MESSAGE_TO                                  5041
#define	IMAP_COPYING_MESSAGES_TO                                  5042
#define	IMAP_COPYING_MESSAGE_TO                                  5043
#define	IMAP_SELECTING_MAILBOX                                  5044
#define IMAP_FOLDER_RECEIVING_MESSAGE_OF						5045
#define IMAP_DISCOVERING_MAILBOX								5046
#define IMAP_ENTER_PASSWORD_PROMPT								5047
#define IMAP_SERVER_NOT_IMAP4									5048
#define IMAP_SERVER_SAID										5049
#define IMAP_DONE												5050
#define IMAP_ENTER_PASSWORD_PROMPT_TITLE			5051
#define IMAP_UNKNOWN_HOST_ERROR						5052
#define IMAP_CONNECTION_REFUSED_ERROR               5053
#define IMAP_NET_TIMEOUT_ERROR                      5054
#define IMAP_MOVE_FOLDER_TO_TRASH                   5055
#define IMAP_NO_NEW_MESSAGES                        5056
#define IMAP_DEFAULT_ACCOUNT_NAME                   5057
#define IMAP_DELETE_NO_TRASH                       5058
#endif /* _nsImapStringBundle_H__ */
