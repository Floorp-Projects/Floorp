/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Jean-Francois Ducarroz <ducarroz@netscape.com>
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

#include "nsMsgSendReport.h"

#include "msgCore.h"
#include "nsIMsgCompose.h"
#include "nsMsgComposeStringBundle.h"
#include "nsMsgCompCID.h"
#include "nsMsgPrompts.h"
#include "nsReadableUtils.h"


NS_IMPL_ISUPPORTS1(nsMsgProcessReport, nsIMsgProcessReport)

nsMsgProcessReport::nsMsgProcessReport()
{
  NS_INIT_ISUPPORTS();
  Reset();
}

nsMsgProcessReport::~nsMsgProcessReport()
{
}

/* attribute boolean proceeded; */
NS_IMETHODIMP nsMsgProcessReport::GetProceeded(PRBool *aProceeded)
{
  NS_ENSURE_ARG_POINTER(aProceeded);
  *aProceeded = mProceeded;
  return NS_OK;
}
NS_IMETHODIMP nsMsgProcessReport::SetProceeded(PRBool aProceeded)
{
  mProceeded = aProceeded;
  return NS_OK;
}

/* attribute nsresult error; */
NS_IMETHODIMP nsMsgProcessReport::GetError(nsresult *aError)
{
  NS_ENSURE_ARG_POINTER(aError);
  *aError = mError;
  return NS_OK;
}
NS_IMETHODIMP nsMsgProcessReport::SetError(nsresult aError)
{
  mError = aError;
  return NS_OK;
}

/* attribute wstring message; */
NS_IMETHODIMP nsMsgProcessReport::GetMessage(PRUnichar * *aMessage)
{
  NS_ENSURE_ARG_POINTER(aMessage);
  *aMessage = ToNewUnicode(mMessage);
  return NS_OK;
}
NS_IMETHODIMP nsMsgProcessReport::SetMessage(const PRUnichar * aMessage)
{
  mMessage = aMessage;
  return NS_OK;
}

/* void Reset (); */
NS_IMETHODIMP nsMsgProcessReport::Reset()
{
  mProceeded = PR_FALSE;
  mError = NS_OK;
  mMessage.AssignWithConversion("");

  return NS_OK;
}


NS_IMPL_ISUPPORTS1(nsMsgSendReport, nsIMsgSendReport)

nsMsgSendReport::nsMsgSendReport()
{
  NS_INIT_ISUPPORTS();

  PRUint32 i;
  for (i = 0; i <= SEND_LAST_PROCESS; i ++)
    NS_NEWXPCOM(mProcessReport[i], nsMsgProcessReport);

  Reset(); 
}

nsMsgSendReport::~nsMsgSendReport()
{
  PRUint32 i;
  for (i = 0; i <= SEND_LAST_PROCESS; i ++)
    mProcessReport[i] = nsnull;
}

/* attribute long currentProcess; */
NS_IMETHODIMP nsMsgSendReport::GetCurrentProcess(PRInt32 *aCurrentProcess)
{
  NS_ENSURE_ARG_POINTER(aCurrentProcess);
  *aCurrentProcess = mCurrentProcess;
  return NS_OK;
}
NS_IMETHODIMP nsMsgSendReport::SetCurrentProcess(PRInt32 aCurrentProcess)
{
  if (aCurrentProcess < 0 || aCurrentProcess > SEND_LAST_PROCESS)
    return NS_ERROR_ILLEGAL_VALUE;

  mCurrentProcess = aCurrentProcess;
  if (mProcessReport[mCurrentProcess])
    mProcessReport[mCurrentProcess]->SetProceeded(PR_TRUE);
  
  return NS_OK;
}

/* attribute long deliveryMode; */
NS_IMETHODIMP nsMsgSendReport::GetDeliveryMode(PRInt32 *aDeliveryMode)
{
  NS_ENSURE_ARG_POINTER(aDeliveryMode);
  *aDeliveryMode = mDeliveryMode;
  return NS_OK;
}
NS_IMETHODIMP nsMsgSendReport::SetDeliveryMode(PRInt32 aDeliveryMode)
{
  mDeliveryMode = aDeliveryMode;
  return NS_OK;
}

/* void Reset (); */
NS_IMETHODIMP nsMsgSendReport::Reset()
{
  PRUint32 i;
  for (i = 0; i <= SEND_LAST_PROCESS; i ++)
    if (mProcessReport[i])
      mProcessReport[i]->Reset();

  mCurrentProcess = 0;
  mDeliveryMode = 0;
  mAlreadyDisplayReport = PR_FALSE;

  return NS_OK;
}

/* void setProceeded (in long process, in boolean proceeded); */
NS_IMETHODIMP nsMsgSendReport::SetProceeded(PRInt32 process, PRBool proceeded)
{
  if (process < process_Current || process > SEND_LAST_PROCESS)
    return NS_ERROR_ILLEGAL_VALUE;
    
  if (process == process_Current)
    process = mCurrentProcess;
    
  if (!mProcessReport[process])
    return NS_ERROR_NOT_INITIALIZED;

  return mProcessReport[process]->SetProceeded(proceeded);
}

