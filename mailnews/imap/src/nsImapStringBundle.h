/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _nsImapStringBundle_H__
#define _nsImapStringBundle_H__

#include "nsIStringBundle.h"

NS_BEGIN_EXTERN_C

nsresult      IMAPGetStringByID(PRInt32 stringID, PRUnichar **aString);
nsresult      IMAPGetStringBundle(nsIStringBundle **aBundle);

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
#define IMAP_DELETE_NO_TRASH                        5058
#define IMAP_HTML_NO_CACHED_BODY_TITLE              5059
#define IMAP_HTML_NO_CACHED_BODY_BODY               5060
#define IMAP_MAILBOX_ALREADY_EXISTS                 5061
#define IMAP_PERSONAL_FILING_CABINET                5062
#define IMAP_PFC_READ_MAIL                          5063
#define IMAP_PFC_SENT_MAIL                          5064
#define IMAP_SPECIAL_CHAR                           5065
#endif /* _nsImapStringBundle_H__ */
