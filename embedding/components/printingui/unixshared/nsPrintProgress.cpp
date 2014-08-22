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

NS_IMPL_ADDREF(nsPrintProgress)
NS_IMPL_RELEASE(nsPrintProgress)

NS_INTERFACE_MAP_BEGIN(nsPrintProgress)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPrintStatusFeedback)
   NS_INTERFACE_MAP_ENTRY(nsIPrintProgress)
   NS_INTERFACE_MAP_ENTRY(nsIPrintStatusFeedback)
   NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
NS_INTERFACE_MAP_END_THREADSAFE


nsPrintProgress::nsPrintProgress(nsIPrintSettings* aPrintSettings)
{
  m_closeProgress = false;
  m_processCanceled = false;
  m_pendingStateFlags = -1;
  m_pendingStateValue = NS_OK;
  m_PrintSetting = aPrintSettings;
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
  *notifyOnOpen = true;
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
  }

  return rv;
}

/* void closeProgressDialog (in boolean forceClose); */
NS_IMETHODIMP nsPrintProgress::CloseProgressDialog(bool forceClose)
{
  m_closeProgress = true;
  // XXX Invalid cast of bool to nsresult (bug 778106)
  return OnStateChange(nullptr, nullptr, nsIWebProgressListener::STATE_STOP,
                       (nsresult)forceClose);
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
  if(m_PrintSetting)
    m_PrintSetting->SetIsCancelled(true);
  m_processCanceled = aProcessCanceledByUser;
  OnStateChange(nullptr, nullptr, nsIWebProgressListener::STATE_STOP, NS_OK);
  return NS_OK;
}

/* void RegisterListener (in nsIWebProgressListener listener); */
NS_IMETHODIMP nsPrintProgress::RegisterListener(nsIWebProgressListener * listener)
{
  if (!listener) //Nothing to do with a null listener!
    return NS_OK;
  
  m_listenerList.AppendObject(listener);
  if (m_closeProgress || m_processCanceled)
    listener->OnStateChange(nullptr, nullptr, nsIWebProgressListener::STATE_STOP, NS_OK);
  else
  {
    listener->OnStatusChange(nullptr, nullptr, NS_OK, m_pendingStatus.get());
    if (m_pendingStateFlags != -1)
      listener->OnStateChange(nullptr, nullptr, m_pendingStateFlags, m_pendingStateValue);
  }
    
  return NS_OK;
}

/* void UnregisterListener (in nsIWebProgressListener listener); */
NS_IMETHODIMP nsPrintProgress::UnregisterListener(nsIWebProgressListener *listener)
{
  if (listener)
    m_listenerList.RemoveObject(listener);
  
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
NS_IMETHODIMP nsPrintProgress::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, uint32_t aStateFlags, nsresult aStatus)
{
  m_pendingStateFlags = aStateFlags;
  m_pendingStateValue = aStatus;
  
  uint32_t count = m_listenerList.Count();
  for (uint32_t i = count - 1; i < count; i --)
  {
    nsCOMPtr<nsIWebProgressListener> progressListener = m_listenerList.SafeObjectAt(i);
    if (progressListener)
      progressListener->OnStateChange(aWebProgress, aRequest, aStateFlags, aStatus);
  }
  
  return NS_OK;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP nsPrintProgress::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, int32_t aCurSelfProgress, int32_t aMaxSelfProgress, int32_t aCurTotalProgress, int32_t aMaxTotalProgress)
{
  uint32_t count = m_listenerList.Count();
  for (uint32_t i = count - 1; i < count; i --)
  {
    nsCOMPtr<nsIWebProgressListener> progressListener = m_listenerList.SafeObjectAt(i);
    if (progressListener)
      progressListener->OnProgressChange(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress);
  }
  
  return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location, in unsigned long aFlags); */
NS_IMETHODIMP nsPrintProgress::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location, uint32_t aFlags)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP nsPrintProgress::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsresult aStatus, const char16_t *aMessage)
{
  if (aMessage && *aMessage)
  m_pendingStatus = aMessage;

  uint32_t count = m_listenerList.Count();
  for (uint32_t i = count - 1; i < count; i --)
  {
    nsCOMPtr<nsIWebProgressListener> progressListener = m_listenerList.SafeObjectAt(i);
    if (progressListener)
      progressListener->OnStatusChange(aWebProgress, aRequest, aStatus, aMessage);
  }
  
  return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP nsPrintProgress::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, uint32_t state)
{
    return NS_OK;
}

nsresult nsPrintProgress::ReleaseListeners()
{
  m_listenerList.Clear();
  
  return NS_OK;
}

NS_IMETHODIMP nsPrintProgress::ShowStatusString(const char16_t *status)
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
NS_IMETHODIMP nsPrintProgress::ShowProgress(int32_t percent)
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

