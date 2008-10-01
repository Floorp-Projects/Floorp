/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*- */
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
 * The Original Code is worker threads.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com> (Original Author)
 *   Ben Turner <bent.mozilla@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsDOMWorkerPool.h"

// Interfaces
#include "nsIDocument.h"
#include "nsIDOMClassInfo.h"
#include "nsIJSContextStack.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIServiceManager.h"
#include "nsIThreadManager.h"
#include "nsIXPConnect.h"
#include "nsPIDOMWindow.h"

// Other includes
#include "nsAutoLock.h"
#include "nsContentUtils.h"
#include "nsDOMJSUtils.h"
#include "nsThreadUtils.h"

// DOMWorker includes
#include "nsDOMThreadService.h"
#include "nsDOMWorkerThread.h"

#define LOG(_args) PR_LOG(gDOMThreadsLog, PR_LOG_DEBUG, _args)

#define LOOP_OVER_WORKERS(_func, _args)                   \
  PR_BEGIN_MACRO                                          \
    PRUint32 workerCount = mWorkers.Length();             \
    for (PRUint32 i = 0; i < workerCount; i++) {          \
      mWorkers[i]-> _func _args ;                         \
    }                                                     \
  PR_END_MACRO

nsDOMWorkerPool::nsDOMWorkerPool(nsIDocument* aDocument)
: mParentGlobal(nsnull),
  mParentDocument(aDocument)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aDocument, "Must have a document!");
}

nsDOMWorkerPool::~nsDOMWorkerPool()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  LOOP_OVER_WORKERS(Cancel, ());

  nsDOMThreadService::get()->NoteDyingPool(this);

  if (mMonitor) {
    nsAutoMonitor::DestroyMonitor(mMonitor);
  }
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsDOMWorkerPool, nsIDOMWorkerPool,
                                               nsIClassInfo)

NS_IMPL_CI_INTERFACE_GETTER1(nsDOMWorkerPool, nsIDOMWorkerPool)

NS_IMPL_THREADSAFE_DOM_CI(nsDOMWorkerPool)

nsresult
nsDOMWorkerPool::Init()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsIScriptGlobalObject* globalObject =
    mParentDocument->GetScriptGlobalObject();
  NS_ENSURE_STATE(globalObject);

  nsCOMPtr<nsPIDOMWindow> domWindow(do_QueryInterface(globalObject));
  NS_ENSURE_TRUE(domWindow, NS_ERROR_NO_INTERFACE);

  nsPIDOMWindow* innerWindow = domWindow->IsOuterWindow() ?
                               domWindow->GetCurrentInnerWindow() :
                               domWindow.get();
  NS_ENSURE_STATE(innerWindow);

  nsCOMPtr<nsISupports> globalSupports(do_QueryInterface(innerWindow));
  NS_ENSURE_TRUE(globalSupports, NS_ERROR_NO_INTERFACE);

  // We don't want a strong ref, this guy owns us.
  mParentGlobal = globalSupports.get();

  mMonitor = nsAutoMonitor::NewMonitor("nsDOMWorkerPool::mMonitor");
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

nsresult
nsDOMWorkerPool::HandleMessage(const nsAString& aMessage,
                               nsDOMWorkerBase* aSource)
{
  nsCOMPtr<nsIDOMWorkerMessageListener> messageListener =
    nsDOMWorkerBase::GetMessageListener();
  if (!messageListener) {
    LOG(("Message received on a worker with no listener!"));
    return NS_OK;
  }

  nsCOMPtr<nsISupports> source;
  aSource->QueryInterface(NS_GET_IID(nsISupports), getter_AddRefs(source));
  NS_ASSERTION(source, "Impossible!");

  messageListener->OnMessage(aMessage, source);
  return NS_OK;
}

nsresult
nsDOMWorkerPool::DispatchMessage(nsIRunnable* aRunnable)
{
  // Can be called from many different threads!

  nsCOMPtr<nsIThread> mainThread(do_GetMainThread());
  NS_ENSURE_TRUE(mainThread, NS_ERROR_FAILURE);

  nsresult rv = mainThread->Dispatch(aRunnable, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
nsDOMWorkerPool::HandleError(nsIScriptError* aError,
                             nsDOMWorkerThread* aSource)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mErrorListener) {
    mErrorListener->OnError(aError,
                            NS_ISUPPORTS_CAST(nsIDOMWorkerThread*, aSource));
  }
}

