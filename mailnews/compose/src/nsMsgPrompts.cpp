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
#include "nsMsgPrompts.h"

#include "nsMsgCopy.h"
#include "nsIPref.h"
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
      target.AssignWithConversion("%P0%");
      retval.ReplaceSubstring(target, *param0);
    }
    if (param1)
    {
      target.AssignWithConversion("%P1%");
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
    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
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
    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
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
