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

#ifndef _nsLocalStringBundle_H__
#define _nsLocalStringBundle_H__

#include "nsIMsgStringService.h"
#include "nsIStringBundle.h"
#include "nsMsgLocalCID.h"
#include "nsCOMPtr.h"

class nsLocalStringService : public nsIMsgStringService
{
public:
  nsLocalStringService();
  virtual ~nsLocalStringService();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGSTRINGSERVICE

private:
  nsCOMPtr<nsIStringBundle> mLocalStringBundle;
  nsresult InitializeStringBundle();
};

#define	IMAP_OUT_OF_MEMORY                                 -1000
#define	LOCAL_STATUS_SELECTING_MAILBOX                      4000
#define	LOCAL_STATUS_DOCUMENT_DONE							4001
#define LOCAL_STATUS_RECEIVING_MESSAGE_OF					4002
#define POP3_SERVER_ERROR									4003
#define POP3_USERNAME_FAILURE								4004
#define POP3_PASSWORD_FAILURE								4005
#define POP3_MESSAGE_WRITE_ERROR							4006
#define POP3_CONNECT_HOST_CONTACTED_SENDING_LOGIN_INFORMATION 4007
#define POP3_NO_MESSAGES									4008
#define POP3_DOWNLOAD_COUNT									4009
#define POP3_SERVER_DOES_NOT_SUPPORT_UIDL_ETC				4010
#define POP3_SERVER_DOES_NOT_SUPPORT_THE_TOP_COMMAND		4011
#define POP3_RETR_FAILURE									4012
#define POP3_PASSWORD_UNDEFINED								4013
#define POP3_USERNAME_UNDEFINED								4014
#define POP3_LIST_FAILURE									4015
#define POP3_DELE_FAILURE									4016
#define POP3_ENTER_PASSWORD_PROMPT                          4017
#define POP3_PREVIOUSLY_ENTERED_PASSWORD_IS_INVALID_ETC     4018
#define POP3_NO_ANSWER                                      4019
#define POP3_ENTER_PASSWORD_PROMPT_TITLE			4020
#define POP3_MOVE_FOLDER_TO_TRASH                           4021
#define POP3_FOLDER_ALREADY_EXISTS                          4022
#define POP3_FOLDER_FOR_TRASH                               4023
#define POP3_STAT_FAILURE                                   4024
#define POP3_SERVER_SAID                                    4025
#define DELETING_MSGS_STATUS                                4026
#define COPYING_MSGS_STATUS                                 4027
#define MOVING_MSGS_STATUS                                  4028
#define POP3_MESSAGE_FOLDER_BUSY                            4029

#endif /* _nsImapStringBundle_H__ */