void
nsDOMWorkerPool::NoteDyingWorker(nsDOMWorkerThread* aWorker)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  NS_ASSERTION(mWorkers.Contains(aWorker), "Worker from a different pool?!");
  mWorkers.RemoveElement(aWorker);
}

void
nsDOMWorkerPool::CancelWorkersForGlobal(nsIScriptGlobalObject* aGlobalObject)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsISupports> globalSupports(do_QueryInterface(aGlobalObject));
  NS_ASSERTION(globalSupports, "Null pointer?!");

  if (globalSupports == mParentGlobal) {
    LOOP_OVER_WORKERS(Cancel, ());
    mWorkers.Clear();
    if (IsSuspended()) {
      nsAutoMonitor mon(mMonitor);
      mon.NotifyAll();
    }
  }
}

void
nsDOMWorkerPool::SuspendWorkersForGlobal(nsIScriptGlobalObject* aGlobalObject)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsISupports> globalSupports(do_QueryInterface(aGlobalObject));
  NS_ASSERTION(globalSupports, "Null pointer?!");

  if (globalSupports == mParentGlobal) {
    LOOP_OVER_WORKERS(Suspend, ());
    Suspend();
  }
}

void
nsDOMWorkerPool::ResumeWorkersForGlobal(nsIScriptGlobalObject* aGlobalObject)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsISupports> globalSupports(do_QueryInterface(aGlobalObject));
  NS_ASSERTION(globalSupports, "Null pointer?!");

  if (globalSupports == mParentGlobal) {
    LOOP_OVER_WORKERS(Resume, ());
    Resume();

    nsAutoMonitor mon(mMonitor);
    mon.NotifyAll();
  }
}

nsIDocument*
nsDOMWorkerPool::ParentDocument()
{
  NS_ASSERTION(NS_IsMainThread(),
               "Don't touch the non-threadsafe document off the main thread!");
  return mParentDocument;
}

nsIScriptContext*
nsDOMWorkerPool::ScriptContext()
{
  NS_ASSERTION(NS_IsMainThread(),
               "Don't touch the non-threadsafe script context off the main "
               "thread!");
  return mParentDocument->GetScriptGlobalObject()->GetContext();
}

NS_IMETHODIMP
nsDOMWorkerPool::PostMessage(const nsAString& aMessage)
{
  nsresult rv = PostMessageInternal(aMessage);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerPool::SetMessageListener(nsIDOMWorkerMessageListener* aListener)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  nsDOMWorkerBase::SetMessageListener(aListener);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerPool::GetMessageListener(nsIDOMWorkerMessageListener** aListener)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  nsCOMPtr<nsIDOMWorkerMessageListener> listener = nsDOMWorkerBase::GetMessageListener();
  listener.forget(aListener);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerPool::SetErrorListener(nsIDOMWorkerErrorListener* aListener)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  mErrorListener = aListener;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerPool::GetErrorListener(nsIDOMWorkerErrorListener** aListener)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_IF_ADDREF(*aListener = mErrorListener);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerPool::CreateWorker(const nsAString& aFullScript,
                              nsIDOMWorkerThread** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  NS_ENSURE_ARG(!aFullScript.IsEmpty());
  NS_ENSURE_ARG_POINTER(_retval);

  nsRefPtr<nsDOMWorkerThread> worker =
    new nsDOMWorkerThread(this, aFullScript, PR_FALSE);
  NS_ENSURE_TRUE(worker, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = worker->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(!mWorkers.Contains(worker), "Um?!");
  mWorkers.AppendElement(worker);

  NS_ADDREF(*_retval = worker);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerPool::CreateWorkerFromURL(const nsAString& aScriptURL,
                                     nsIDOMWorkerThread** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  NS_ENSURE_ARG(!aScriptURL.IsEmpty());
  NS_ENSURE_ARG_POINTER(_retval);

  nsRefPtr<nsDOMWorkerThread> worker =
    new nsDOMWorkerThread(this, aScriptURL, PR_TRUE);
  NS_ENSURE_TRUE(worker, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = worker->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(!mWorkers.Contains(worker), "Um?!");
  mWorkers.AppendElement(worker);

  NS_ADDREF(*_retval = worker);
  return NS_OK;
}
