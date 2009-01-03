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
 *   Ben Turner <bent.mozilla@gmail.com> (Original Author)
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

#ifndef __NSDOMWORKERSCRIPTLOADER_H__
#define __NSDOMWORKERSCRIPTLOADER_H__

// Bases
#include "nsIRunnable.h"
#include "nsIStreamLoader.h"

// Interfaces
#include "nsIChannel.h"
#include "nsIURI.h"

// Other includes
#include "jsapi.h"
#include "nsAutoPtr.h"
#include "nsAutoJSValHolder.h"
#include "nsCOMPtr.h"
#include "nsStringGlue.h"
#include "nsTArray.h"
#include "prlock.h"

#include "nsDOMWorker.h"

class nsIThread;

/**
 * This class takes a list of script URLs, downloads the scripts, compiles the
 * scripts, and then finally executes them. Due to platform limitations all
 * network operations must happen on the main thread so this object sends events
 * back and forth from the worker thread to the main thread. The flow goes like
 * this:
 *
 *  1. (Worker thread) nsDOMWorkerScriptLoader created.
 *  2. (Worker thread) LoadScript(s) called. Some simple argument validation is
 *                     performed (currently limited to ensuring that all
 *                     arguments are strings). nsDOMWorkerScriptLoader is then
 *                     dispatched to the main thread.
 *  3. (Main thread)   Arguments validated as URIs, security checks performed,
 *                     content policy consulted. Network loads begin.
 *  4. (Necko thread)  Necko stuff!
 *  5. (Main thread)   Completed downloads are packaged in a ScriptCompiler
 *                     runnable and sent to the worker thread.
 *  6. (Worker thread) ScriptCompiler runnables are processed (i.e. their
 *                     scripts are compiled) in the order in which the necko
 *                     downloads completed.
 *  7. (Worker thread) After all loads complete and all compilation succeeds
 *                     the scripts are executed in the order that the URLs were
 *                     given to LoadScript(s).
 *
 * Currently if *anything* after 2 fails then we cancel any pending loads and
 * bail out entirely.
 */
class nsDOMWorkerScriptLoader : public nsDOMWorkerFeature,
                                public nsIRunnable,
                                public nsIStreamLoaderObserver
{
  friend class AutoSuspendWorkerEvents;
  friend class ScriptLoaderRunnable;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSISTREAMLOADEROBSERVER

  nsDOMWorkerScriptLoader(nsDOMWorker* aWorker);

  nsresult LoadScripts(JSContext* aCx,
                       const nsTArray<nsString>& aURLs,
                       PRBool aForWorker);

  nsresult LoadScript(JSContext* aCx,
                      const nsString& aURL,
                      PRBool aForWorker);

  virtual void Cancel();

private:
  ~nsDOMWorkerScriptLoader() { }


  nsresult DoRunLoop(JSContext* aCx);
  nsresult VerifyScripts(JSContext* aCx);
  nsresult ExecuteScripts(JSContext* aCx);

  nsresult RunInternal();

  nsresult OnStreamCompleteInternal(nsIStreamLoader* aLoader,
                                    nsISupports* aContext,
                                    nsresult aStatus,
                                    PRUint32 aStringLen,
                                    const PRUint8* aString);

  void NotifyDone();

  void SuspendWorkerEvents();
  void ResumeWorkerEvents();

  PRLock* Lock() {
    return mWorker->Lock();
  }

  class ScriptLoaderRunnable : public nsIRunnable
  {
  public:
    NS_DECL_ISUPPORTS

  protected:
    // Meant to be inherited.
    ScriptLoaderRunnable(nsDOMWorkerScriptLoader* aLoader);
    virtual ~ScriptLoaderRunnable();

  public:
    void Revoke();

  protected:
    PRBool mRevoked;

  private:
    nsDOMWorkerScriptLoader* mLoader;
  };

  class ScriptCompiler : public ScriptLoaderRunnable
  {
  public:
    NS_DECL_NSIRUNNABLE

    ScriptCompiler(nsDOMWorkerScriptLoader* aLoader,
                   const nsString& aScriptText,
                   const nsCString& aFilename,
                   nsAutoJSValHolder& aScriptObj);

  private:
    nsString mScriptText;
    nsCString mFilename;
    nsAutoJSValHolder& mScriptObj;
  };

  class ScriptLoaderDone : public ScriptLoaderRunnable
  {
  public:
    NS_DECL_NSIRUNNABLE

    ScriptLoaderDone(nsDOMWorkerScriptLoader* aLoader,
                     volatile PRBool* aDoneFlag);

  private:
    volatile PRBool* mDoneFlag;
  };

  class AutoSuspendWorkerEvents
  {
  public:
    AutoSuspendWorkerEvents(nsDOMWorkerScriptLoader* aLoader);
    ~AutoSuspendWorkerEvents();

  private:
    nsDOMWorkerScriptLoader* mLoader;
  };

  struct ScriptLoadInfo
  {
    ScriptLoadInfo() : done(PR_FALSE), result(NS_ERROR_NOT_INITIALIZED) { }

    nsString url;
    nsString scriptText;
    PRBool done;
    nsresult result;
    nsCOMPtr<nsIURI> finalURI;
    nsCOMPtr<nsIChannel> channel;
    nsAutoJSValHolder scriptObj;
  };

  nsIThread* mTarget;

  nsRefPtr<ScriptLoaderDone> mDoneRunnable;

  PRUint32 mScriptCount;
  nsTArray<ScriptLoadInfo> mLoadInfos;

  // Protected by mWorker's lock!
  nsTArray<ScriptLoaderRunnable*> mPendingRunnables;

  PRPackedBool mCanceled;
  PRPackedBool mForWorker;
};

#endif /* __NSDOMWORKERSCRIPTLOADER_H__ */
