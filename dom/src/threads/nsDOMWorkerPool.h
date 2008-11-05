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

#ifndef __NSDOMWORKERPOOL_H__
#define __NSDOMWORKERPOOL_H__

// Bases
#include "nsDOMWorkerBase.h"
#include "nsIClassInfo.h"
#include "nsIDOMThreads.h"

// Other includes
#include "jsapi.h"
#include "nsStringGlue.h"
#include "nsTPtrArray.h"
#include "prmon.h"

class nsDOMWorkerThread;
class nsIDocument;
class nsIScriptContext;
class nsIScriptError;
class nsIScriptGlobalObject;

/**
 * The pool is almost always touched only on the main thread.
 */
class nsDOMWorkerPool : public nsDOMWorkerBase,
                        public nsIDOMWorkerPool,
                        public nsIClassInfo
{
  friend class nsDOMThreadService;
  friend class nsDOMWorkerFunctions;
  friend class nsDOMWorkerPoolWeakRef;
  friend class nsDOMWorkerScriptLoader;
  friend class nsDOMWorkerStreamObserver;
  friend class nsDOMWorkerThread;
  friend class nsReportErrorRunnable;
  friend JSBool DOMWorkerOperationCallback(JSContext* aCx);

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMWORKERPOOL
  NS_DECL_NSICLASSINFO

  nsDOMWorkerPool(nsIDocument* aDocument);

  // For nsDOMWorkerBase
  virtual nsDOMWorkerPool* Pool() {
    return this;
  }

  nsIDocument* ParentDocument();
  nsIScriptContext* ScriptContext();

private:
  virtual ~nsDOMWorkerPool();

  nsresult Init();

  // For nsDOMWorkerBase
  virtual nsresult HandleMessage(const nsAString& aMessage,
                                 nsDOMWorkerBase* aSourceThread);

  // For nsDOMWorkerBase
  virtual nsresult DispatchMessage(nsIRunnable* aRunnable);

  void HandleError(nsIScriptError* aError,
                   nsDOMWorkerThread* aSource);

  void NoteDyingWorker(nsDOMWorkerThread* aWorker);

  void CancelWorkersForGlobal(nsIScriptGlobalObject* aGlobalObject);
  void SuspendWorkersForGlobal(nsIScriptGlobalObject* aGlobalObject);
  void ResumeWorkersForGlobal(nsIScriptGlobalObject* aGlobalObject);

  PRMonitor* Monitor() {
    return mMonitor;
  }

  // Weak reference to the window that created and owns this pool.
  nsISupports* mParentGlobal;

  // Weak reference to the document that created this pool.
  nsIDocument* mParentDocument;

  // Weak array of workers. The idea is that workers can be garbage collected
  // independently of the owning pool and other workers.
  nsTPtrArray<nsDOMWorkerThread> mWorkers;

  // An error handler function, may be null.
  nsCOMPtr<nsIDOMWorkerErrorListener> mErrorListener;

  // Monitor for suspending and resuming workers.
  PRMonitor* mMonitor;
};

#endif /* __NSDOMWORKERPOOL_H__ */
