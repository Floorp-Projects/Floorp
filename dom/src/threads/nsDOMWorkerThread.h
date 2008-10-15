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

#ifndef __NSDOMWORKERTHREAD_H__
#define __NSDOMWORKERTHREAD_H__

// Bases
#include "nsDOMWorkerBase.h"
#include "nsIClassInfo.h"
#include "nsIDOMThreads.h"

// Other includes
#include "jsapi.h"
#include "nsAutoJSObjectHolder.h"
#include "nsCOMPtr.h"
#include "nsStringGlue.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "prclist.h"
#include "prlock.h"

// DOMWorker includes
#include "nsDOMThreadService.h"

// Macro to generate nsIClassInfo methods for these threadsafe DOM classes 
#define NS_IMPL_THREADSAFE_DOM_CI_GETINTERFACES(_class)                       \
NS_IMETHODIMP                                                                 \
_class::GetInterfaces(PRUint32* _count, nsIID*** _array)                      \
{                                                                             \
  return NS_CI_INTERFACE_GETTER_NAME(_class)(_count, _array);                 \
}                                                                             \

#define NS_IMPL_THREADSAFE_DOM_CI_ALL_THE_REST(_class)                        \
NS_IMETHODIMP                                                                 \
_class::GetHelperForLanguage(PRUint32 _language, nsISupports** _retval)       \
{                                                                             \
  *_retval = nsnull;                                                          \
  return NS_OK;                                                               \
}                                                                             \
                                                                              \
NS_IMETHODIMP                                                                 \
_class::GetContractID(char** _contractID)                                     \
{                                                                             \
  *_contractID = nsnull;                                                      \
  return NS_OK;                                                               \
}                                                                             \
                                                                              \
NS_IMETHODIMP                                                                 \
_class::GetClassDescription(char** _classDescription)                         \
{                                                                             \
  *_classDescription = nsnull;                                                \
  return NS_OK;                                                               \
}                                                                             \
                                                                              \
NS_IMETHODIMP                                                                 \
_class::GetClassID(nsCID** _classID)                                          \
{                                                                             \
  *_classID = nsnull;                                                         \
  return NS_OK;                                                               \
}                                                                             \
                                                                              \
NS_IMETHODIMP                                                                 \
_class::GetImplementationLanguage(PRUint32* _language)                        \
{                                                                             \
  *_language = nsIProgrammingLanguage::CPLUSPLUS;                             \
  return NS_OK;                                                               \
}                                                                             \
                                                                              \
NS_IMETHODIMP                                                                 \
_class::GetFlags(PRUint32* _flags)                                            \
{                                                                             \
  *_flags = nsIClassInfo::THREADSAFE | nsIClassInfo::DOM_OBJECT;              \
  return NS_OK;                                                               \
}                                                                             \
                                                                              \
NS_IMETHODIMP                                                                 \
_class::GetClassIDNoAlloc(nsCID* _classIDNoAlloc)                             \
{                                                                             \
  return NS_ERROR_NOT_AVAILABLE;                                              \
}

#define NS_IMPL_THREADSAFE_DOM_CI(_class)                                     \
NS_IMPL_THREADSAFE_DOM_CI_GETINTERFACES(_class)                               \
NS_IMPL_THREADSAFE_DOM_CI_ALL_THE_REST(_class)

class nsDOMWorkerPool;
class nsDOMWorkerScriptLoader;
class nsDOMWorkerTimeout;
class nsDOMWorkerXHR;

class nsDOMWorkerThread : public nsDOMWorkerBase,
                          public nsIDOMWorkerThread,
                          public nsIClassInfo
{
  friend class nsDOMCreateJSContextRunnable;
  friend class nsDOMWorkerFunctions;
  friend class nsDOMWorkerPool;
  friend class nsDOMWorkerRunnable;
  friend class nsDOMWorkerScriptLoader;
  friend class nsDOMWorkerTimeout;
  friend class nsDOMWorkerXHR;

  friend JSBool DOMWorkerOperationCallback(JSContext* aCx);

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMWORKERTHREAD
  NS_DECL_NSICLASSINFO

  nsDOMWorkerThread(nsDOMWorkerPool* aPool,
                    const nsAString& aSource,
                    PRBool aSourceIsURL);

  virtual nsDOMWorkerPool* Pool() {
    NS_ASSERTION(!IsCanceled(), "Don't touch Pool after we've been canceled!");
    return mPool;
  }

private:
  virtual ~nsDOMWorkerThread();

  nsresult Init();

  // For nsDOMWorkerBase
  virtual nsresult HandleMessage(const nsAString& aMessage,
                                 nsDOMWorkerBase* aSourceThread);

  // For nsDOMWorkerBase
  virtual nsresult DispatchMessage(nsIRunnable* aRunnable);

  virtual void Cancel();
  virtual void Suspend();
  virtual void Resume();

  PRBool SetGlobalForContext(JSContext* aCx);
  PRBool CompileGlobalObject(JSContext* aCx);

  inline nsDOMWorkerTimeout* FirstTimeout();
  inline nsDOMWorkerTimeout* NextTimeout(nsDOMWorkerTimeout* aTimeout);

  PRBool AddTimeout(nsDOMWorkerTimeout* aTimeout);
  void RemoveTimeout(nsDOMWorkerTimeout* aTimeout);
  void ClearTimeouts();
  void CancelTimeout(PRUint32 aId);
  void SuspendTimeouts();
  void ResumeTimeouts();

  void CancelScriptLoaders();

  PRBool AddXHR(nsDOMWorkerXHR* aXHR);
  void RemoveXHR(nsDOMWorkerXHR* aXHR);
  void CancelXHRs();

  PRLock* Lock() {
    return mLock;
  }

  nsDOMWorkerPool* mPool;
  nsString mSource;
  nsString mSourceURL;

  nsAutoJSObjectHolder mGlobal;
  PRBool mCompiled;

  PRUint32 mCallbackCount;

  PRUint32 mNextTimeoutId;

  PRLock* mLock;
  PRCList mTimeouts;

  nsTArray<nsDOMWorkerScriptLoader*> mScriptLoaders;
  nsTArray<nsDOMWorkerXHR*> mXHRs;
};

#endif /* __NSDOMWORKERTHREAD_H__ */
