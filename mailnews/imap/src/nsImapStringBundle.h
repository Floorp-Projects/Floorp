/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL. You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All Rights
 * Reserved.
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
#define	XP_RECEIVING_MESSAGE_FLAGS_OF                                  5037
#define	IMAP_DELETING_MESSAGES                                  5038
#define	IMAP_DELETING_MESSAGE                                  5039
#define	IMAP_MOVING_MESSAGES_TO                                  5040
#define	IMAP_MOVING_MESSAGE_TO                                  5041
#define	IMAP_COPYING_MESSAGES_TO                                  5042
#define	IMAP_COPYING_MESSAGE_TO                                  5043
#define	IMAP_SELECTING_MAILBOX                                  5044


#endif /* _nsImapStringBundle_H__ */