/* void setError (in long process, in nsresult error, in boolean overwriteError); */
NS_IMETHODIMP nsMsgSendReport::SetError(PRInt32 process, nsresult newError, PRBool overwriteError)
{
  if (process < process_Current || process > SEND_LAST_PROCESS)
    return NS_ERROR_ILLEGAL_VALUE;
  
  if (process == process_Current)
    process = mCurrentProcess;
    
  if (!mProcessReport[process])
    return NS_ERROR_NOT_INITIALIZED;

  nsresult currError = NS_OK;
  mProcessReport[process]->GetError(&currError);
  if (overwriteError || currError == NS_OK)
    return mProcessReport[process]->SetError(newError);
  else
    return NS_OK;
}

/* void setMessage (in long process, in wstring message, in boolean overwriteMessage); */
NS_IMETHODIMP nsMsgSendReport::SetMessage(PRInt32 process, const PRUnichar *message, PRBool overwriteMessage)
{
  if (process < process_Current || process > SEND_LAST_PROCESS)
    return NS_ERROR_ILLEGAL_VALUE;
    
  if (process == process_Current)
    process = mCurrentProcess;
    
  if (!mProcessReport[process])
    return NS_ERROR_NOT_INITIALIZED;

  nsXPIDLString currMessage;
  mProcessReport[process]->GetMessage(getter_Copies(currMessage));
  if (overwriteMessage || (!currMessage) || (const char *)currMessage[0] == 0)
    return mProcessReport[process]->SetMessage(message);
  else
    return NS_OK;
}

/* nsIMsgProcessReport getProcessReport (in long process); */
NS_IMETHODIMP nsMsgSendReport::GetProcessReport(PRInt32 process, nsIMsgProcessReport **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  if (process < process_Current || process > SEND_LAST_PROCESS)
    return NS_ERROR_ILLEGAL_VALUE;

  if (process == process_Current)
    process = mCurrentProcess;
    
  *_retval = mProcessReport[process];
  NS_IF_ADDREF(*_retval);
  return NS_OK;
}

