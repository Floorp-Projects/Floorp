/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintProgress.h"

#include "nsArray.h"
#include "nsIBaseWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIXULWindow.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsPIDOMWindow.h"

#if 0
NS_IMPL_ADDREF(nsPrintProgress)
NS_IMPL_RELEASE(nsPrintProgress)
#else
NS_IMETHODIMP_(MozExternalRefCountType) nsPrintProgress::AddRef(void)
{
  NS_PRECONDITION(int32_t(mRefCnt) >= 0, "illegal refcnt");
  nsrefcnt count;
  count = ++mRefCnt;
  //NS_LOG_ADDREF(this, count, "nsPrintProgress", sizeof(*this));
  return count;
}

NS_IMETHODIMP_(MozExternalRefCountType) nsPrintProgress::Release(void)
{
  nsrefcnt count;
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  count = --mRefCnt;
  //NS_LOG_RELEASE(this, count, "nsPrintProgress");
  if (0 == count) {
    mRefCnt = 1; /* stabilize */
    /* enable this to find non-threadsafe destructors: */
    /* NS_ASSERT_OWNINGTHREAD(nsPrintProgress); */
    delete this;
    return 0;
  }
  return count;
}

#endif

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
  m_pendingStateValue = NS_OK;
}

nsPrintProgress::~nsPrintProgress()
{
  (void)ReleaseListeners();
}

NS_IMETHODIMP nsPrintProgress::OpenProgressDialog(mozIDOMWindowProxy *parent,
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
    nsCOMPtr<nsIMutableArray> array = nsArray::Create();

    nsCOMPtr<nsISupportsInterfacePointer> ifptr =
      do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    ifptr->SetData(static_cast<nsIPrintProgress*>(this));
    ifptr->SetDataIID(&NS_GET_IID(nsIPrintProgress));

    array->AppendElement(ifptr, /*weak =*/ false);

    array->AppendElement(parameters, /*weak = */ false);

    // We will set the opener of the dialog to be the nsIDOMWindow for the
    // browser XUL window itself, as opposed to the content. That way, the
    // progress window has access to the opener.
    nsCOMPtr<nsPIDOMWindowOuter> pParentWindow = nsPIDOMWindowOuter::From(parent);
    NS_ENSURE_STATE(pParentWindow);

    nsCOMPtr<nsIDocShell> docShell = pParentWindow->GetDocShell();
    NS_ENSURE_STATE(docShell);

    nsCOMPtr<nsIDocShellTreeOwner> owner;
    docShell->GetTreeOwner(getter_AddRefs(owner));

    nsCOMPtr<nsIXULWindow> ownerXULWindow = do_GetInterface(owner);
    nsCOMPtr<mozIDOMWindowProxy> ownerWindow = do_GetInterface(ownerXULWindow);
    NS_ENSURE_STATE(ownerWindow);

    nsCOMPtr<nsPIDOMWindowOuter> piOwnerWindow = nsPIDOMWindowOuter::From(ownerWindow);

    // Open the dialog.
    nsCOMPtr<nsPIDOMWindowOuter> newWindow;
    rv = piOwnerWindow->OpenDialog(NS_ConvertASCIItoUTF16(dialogURL),
                                   NS_LITERAL_STRING("_blank"),
                                   NS_LITERAL_STRING("chrome,titlebar,dependent,centerscreen"),
                                   array, getter_AddRefs(newWindow));
  }

  return rv;
}

NS_IMETHODIMP nsPrintProgress::CloseProgressDialog(bool forceClose)
{
  m_closeProgress = true;
  // XXX Casting from bool to nsresult
  return OnStateChange(nullptr, nullptr, nsIWebProgressListener::STATE_STOP,
                       static_cast<nsresult>(forceClose));
}

NS_IMETHODIMP nsPrintProgress::GetPrompter(nsIPrompt **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nullptr;

  if (! m_closeProgress && m_dialog) {
    nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryInterface(m_dialog);
    MOZ_ASSERT(window);
    return window->GetPrompter(_retval);
  }
    
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsPrintProgress::GetProcessCanceledByUser(bool *aProcessCanceledByUser)
{
  NS_ENSURE_ARG_POINTER(aProcessCanceledByUser);
  *aProcessCanceledByUser = m_processCanceled;
  return NS_OK;
}
NS_IMETHODIMP nsPrintProgress::SetProcessCanceledByUser(bool aProcessCanceledByUser)
{
  m_processCanceled = aProcessCanceledByUser;
  OnStateChange(nullptr, nullptr, nsIWebProgressListener::STATE_STOP, NS_OK);
  return NS_OK;
}

NS_IMETHODIMP nsPrintProgress::RegisterListener(nsIWebProgressListener * listener)
{
  if (!listener) //Nothing to do with a null listener!
    return NS_OK;
  
  m_listenerList.AppendObject(listener);
  if (m_closeProgress || m_processCanceled)
    listener->OnStateChange(nullptr, nullptr,
                            nsIWebProgressListener::STATE_STOP, NS_OK);
  else
  {
    listener->OnStatusChange(nullptr, nullptr, NS_OK, m_pendingStatus.get());
    if (m_pendingStateFlags != -1)
      listener->OnStateChange(nullptr, nullptr, m_pendingStateFlags, m_pendingStateValue);
  }
    
  return NS_OK;
}

NS_IMETHODIMP nsPrintProgress::UnregisterListener(nsIWebProgressListener *listener)
{
  if (listener)
    m_listenerList.RemoveObject(listener);
  
  return NS_OK;
}

NS_IMETHODIMP nsPrintProgress::DoneIniting()
{
  if (m_observer) {
    m_observer->Observe(nullptr, nullptr, nullptr);
  }
  return NS_OK;
}

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

NS_IMETHODIMP nsPrintProgress::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location, uint32_t aFlags)
{
    return NS_OK;
}

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

NS_IMETHODIMP nsPrintProgress::StartMeteors()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPrintProgress::StopMeteors()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPrintProgress::ShowProgress(int32_t percent)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPrintProgress::SetDocShell(nsIDocShell *shell, mozIDOMWindowProxy *window)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPrintProgress::CloseWindow()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
