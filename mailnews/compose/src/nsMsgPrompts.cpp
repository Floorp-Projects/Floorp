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
nsMsgDisplayMessageByID(PRInt32 msgID)
{
  nsresult rv;

  nsCOMPtr<nsIMsgStringService> composebundle (do_GetService(NS_MSG_COMPOSESTRINGSERVICE_PROGID, &rv));
  nsXPIDLString msg;

  if (composebundle)
  {
    composebundle->GetStringByID(msgID, getter_Copies(msg));
    rv = nsMsgDisplayMessageByString(msg);
  }
  return rv;
}

nsresult
nsMsgDisplayMessageByString(const PRUnichar * msg)
{
  nsresult rv;

  if ((!msg) || (!*msg))
    return NS_ERROR_INVALID_ARG;

  NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &rv);
  if (NS_FAILED(rv)) return rv;
  rv = dialog->Alert(msg);
  return NS_OK;
}

nsresult
nsMsgAskBooleanQuestionByID(PRInt32 msgID, PRBool *answer)
{
  nsCOMPtr<nsIMsgStringService> composebundle (do_GetService(NS_MSG_COMPOSESTRINGSERVICE_PROGID));
  nsXPIDLString msg;

  if (composebundle)
  {
    composebundle->GetStringByID(msgID, getter_Copies(msg));
    nsMsgAskBooleanQuestionByString(msg, answer);
  }

  return NS_OK;
}     

nsresult
nsMsgAskBooleanQuestionByString(const PRUnichar * msg, PRBool *answer)
{
  nsresult rv;
  PRInt32 result;

  if ((!msg) || (!*msg))
    return NS_ERROR_INVALID_ARG;

  NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &rv);  
  if (NS_FAILED(rv))
    return NS_ERROR_FACTORY_NOT_LOADED;
  
  if (dialog) 
  {
    rv = dialog->Confirm(msg, &result);
    if (result == 1) 
      *answer = PR_TRUE;
    else 
      *answer = PR_FALSE;
  }

  return NS_OK;
}     
