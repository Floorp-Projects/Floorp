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
 *   Jean-Francois Ducarroz <ducarroz@netscape.com>
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

#include "nsMsgProgress.h"

#include "nsIBaseWindow.h"
#include "nsISupportsArray.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIComponentManager.h"
#include "nsNetError.h"

NS_IMPL_THREADSAFE_ADDREF(nsMsgProgress)
NS_IMPL_THREADSAFE_RELEASE(nsMsgProgress)

NS_INTERFACE_MAP_BEGIN(nsMsgProgress)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMsgStatusFeedback)
   NS_INTERFACE_MAP_ENTRY(nsIMsgProgress)
   NS_INTERFACE_MAP_ENTRY(nsIMsgStatusFeedback)
   NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
   NS_INTERFACE_MAP_ENTRY(nsIProgressEventSink)
NS_INTERFACE_MAP_END_THREADSAFE


nsMsgProgress::nsMsgProgress()
{
  m_closeProgress = PR_FALSE;
  m_processCanceled = PR_FALSE;
  m_pendingStateFlags = -1;
  m_pendingStateValue = 0;
}

nsMsgProgress::~nsMsgProgress()
{
  (void)ReleaseListeners();
}

/* void openProgressDialog (in nsIDOMWindowInternal parent, in nsIMsgWindow aMsgWindow,
  in string dialogURL, in nsISupports parameters); 
*/
NS_IMETHODIMP nsMsgProgress::OpenProgressDialog(nsIDOMWindowInternal *parent, nsIMsgWindow *aMsgWindow,
                                                const char *dialogURL,
                                                nsISupports *parameters)
{
  nsresult rv = NS_ERROR_FAILURE;
  
  m_msgWindow = aMsgWindow;
  if (m_msgWindow)
    m_msgWindow->SetStatusFeedback(this);
  if (m_dialog)
    return NS_ERROR_ALREADY_INITIALIZED;
  
  if (!dialogURL || !*dialogURL)
    return NS_ERROR_INVALID_ARG;

  if (parent)
  {
    // Set up window.arguments[0]...
    nsCOMPtr<nsISupportsArray> array;
    rv = NS_NewISupportsArray(getter_AddRefs(array));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISupportsInterfacePointer> ifptr =
      do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    ifptr->SetData(NS_STATIC_CAST(nsIMsgProgress*, this));
    ifptr->SetDataIID(&NS_GET_IID(nsIMsgProgress));

    array->AppendElement(ifptr);

    array->AppendElement(parameters);

    // Open the dialog.
    nsCOMPtr<nsIDOMWindow> newWindow;
    rv = parent->OpenDialog(NS_ConvertASCIItoUCS2(dialogURL),
                            NS_LITERAL_STRING("_blank"),
                            NS_LITERAL_STRING("chrome,titlebar,dependent"),
                            array, getter_AddRefs(newWindow));
  }

  return rv;
}

/* void closeProgressDialog (in boolean forceClose); */
NS_IMETHODIMP nsMsgProgress::CloseProgressDialog(PRBool forceClose)
{
  m_closeProgress = PR_TRUE;
  return OnStateChange(nsnull, nsnull, nsIWebProgressListener::STATE_STOP, forceClose ? NS_ERROR_FAILURE : NS_OK);
}

/* nsIPrompt GetPrompter (); */
NS_IMETHODIMP nsMsgProgress::GetPrompter(nsIPrompt **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;

  if (! m_closeProgress && m_dialog)
    return m_dialog->GetPrompter(_retval);
    
  return NS_ERROR_FAILURE;
}

/* attribute boolean processCanceledByUser; */
NS_IMETHODIMP nsMsgProgress::GetProcessCanceledByUser(PRBool *aProcessCanceledByUser)
{
  NS_ENSURE_ARG_POINTER(aProcessCanceledByUser);
  *aProcessCanceledByUser = m_processCanceled;
  return NS_OK;
}
NS_IMETHODIMP nsMsgProgress::SetProcessCanceledByUser(PRBool aProcessCanceledByUser)
{
  m_processCanceled = aProcessCanceledByUser;
  OnStateChange(nsnull, nsnull, nsIWebProgressListener::STATE_STOP, NS_BINDING_ABORTED);
  return NS_OK;
}

/* void RegisterListener (in nsIWebProgressListener listener); */
NS_IMETHODIMP nsMsgProgress::RegisterListener(nsIWebProgressListener * listener)
{
  nsresult rv = NS_OK;
  
  if (!listener) //Nothing to do with a null listener!
    return NS_OK;
  
  if (!m_listenerList)
    rv = NS_NewISupportsArray(getter_AddRefs(m_listenerList));
  
  if (NS_SUCCEEDED(rv) && m_listenerList)
  {
    m_listenerList->AppendElement(listener);
    if (m_closeProgress || m_processCanceled)
      listener->OnStateChange(nsnull, nsnull, nsIWebProgressListener::STATE_STOP, 0);
    else
    {
      listener->OnStatusChange(nsnull, nsnull, 0, m_pendingStatus.get());
      if (m_pendingStateFlags != -1)
        listener->OnStateChange(nsnull, nsnull, m_pendingStateFlags, m_pendingStateValue);
    }
  }
    
  return NS_OK;
}

/* void UnregisterListener (in nsIWebProgressListener listener); */
NS_IMETHODIMP nsMsgProgress::UnregisterListener(nsIWebProgressListener *listener)
{
  if (m_listenerList && listener)
    m_listenerList->RemoveElement(listener);
  
  return NS_OK;
}

