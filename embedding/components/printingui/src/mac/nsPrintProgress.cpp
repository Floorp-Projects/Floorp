/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintProgress.h"

#include "nsIBaseWindow.h"
#include "nsISupportsArray.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIComponentManager.h"

NS_IMPL_THREADSAFE_ADDREF(nsPrintProgress)
NS_IMPL_THREADSAFE_RELEASE(nsPrintProgress)

NS_INTERFACE_MAP_BEGIN(nsPrintProgress)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPrintStatusFeedback)
   NS_INTERFACE_MAP_ENTRY(nsIPrintProgress)
   NS_INTERFACE_MAP_ENTRY(nsIPrintStatusFeedback)
   NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
NS_INTERFACE_MAP_END_THREADSAFE


nsPrintProgress::nsPrintProgress()
{
  m_closeProgress = false;
  m_processCanceled = false;
  m_pendingStateFlags = -1;
  m_pendingStateValue = 0;
}

nsPrintProgress::~nsPrintProgress()
{
  (void)ReleaseListeners();
}

/* void openProgressDialog (in nsIDOMWindow parent, in string dialogURL, in nsISupports parameters); */
NS_IMETHODIMP nsPrintProgress::OpenProgressDialog(nsIDOMWindow *parent,
                                                  const char *dialogURL,
                                                  nsISupports *parameters, 
                                                  nsIObserver *openDialogObserver,
                                                  bool *notifyOnOpen)
{
  m_observer = openDialogObserver;

  nsresult rv = NS_ERROR_FAILURE;
  
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
    
    ifptr->SetData(static_cast<nsIPrintProgress*>(this));
    ifptr->SetDataIID(&NS_GET_IID(nsIPrintProgress));

    array->AppendElement(ifptr);

    array->AppendElement(parameters);

    // Open the dialog.
    nsCOMPtr<nsIDOMWindow> newWindow;
    rv = parent->OpenDialog(NS_ConvertASCIItoUTF16(dialogURL),
                            NS_LITERAL_STRING("_blank"),
                            NS_LITERAL_STRING("chrome,titlebar,dependent,centerscreen"),
                            array, getter_AddRefs(newWindow));
    if (NS_SUCCEEDED(rv)) {
      *notifyOnOpen = true;
    }
  }

  return rv;
}

/* void closeProgressDialog (in boolean forceClose); */
NS_IMETHODIMP nsPrintProgress::CloseProgressDialog(bool forceClose)
{
  m_closeProgress = true;
  return OnStateChange(nullptr, nullptr, nsIWebProgressListener::STATE_STOP, forceClose);
}

/* nsIPrompt GetPrompter (); */
NS_IMETHODIMP nsPrintProgress::GetPrompter(nsIPrompt **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nullptr;

  if (! m_closeProgress && m_dialog)
    return m_dialog->GetPrompter(_retval);
    
  return NS_ERROR_FAILURE;
}

/* attribute boolean processCanceledByUser; */
NS_IMETHODIMP nsPrintProgress::GetProcessCanceledByUser(bool *aProcessCanceledByUser)
{
  NS_ENSURE_ARG_POINTER(aProcessCanceledByUser);
  *aProcessCanceledByUser = m_processCanceled;
  return NS_OK;
}
NS_IMETHODIMP nsPrintProgress::SetProcessCanceledByUser(bool aProcessCanceledByUser)
{
  m_processCanceled = aProcessCanceledByUser;
  OnStateChange(nullptr, nullptr, nsIWebProgressListener::STATE_STOP, false);
  return NS_OK;
}

/* void RegisterListener (in nsIWebProgressListener listener); */
NS_IMETHODIMP nsPrintProgress::RegisterListener(nsIWebProgressListener * listener)
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
      listener->OnStateChange(nullptr, nullptr, nsIWebProgressListener::STATE_STOP, 0);
    else
    {
      listener->OnStatusChange(nullptr, nullptr, 0, m_pendingStatus.get());
      if (m_pendingStateFlags != -1)
        listener->OnStateChange(nullptr, nullptr, m_pendingStateFlags, m_pendingStateValue);
    }
  }
    
  return NS_OK;
}

/* void UnregisterListener (in nsIWebProgressListener listener); */
NS_IMETHODIMP nsPrintProgress::UnregisterListener(nsIWebProgressListener *listener)
{
  if (m_listenerList && listener)
    m_listenerList->RemoveElement(listener);
  
  return NS_OK;
}

/* void doneIniting (); */
NS_IMETHODIMP nsPrintProgress::DoneIniting()
{
  if (m_observer) {
    m_observer->Observe(nullptr, nullptr, nullptr);
  }
  return NS_OK;
}

/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long aStateFlags, in nsresult aStatus); */
NS_IMETHODIMP nsPrintProgress::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 aStateFlags, nsresult aStatus)
{
  nsresult rv = NS_OK;

  m_pendingStateFlags = aStateFlags;
  m_pendingStateValue = aStatus;
  
  if (m_listenerList)
  {
    PRUint32 count;
    PRInt32 i;

    rv = m_listenerList->Count(&count);
    NS_ASSERTION(NS_SUCCEEDED(rv), "m_listenerList->Count() failed");
    if (NS_FAILED(rv))
      return rv;
  
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
  
  return rv;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP nsPrintProgress::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
  nsresult rv = NS_OK;

  if (m_listenerList)
  {
    PRUint32 count;
    PRInt32 i;

    rv = m_listenerList->Count(&count);
    NS_ASSERTION(NS_SUCCEEDED(rv), "m_listenerList->Count() failed");
    if (NS_FAILED(rv))
      return rv;
  
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
  
  return rv;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location, in unsigned long aFlags); */
NS_IMETHODIMP nsPrintProgress::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location, PRUint32 aFlags)
{
    return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP nsPrintProgress::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsresult aStatus, const PRUnichar *aMessage)
{
  nsresult rv = NS_OK;

  if (aMessage && *aMessage)
  m_pendingStatus = aMessage;
  if (m_listenerList)
  {
    PRUint32 count;
    PRInt32 i;

    rv = m_listenerList->Count(&count);
    NS_ASSERTION(NS_SUCCEEDED(rv), "m_listenerList->Count() failed");
    if (NS_FAILED(rv))
      return rv;
  
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
  
  return rv;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP nsPrintProgress::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
    return NS_OK;
}

nsresult nsPrintProgress::ReleaseListeners()
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

NS_IMETHODIMP nsPrintProgress::ShowStatusString(const PRUnichar *status)
{
  return OnStatusChange(nullptr, nullptr, NS_OK, status);
}

/* void startMeteors (); */
NS_IMETHODIMP nsPrintProgress::StartMeteors()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void stopMeteors (); */
NS_IMETHODIMP nsPrintProgress::StopMeteors()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void showProgress (in long percent); */
NS_IMETHODIMP nsPrintProgress::ShowProgress(PRInt32 percent)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] void setDocShell (in nsIDocShell shell, in nsIDOMWindow window); */
NS_IMETHODIMP nsPrintProgress::SetDocShell(nsIDocShell *shell, nsIDOMWindow *window)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void closeWindow (); */
NS_IMETHODIMP nsPrintProgress::CloseWindow()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