/* nsresult displayReport (in nsIPrompt prompt, in boolean showErrorOnly, in boolean dontShowReportTwice); */
NS_IMETHODIMP nsMsgSendReport::DisplayReport(nsIPrompt *prompt, PRBool showErrorOnly, PRBool dontShowReportTwice, nsresult *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult currError = NS_OK;
  mProcessReport[mCurrentProcess]->GetError(&currError);
  *_retval = currError;
  
  if (dontShowReportTwice && mAlreadyDisplayReport)
    return NS_OK;
  
  if (showErrorOnly && NS_SUCCEEDED(currError))
    return NS_OK;
  
  nsXPIDLString currMessage;
  mProcessReport[mCurrentProcess]->GetMessage(getter_Copies(currMessage));


  nsCOMPtr<nsIMsgStringService> composebundle (do_GetService(NS_MSG_COMPOSESTRINGSERVICE_CONTRACTID));
  if (!composebundle)
  {
    //TODO need to display a generic hardcoded message
    mAlreadyDisplayReport = PR_TRUE;
    return NS_OK;
  }

  nsXPIDLString dialogTitle;
  nsXPIDLString dialogMessage;

  if (NS_SUCCEEDED(currError))
  {
    //TODO display a success error message
    return NS_OK;
  }
  
  //Do we have an explanation of the error? if no, try to build one...
  if (currMessage.IsEmpty())
  {
      switch (currError)
      {
        case NS_ERROR_SEND_FAILED:
        case NS_ERROR_SEND_FAILED_BUT_NNTP_OK:
        case NS_MSG_FAILED_COPY_OPERATION:
        case NS_MSG_UNABLE_TO_SEND_LATER:
        case NS_MSG_UNABLE_TO_SAVE_DRAFT:
        case NS_MSG_UNABLE_TO_SAVE_TEMPLATE:
          //Ignore, don't need to repeat ourself.
          break;
        default:
          nsAutoString errorMsg;
          nsMsgBuildErrorMessageByID(currError, errorMsg);

          if (! errorMsg.IsEmpty())
            currMessage.Adopt(ToNewUnicode(errorMsg));
          break;
      }
  }
  
  if (mDeliveryMode == nsIMsgCompDeliverMode::Now || mDeliveryMode == nsIMsgCompDeliverMode::SendUnsent)
  {
    // SMTP is taking care of it's own error message and will return NS_ERROR_BUT_DONT_SHOW_ALERT as error code.
    // In that case, we must not show an alert ourself.
    if (currError == NS_ERROR_BUT_DONT_SHOW_ALERT)
    {
      mAlreadyDisplayReport = PR_TRUE;
      return NS_OK;
    }
  
    composebundle->GetStringByID(NS_MSG_SEND_ERROR_TITLE, getter_Copies(dialogTitle));

    PRInt32 preStrId = NS_ERROR_SEND_FAILED;
    PRBool askToGoBackToCompose = PR_FALSE;
    switch (mCurrentProcess)
    {
      case process_BuildMessage :
        preStrId = NS_ERROR_SEND_FAILED;
        askToGoBackToCompose = PR_FALSE;
        break;
      case process_NNTP :
        preStrId = NS_ERROR_SEND_FAILED;
        askToGoBackToCompose = PR_FALSE;
        break;
      case process_SMTP :
        PRBool nntpProceeded;
        mProcessReport[process_NNTP]->GetProceeded(&nntpProceeded);
        if (nntpProceeded)
          preStrId = NS_ERROR_SEND_FAILED_BUT_NNTP_OK;
        else
          preStrId = NS_ERROR_SEND_FAILED;
        askToGoBackToCompose = PR_FALSE;
        break;
      case process_Copy:
        preStrId = NS_MSG_FAILED_COPY_OPERATION;
        askToGoBackToCompose = (mDeliveryMode == nsIMsgCompDeliverMode::Now);
        break;
      case process_FCC:
        preStrId = NS_MSG_FAILED_COPY_OPERATION;
        askToGoBackToCompose = (mDeliveryMode == nsIMsgCompDeliverMode::Now);
        break;
    }
    composebundle->GetStringByID(preStrId, getter_Copies(dialogMessage));

    //Do we already have an error message?
    if (!askToGoBackToCompose && currMessage.IsEmpty())
    {
      //we don't have an error description but we can put a generic explanation
      composebundle->GetStringByID(NS_MSG_GENERIC_FAILURE_EXPLANATION, getter_Copies(currMessage));
    }
    
    if (!currMessage.IsEmpty())
    {
      nsAutoString temp((const PRUnichar *)dialogMessage);  // Because of bug 74726, we cannot use directly an XPIDLString

      //Don't need to repeat ourself!
      if (! currMessage.Equals(temp))
      {
        if (! dialogMessage.IsEmpty())
          temp.Append(NS_LITERAL_STRING("\n"));
        temp.Append(currMessage);
        dialogMessage.Adopt(ToNewUnicode(temp));
      }
    }
      
    if (askToGoBackToCompose)
    {
      PRBool oopsGiveMeBackTheComposeWindow = PR_TRUE;
      nsXPIDLString text1;
      composebundle->GetStringByID(NS_MSG_ASK_TO_COMEBACK_TO_COMPOSE, getter_Copies(text1));
      nsAutoString temp((const PRUnichar *)dialogMessage);  // Because of bug 74726, we cannot use directly an XPIDLString
      if (! dialogMessage.IsEmpty())
        temp.Append(NS_LITERAL_STRING("\n"));
      temp.Append(text1);
      dialogMessage.Adopt(ToNewUnicode(temp));
      nsMsgAskBooleanQuestionByString(prompt, dialogMessage, &oopsGiveMeBackTheComposeWindow, dialogTitle);
      if (!oopsGiveMeBackTheComposeWindow)
        *_retval = NS_OK;
    }
    else
      nsMsgDisplayMessageByString(prompt, dialogMessage, dialogTitle);
  }
  else
  {
    PRInt32 titleID;
    PRInt32 preStrId;

    switch (mDeliveryMode)
    {
      case nsIMsgCompDeliverMode::Later:
        titleID = NS_MSG_SENDLATER_ERROR_TITLE;
        preStrId = NS_MSG_UNABLE_TO_SEND_LATER;
        break;

      case nsIMsgCompDeliverMode::SaveAsDraft:
        titleID = NS_MSG_SAVE_DRAFT_TITLE;
        preStrId = NS_MSG_UNABLE_TO_SAVE_DRAFT;
        break;

      case nsIMsgCompDeliverMode::SaveAsTemplate:
        titleID = NS_MSG_SAVE_TEMPLATE_TITLE;
        preStrId = NS_MSG_UNABLE_TO_SAVE_TEMPLATE;
        break;

      default:
        /* This should never happend! */
        titleID = NS_MSG_SEND_ERROR_TITLE;
        preStrId = NS_ERROR_SEND_FAILED;
        break;
    }

    composebundle->GetStringByID(titleID, getter_Copies(dialogTitle));
    composebundle->GetStringByID(preStrId, getter_Copies(dialogMessage));

    //Do we have an error message...
    if (currMessage.IsEmpty())
    {
      //we don't have an error description but we can put a generic explanation
      composebundle->GetStringByID(NS_MSG_GENERIC_FAILURE_EXPLANATION, getter_Copies(currMessage));
    }

    if (!currMessage.IsEmpty())
    {
      nsAutoString temp((const PRUnichar *)dialogMessage);  // Because of bug 74726, we cannot use directly an XPIDLString
      if (! dialogMessage.IsEmpty())
        temp.Append(NS_LITERAL_STRING("\n"));
      temp.Append(currMessage);
      dialogMessage.Adopt(ToNewUnicode(temp));
    }

    nsMsgDisplayMessageByString(prompt, dialogMessage, dialogTitle);
  }
  
  mAlreadyDisplayReport = PR_TRUE;
  return NS_OK;
}

