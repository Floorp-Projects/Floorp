/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsMsgPrompts.h"

#include "nsMsgCopy.h"
#include "nsIMsgStringService.h"
#include "nsIPrompt.h"
#include "nsIWindowWatcher.h"
#include "nsMsgComposeStringBundle.h"
#include "nsXPIDLString.h"
#include "nsMsgCompCID.h"

nsresult
nsMsgBuildErrorMessageByID(PRInt32 msgID, nsString& retval, nsString* param0, nsString* param1)
{
  nsresult rv;

  nsCOMPtr<nsIMsgStringService> composebundle (do_GetService(NS_MSG_COMPOSESTRINGSERVICE_CONTRACTID, &rv));
  nsXPIDLString msg;

  if (composebundle)
  {
    composebundle->GetStringByID(msgID, getter_Copies(msg));
    retval = msg;

    nsString target;
    if (param0)
    {
      target.AssignLiteral("%P0%");
      retval.ReplaceSubstring(target, *param0);
    }
    if (param1)
    {
      target.AssignLiteral("%P1%");
      retval.ReplaceSubstring(target, *param1);
    }
  }
  return rv;
}

nsresult
nsMsgDisplayMessageByID(nsIPrompt * aPrompt, PRInt32 msgID, const PRUnichar * windowTitle)
{
  nsresult rv;

  nsCOMPtr<nsIMsgStringService> composebundle (do_GetService(NS_MSG_COMPOSESTRINGSERVICE_CONTRACTID, &rv));
  nsXPIDLString msg;

  if (composebundle)
  {
    composebundle->GetStringByID(msgID, getter_Copies(msg));
    rv = nsMsgDisplayMessageByString(aPrompt, msg, windowTitle);
  }
  return rv;
}

nsresult
nsMsgDisplayMessageByString(nsIPrompt * aPrompt, const PRUnichar * msg, const PRUnichar * windowTitle)
{
  nsresult rv;
  nsCOMPtr<nsIPrompt> prompt = aPrompt;

  if ((!msg) || (!*msg))
    return NS_ERROR_INVALID_ARG;
  
  if (!prompt)
  {
    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
    if (wwatch)
      wwatch->GetNewPrompter(0, getter_AddRefs(prompt));
  }
  
  if (prompt)
    rv = prompt->Alert(windowTitle, msg);
  return NS_OK;
}

nsresult
nsMsgAskBooleanQuestionByID(nsIPrompt * aPrompt, PRInt32 msgID, PRBool *answer, const PRUnichar * windowTitle)
{
  nsCOMPtr<nsIMsgStringService> composebundle (do_GetService(NS_MSG_COMPOSESTRINGSERVICE_CONTRACTID));
  nsXPIDLString msg;

  if (composebundle)
  {
    composebundle->GetStringByID(msgID, getter_Copies(msg));
    nsMsgAskBooleanQuestionByString(aPrompt, msg, answer, windowTitle);
  }

  return NS_OK;
}     

nsresult
nsMsgAskBooleanQuestionByString(nsIPrompt * aPrompt, const PRUnichar * msg, PRBool *answer, const PRUnichar * windowTitle)
{
  nsresult rv;
  PRInt32 result;
  nsCOMPtr<nsIPrompt> dialog = aPrompt;

  if ((!msg) || (!*msg))
    return NS_ERROR_INVALID_ARG;
  
  if (!dialog)
  {
    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
    if (wwatch)
      wwatch->GetNewPrompter(0, getter_AddRefs(dialog));
  }

  if (dialog) 
  {
    rv = dialog->Confirm(windowTitle, msg, &result);
    if (result == 1) 
      *answer = PR_TRUE;
    else 
      *answer = PR_FALSE;
  }

  return NS_OK;
}     