/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long aStateFlags, in nsresult aStatus); */
NS_IMETHODIMP nsMsgProgress::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 aStateFlags, nsresult aStatus)
{
  nsresult rv;

  m_pendingStateFlags = aStateFlags;
  m_pendingStateValue = aStatus;
  
  if (m_listenerList)
  {
    PRUint32 count = 0;
    PRInt32 i;

    rv = m_listenerList->Count(&count);
    NS_ASSERTION(NS_SUCCEEDED(rv), "m_listenerList->Count() failed");
  
    nsCOMPtr<nsISupports> aSupports;
    nsCOMPtr<nsIWebProgressListener> aProgressListener;
    for (i = count - 1; i >= 0; i --)
    {
      m_listenerList->GetElementAt(i, getter_AddRefs(aSupports));
      aProgressListener = do_QueryInterface(aSupports);
      if (aProgressListener)
        aProgressListener->OnStateChange(aWebProgress, aRequest, aStateFlags, aStatus);
    }
  }
  
  if (aStateFlags == nsIWebProgressListener::STATE_STOP && m_msgWindow && NS_FAILED(aStatus))
    m_msgWindow->StopUrls();

  return NS_OK;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP nsMsgProgress::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
  nsresult rv;

  if (m_listenerList)
  {
    PRUint32 count = 0;
    PRInt32 i;

    rv = m_listenerList->Count(&count);
    NS_ASSERTION(NS_SUCCEEDED(rv), "m_listenerList->Count() failed");
  
    nsCOMPtr<nsISupports> aSupports;
    nsCOMPtr<nsIWebProgressListener> aProgressListener;
    for (i = count - 1; i >= 0; i --)
    {
      m_listenerList->GetElementAt(i, getter_AddRefs(aSupports));
      aProgressListener = do_QueryInterface(aSupports);
      if (aProgressListener)
        aProgressListener->OnProgressChange(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress);
    }
  }
  
  return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP nsMsgProgress::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location)
{
    return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP nsMsgProgress::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsresult aStatus, const PRUnichar *aMessage)
{
  nsresult rv;

  if (aMessage && *aMessage)
  m_pendingStatus = aMessage;
  if (m_listenerList)
  {
    PRUint32 count = 0;
    PRInt32 i;

    rv = m_listenerList->Count(&count);
    NS_ASSERTION(NS_SUCCEEDED(rv), "m_listenerList->Count() failed");
  
    nsCOMPtr<nsISupports> aSupports;
    nsCOMPtr<nsIWebProgressListener> aProgressListener;
    for (i = count - 1; i >= 0; i --)
    {
      m_listenerList->GetElementAt(i, getter_AddRefs(aSupports));
      aProgressListener = do_QueryInterface(aSupports);
      if (aProgressListener)
        aProgressListener->OnStatusChange(aWebProgress, aRequest, aStatus, aMessage);
    }
  }
  
  return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP nsMsgProgress::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
    return NS_OK;
}

nsresult nsMsgProgress::ReleaseListeners()
{
  nsresult rv = NS_OK;

  if (m_listenerList)
  {
    PRUint32 count;
    PRInt32 i;

    rv = m_listenerList->Count(&count);
    NS_ASSERTION(NS_SUCCEEDED(rv), "m_listenerList->Count() failed");
    if (NS_SUCCEEDED(rv))    
      for (i = count - 1; i >= 0; i --)
        m_listenerList->RemoveElementAt(i);
  }
  
  return rv;
}

NS_IMETHODIMP nsMsgProgress::ShowStatusString(const PRUnichar *status)
{
  return OnStatusChange(nsnull, nsnull, NS_OK, status);
}

/* void startMeteors (); */
NS_IMETHODIMP nsMsgProgress::StartMeteors()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void stopMeteors (); */
NS_IMETHODIMP nsMsgProgress::StopMeteors()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void showProgress (in long percent); */
NS_IMETHODIMP nsMsgProgress::ShowProgress(PRInt32 percent)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] void setDocShell (in nsIDocShell shell, in nsIDOMWindowInternal window); */
NS_IMETHODIMP nsMsgProgress::SetDocShell(nsIDocShell *shell, nsIDOMWindowInternal *window)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void closeWindow (); */
NS_IMETHODIMP nsMsgProgress::CloseWindow()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgProgress::SetMsgWindow(nsIMsgWindow *aMsgWindow)
{
  m_msgWindow = aMsgWindow;
  return NS_OK;
}

NS_IMETHODIMP nsMsgProgress::GetMsgWindow(nsIMsgWindow **aMsgWindow)
{
  NS_ENSURE_ARG_POINTER(aMsgWindow);
  NS_IF_ADDREF(*aMsgWindow = m_msgWindow);
  return NS_OK;
}


NS_IMETHODIMP nsMsgProgress::OnProgress(nsIRequest *request, nsISupports* ctxt, 
                                          PRUint32 aProgress, PRUint32 aProgressMax)
{
  // XXX: What should the nsIWebProgress be?
  return OnProgressChange(nsnull, request, aProgress, aProgressMax, 
                          aProgress /* current total progress */, aProgressMax /* max total progress */);
}

NS_IMETHODIMP nsMsgProgress::OnStatus(nsIRequest *request, nsISupports* ctxt, 
                                            nsresult aStatus, const PRUnichar* aStatusArg)
{
  nsresult rv;
  nsCOMPtr<nsIStringBundleService> sbs = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;
  nsXPIDLString str;
  rv = sbs->FormatStatusMessage(aStatus, aStatusArg, getter_Copies(str));
  if (NS_FAILED(rv)) return rv;
  nsAutoString msg(NS_STATIC_CAST(const PRUnichar*, str));
  return ShowStatusString(msg.get());
}
