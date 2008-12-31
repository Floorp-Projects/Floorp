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
#include "nsDOMWorker.h"

#define LOG(_args) PR_LOG(gDOMThreadsLog, PR_LOG_DEBUG, _args)

nsDOMWorkerPool::nsDOMWorkerPool(nsIScriptGlobalObject* aGlobalObject,
                                 nsIDocument* aDocument)
: mParentGlobal(aGlobalObject),
  mParentDocument(aDocument),
  mMonitor(nsnull),
  mCanceled(PR_FALSE),
  mSuspended(PR_FALSE)
{
  NS_ASSERTION(aGlobalObject, "Must have a global object!");
  NS_ASSERTION(aDocument, "Must have a document!");
}

nsDOMWorkerPool::~nsDOMWorkerPool()
{
  if (mMonitor) {
    nsAutoMonitor::DestroyMonitor(mMonitor);
  }
}

NS_IMPL_THREADSAFE_ADDREF(nsDOMWorkerPool)
NS_IMPL_THREADSAFE_RELEASE(nsDOMWorkerPool)

nsresult
nsDOMWorkerPool::Init()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  mMonitor = nsAutoMonitor::NewMonitor("nsDOMWorkerPool::mMonitor");
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

nsresult
nsDOMWorkerPool::NoteWorker(nsDOMWorker* aWorker)
{
  NS_ASSERTION(aWorker, "Null pointer!");

  PRBool suspendWorker;

  {
    nsAutoMonitor mon(mMonitor);

    if (mCanceled) {
      return NS_ERROR_ABORT;
    }

    nsDOMWorker** newWorker = mWorkers.AppendElement(aWorker);
    NS_ENSURE_TRUE(newWorker, NS_ERROR_OUT_OF_MEMORY);

    suspendWorker = mSuspended;
  }

  if (suspendWorker) {
    aWorker->Suspend();
  }

  return NS_OK;
}

void
nsDOMWorkerPool::NoteDyingWorker(nsDOMWorker* aWorker)
{
  NS_ASSERTION(aWorker, "Null pointer!");

  PRBool removeFromThreadService = PR_FALSE;

  {
    nsAutoMonitor mon(mMonitor);

    NS_ASSERTION(mWorkers.Contains(aWorker), "Worker from a different pool?!");
    mWorkers.RemoveElement(aWorker);

    if (!mCanceled && !mWorkers.Length()) {
      removeFromThreadService = PR_TRUE;
    }
  }

  if (removeFromThreadService) {
    nsRefPtr<nsDOMWorkerPool> kungFuDeathGrip(this);
    nsDOMThreadService::get()->NoteEmptyPool(this);
  }
}

void
nsDOMWorkerPool::GetWorkers(nsTArray<nsDOMWorker*>& aArray)
{
  aArray.Clear();

  nsAutoMonitor mon(mMonitor);
#ifdef DEBUG
  nsDOMWorker** newWorkers =
#endif
  aArray.AppendElements(mWorkers);
  NS_WARN_IF_FALSE(newWorkers, "Out of memory!");
}

void
nsDOMWorkerPool::Cancel()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!mCanceled, "Canceled more than once!");

  {
    nsAutoMonitor mon(mMonitor);

    mCanceled = PR_TRUE;

    nsAutoTArray<nsDOMWorker*, 10> workers;
    GetWorkers(workers);

    PRUint32 count = workers.Length();
    if (count) {
      for (PRUint32 index = 0; index < count; index++) {
        workers[index]->Cancel();
      }
      mon.NotifyAll();
    }
  }

  mParentGlobal = nsnull;
  mParentDocument = nsnull;
}

void
nsDOMWorkerPool::Suspend()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsAutoTArray<nsDOMWorker*, 10> workers;
  {
    nsAutoMonitor mon(mMonitor);

    NS_ASSERTION(!mSuspended, "Suspended more than once!");
    mSuspended = PR_TRUE;

    GetWorkers(workers);
  }

  PRUint32 count = workers.Length();
  for (PRUint32 index = 0; index < count; index++) {
    workers[index]->Suspend();
  }
}

void
nsDOMWorkerPool::Resume()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsAutoTArray<nsDOMWorker*, 10> workers;
  {
    nsAutoMonitor mon(mMonitor);

    NS_ASSERTION(mSuspended, "Not suspended!");
    mSuspended = PR_FALSE;

    GetWorkers(workers);
  }

  PRUint32 count = workers.Length();
  if (count) {
    for (PRUint32 index = 0; index < count; index++) {
      workers[index]->Resume();
    }
    nsAutoMonitor mon(mMonitor);
    mon.NotifyAll();
  }
}
