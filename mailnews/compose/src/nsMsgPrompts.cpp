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
#include "nsMsgCopy.h"
#include "nsIPref.h"
#include "nsMsgPrompts.h"
#include "nsINetSupportDialogService.h"
#include "nsIMsgStringService.h"
#include "nsMsgComposeStringBundle.h"
#include "nsXPIDLString.h"
#include "nsMsgCompCID.h"

static NS_DEFINE_CID(kNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);

nsresult
nsMsgDisplayMessageByID(nsIPrompt * aPrompt, PRInt32 msgID)
{
  nsresult rv;

  nsCOMPtr<nsIMsgStringService> composebundle (do_GetService(NS_MSG_COMPOSESTRINGSERVICE_CONTRACTID, &rv));
  nsXPIDLString msg;

  if (composebundle)
  {
    composebundle->GetStringByID(msgID, getter_Copies(msg));
    rv = nsMsgDisplayMessageByString(aPrompt, msg);
  }
  return rv;
}

nsresult
nsMsgDisplayMessageByString(nsIPrompt * aPrompt, const PRUnichar * msg)
{
  nsresult rv;
  nsCOMPtr<nsIPrompt> prompt = aPrompt;

  if ((!msg) || (!*msg))
    return NS_ERROR_INVALID_ARG;
  
  if (!prompt)
    prompt = do_GetService(kNetSupportDialogCID);
  
  if (prompt)
    rv = prompt->Alert(nsnull, msg);
  return NS_OK;
}

nsresult
nsMsgAskBooleanQuestionByID(nsIPrompt * aPrompt, PRInt32 msgID, PRBool *answer)
{
  nsCOMPtr<nsIMsgStringService> composebundle (do_GetService(NS_MSG_COMPOSESTRINGSERVICE_CONTRACTID));
  nsXPIDLString msg;

  if (composebundle)
  {
    composebundle->GetStringByID(msgID, getter_Copies(msg));
    nsMsgAskBooleanQuestionByString(aPrompt, msg, answer);
  }

  return NS_OK;
}     

nsresult
nsMsgAskBooleanQuestionByString(nsIPrompt * aPrompt, const PRUnichar * msg, PRBool *answer)
{
  nsresult rv;
  PRInt32 result;
  nsCOMPtr<nsIPrompt> dialog = aPrompt;

  if ((!msg) || (!*msg))
    return NS_ERROR_INVALID_ARG;
  
  if (!dialog)
    dialog = do_GetService(kNetSupportDialogCID);
  
  if (dialog) 
  {
    rv = dialog->Confirm(nsnull, msg, &result);
    if (result == 1) 
      *answer = PR_TRUE;
    else 
      *answer = PR_FALSE;
  }

  return NS_OK;
}     
