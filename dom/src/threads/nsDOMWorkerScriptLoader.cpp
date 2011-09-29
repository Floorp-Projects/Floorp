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

#include "nsDOMWorkerScriptLoader.h"

// Interfaces
#include "nsIChannel.h"
#include "nsIContentPolicy.h"
#include "nsIHttpChannel.h"
#include "nsIIOService.h"
#include "nsIProtocolHandler.h"
#include "nsIRequest.h"
#include "nsIScriptSecurityManager.h"
#include "nsIStreamLoader.h"

// Other includes
#include "nsContentErrors.h"
#include "nsContentPolicyUtils.h"
#include "nsContentUtils.h"
#include "nsISupportsPrimitives.h"
#include "nsNetError.h"
#include "nsNetUtil.h"
#include "nsScriptLoader.h"
#include "nsThreadUtils.h"
#include "pratom.h"
#include "nsDocShellCID.h"
#include "nsIChannelPolicy.h"
#include "nsChannelPolicy.h"
#include "nsIContentSecurityPolicy.h"

// DOMWorker includes
#include "nsDOMWorkerPool.h"
#include "nsDOMWorkerSecurityManager.h"
#include "nsDOMThreadService.h"
#include "nsDOMWorkerTimeout.h"

using namespace mozilla;

#define LOG(_args) PR_LOG(gDOMThreadsLog, PR_LOG_DEBUG, _args)

nsDOMWorkerScriptLoader::nsDOMWorkerScriptLoader(nsDOMWorker* aWorker)
: nsDOMWorkerFeature(aWorker),
  mTarget(nsnull),
  mScriptCount(0),
  mCanceled(PR_FALSE),
  mForWorker(PR_FALSE)
{
  // Created on worker thread.
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aWorker, "Null worker!");
}

NS_IMPL_ISUPPORTS_INHERITED2(nsDOMWorkerScriptLoader, nsDOMWorkerFeature,
                                                      nsIRunnable,
                                                      nsIStreamLoaderObserver)

nsresult
nsDOMWorkerScriptLoader::LoadScripts(JSContext* aCx,
                                     const nsTArray<nsString>& aURLs,
                                     bool aExecute)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aCx, "Null context!");

  mTarget = NS_GetCurrentThread();
  NS_ASSERTION(mTarget, "This should never be null!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  mScriptCount = aURLs.Length();
  if (!mScriptCount) {
    return NS_ERROR_INVALID_ARG;
  }

  // Do all the memory work for these arrays now rather than checking for
  // failures all along the way.
  bool success = mLoadInfos.SetCapacity(mScriptCount);
  NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);

  // Need one runnable per script and then an extra for the finished
  // notification.
  success = mPendingRunnables.SetCapacity(mScriptCount + 1);
  NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);

  for (PRUint32 index = 0; index < mScriptCount; index++) {
    ScriptLoadInfo* newInfo = mLoadInfos.AppendElement();
    NS_ASSERTION(newInfo, "Shouldn't fail if SetCapacity succeeded above!");

    newInfo->url.Assign(aURLs[index]);
    if (newInfo->url.IsEmpty()) {
      return NS_ERROR_INVALID_ARG;
    }

    success = newInfo->scriptObj.Hold(aCx);
    NS_ENSURE_TRUE(success, NS_ERROR_FAILURE);
  }

  // Don't want timeouts, etc., from queuing up while we're waiting on the
  // network or compiling.
  AutoSuspendWorkerEvents aswe(this);

  nsresult rv = DoRunLoop(aCx);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Verify that all scripts downloaded and compiled.
  rv = VerifyScripts(aCx);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (aExecute) {
    rv = ExecuteScripts(aCx);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult
nsDOMWorkerScriptLoader::LoadWorkerScript(JSContext* aCx,
                                          const nsString& aURL)
{
  mForWorker = PR_TRUE;

  nsAutoTArray<nsString, 1> url;
  url.AppendElement(aURL);

  return LoadScripts(aCx, url, PR_FALSE);
}

nsresult
nsDOMWorkerScriptLoader::DoRunLoop(JSContext* aCx)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  volatile bool done = false;
  mDoneRunnable = new ScriptLoaderDone(this, &done);
  NS_ENSURE_TRUE(mDoneRunnable, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = NS_DispatchToMainThread(this);
  NS_ENSURE_SUCCESS(rv, rv);

  while (!(done || mCanceled)) {
    JSAutoSuspendRequest asr(aCx);
    NS_ProcessNextEvent(mTarget);
  }

  return mCanceled ? NS_ERROR_ABORT : NS_OK;
}

nsresult
nsDOMWorkerScriptLoader::VerifyScripts(JSContext* aCx)
{
  NS_ASSERTION(aCx, "Shouldn't be null!");

  nsresult rv = NS_OK;

  for (PRUint32 index = 0; index < mScriptCount; index++) {
    ScriptLoadInfo& loadInfo = mLoadInfos[index];
    NS_ASSERTION(loadInfo.done, "Inconsistent state!");

    if (NS_SUCCEEDED(loadInfo.result) && loadInfo.scriptObj.ToJSObject()) {
      continue;
    }

    NS_ASSERTION(!loadInfo.scriptObj.ToJSObject(), "Inconsistent state!");

    // Flag failure before worrying about whether or not to report an error.
    rv = NS_FAILED(loadInfo.result) ? loadInfo.result : NS_ERROR_FAILURE;

    // If loadInfo.result is a success code then the compiler probably reported
    // an error already. Also we don't really care about NS_BINDING_ABORTED
    // since that's the code we set when some other script had a problem and the
    // rest were canceled.
    if (NS_SUCCEEDED(loadInfo.result) || loadInfo.result == NS_BINDING_ABORTED) {
      continue;
    }

    // Ok, this is the script that caused us to fail.

    JSAutoRequest ar(aCx);

    // Only throw an error if there is no other pending exception.
    if (!JS_IsExceptionPending(aCx)) {
      const char* message;
      switch (loadInfo.result) {
        case NS_ERROR_MALFORMED_URI:
          message = "Malformed script URI: %s";
          break;
        case NS_ERROR_FILE_NOT_FOUND:
        case NS_ERROR_NOT_AVAILABLE:
          message = "Script file not found: %s";
          break;
        default:
          message = "Failed to load script: %s (nsresult = 0x%x)";
          break;
      }
      NS_ConvertUTF16toUTF8 url(loadInfo.url);
      JS_ReportError(aCx, message, url.get(), loadInfo.result);
    }
    break;
  }

  return rv;
}

nsresult
nsDOMWorkerScriptLoader::ExecuteScripts(JSContext* aCx)
{
  NS_ASSERTION(aCx, "Shouldn't be null!");

  // Now execute all the scripts.
  for (PRUint32 index = 0; index < mScriptCount; index++) {
    ScriptLoadInfo& loadInfo = mLoadInfos[index];

    JSAutoRequest ar(aCx);

    JSObject* scriptObj = loadInfo.scriptObj.ToJSObject();
    NS_ASSERTION(scriptObj, "This shouldn't ever be null!");

    JSObject* global = mWorker->mGlobal ?
                       mWorker->mGlobal :
                       JS_GetGlobalObject(aCx);
    NS_ENSURE_STATE(global);

    // Because we may have nested calls to this function we don't want the
    // execution to automatically report errors. We let them propagate instead.
    uint32 oldOpts =
      JS_SetOptions(aCx, JS_GetOptions(aCx) | JSOPTION_DONT_REPORT_UNCAUGHT);

    bool success = JS_ExecuteScript(aCx, global, scriptObj, NULL);

    JS_SetOptions(aCx, oldOpts);

    if (!success) {
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

void
nsDOMWorkerScriptLoader::Cancel()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  NS_ASSERTION(!mCanceled, "Cancel called more than once!");
  mCanceled = PR_TRUE;

  for (PRUint32 index = 0; index < mScriptCount; index++) {
    ScriptLoadInfo& loadInfo = mLoadInfos[index];

    nsIRequest* request =
      static_cast<nsIRequest*>(loadInfo.channel.get());
    if (request) {
#ifdef DEBUG
      nsresult rv =
#endif
      request->Cancel(NS_BINDING_ABORTED);
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Failed to cancel channel!");
    }
  }

  nsAutoTArray<ScriptLoaderRunnable*, 10> runnables;
  {
    MutexAutoLock lock(mWorker->GetLock());
    runnables.AppendElements(mPendingRunnables);
    mPendingRunnables.Clear();
  }

  PRUint32 runnableCount = runnables.Length();
  for (PRUint32 index = 0; index < runnableCount; index++) {
    runnables[index]->Revoke();
  }

  // We're about to post a revoked event to the worker thread, which seems
  // silly, but we have to do this because the worker thread may be sleeping
  // waiting on its event queue.
  NotifyDone();
}

NS_IMETHODIMP
nsDOMWorkerScriptLoader::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // We may have been canceled already.
  if (mCanceled) {
    return NS_BINDING_ABORTED;
  }

  nsresult rv = RunInternal();
  if (NS_SUCCEEDED(rv)) {
    return rv;
  }

  // Ok, something failed beyond a normal cancel.

  // If necko is holding a ref to us then we'll end up notifying in the
  // OnStreamComplete method, not here.
  bool needsNotify = true;

  // Cancel any async channels that were already opened.
  for (PRUint32 index = 0; index < mScriptCount; index++) {
    ScriptLoadInfo& loadInfo = mLoadInfos[index];

    nsIRequest* request = static_cast<nsIRequest*>(loadInfo.channel.get());
    if (request) {
#ifdef DEBUG
      nsresult rvInner =
#endif
      request->Cancel(NS_BINDING_ABORTED);
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rvInner), "Failed to cancel channel!");

      // Necko is holding a ref to us so make sure that the OnStreamComplete
      // code sends the done event.
      needsNotify = PR_FALSE;
    }
    else {
      // Make sure to set this so that the OnStreamComplete code will dispatch
      // the done event.
      loadInfo.done = PR_TRUE;
    }
  }

  if (needsNotify) {
    NotifyDone();
  }

  return rv;
}

NS_IMETHODIMP
nsDOMWorkerScriptLoader::OnStreamComplete(nsIStreamLoader* aLoader,
                                          nsISupports* aContext,
                                          nsresult aStatus,
                                          PRUint32 aStringLen,
                                          const PRUint8* aString)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // We may have been canceled already.
  if (mCanceled) {
    return NS_BINDING_ABORTED;
  }

  nsresult rv = OnStreamCompleteInternal(aLoader, aContext, aStatus, aStringLen,
                                         aString);

  // Dispatch the done event if we've received all the data.
  for (PRUint32 index = 0; index < mScriptCount; index++) {
    if (!mLoadInfos[index].done) {
      // Some async load is still outstanding, don't notify yet.
      break;
    }

    if (index == mScriptCount - 1) {
      // All loads complete, signal the thread.
      NotifyDone();
    }
  }

  return rv;
}

nsresult
nsDOMWorkerScriptLoader::RunInternal()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mForWorker) {
    NS_ASSERTION(mScriptCount == 1, "Bad state!");
  }

  nsRefPtr<nsDOMWorker> parentWorker = mWorker->GetParent();

  // Figure out which principal to use.
  nsIPrincipal* principal = mWorker->GetPrincipal();
  if (!principal) {
    if (!parentWorker) {
      NS_ERROR("Must have a principal if this is not a subworker!");
    }
    principal = parentWorker->GetPrincipal();
  }
  NS_ASSERTION(principal, "This should never be null here!");

  // Figure out our base URI.
  nsCOMPtr<nsIURI> baseURI;
  if (mForWorker) {
    if (parentWorker) {
      baseURI = parentWorker->GetBaseURI();
      NS_ASSERTION(baseURI, "Should have been set already!");
    }
    else {
      // May be null.
      baseURI = mWorker->GetBaseURI();

      // Don't leave a temporary URI hanging around.
      mWorker->ClearBaseURI();
    }
    NS_ASSERTION(!mWorker->GetBaseURI(), "Should not be set here!");
  }
  else {
    baseURI = mWorker->GetBaseURI();
    NS_ASSERTION(baseURI, "Should have been set already!");
  }

  nsCOMPtr<nsIDocument> parentDoc = mWorker->Pool()->ParentDocument();

  // All of these can potentially be null, but that should be ok. We'll either
  // succeed without them or fail below.
  nsCOMPtr<nsILoadGroup> loadGroup;
  if (parentDoc) {
    loadGroup = parentDoc->GetDocumentLoadGroup();
  }

  nsCOMPtr<nsIIOService> ios(do_GetIOService());

  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  NS_ASSERTION(secMan, "This should never be null!");

  for (PRUint32 index = 0; index < mScriptCount; index++) {
    ScriptLoadInfo& loadInfo = mLoadInfos[index];
    nsresult& rv = loadInfo.result;

    nsCOMPtr<nsIURI>& uri = loadInfo.finalURI;
    rv = nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(uri),
                                                   loadInfo.url, parentDoc,
                                                   baseURI);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // If we're part of a document then check the content load policy.
    if (parentDoc) {
      PRInt16 shouldLoad = nsIContentPolicy::ACCEPT;
      rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_SCRIPT, uri,
                                     principal, parentDoc,
                                     NS_LITERAL_CSTRING("text/javascript"),
                                     nsnull, &shouldLoad,
                                     nsContentUtils::GetContentPolicy(),
                                     secMan);
      if (NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad)) {
        if (NS_FAILED(rv) || shouldLoad != nsIContentPolicy::REJECT_TYPE) {
          return (rv = NS_ERROR_CONTENT_BLOCKED);
        }
        return (rv = NS_ERROR_CONTENT_BLOCKED_SHOW_ALT);
      }
    }

    // If this script loader is being used to make a new worker then we need to
    // do a same-origin check. Otherwise we need to clear the load with the
    // security manager.
    rv = mForWorker ?
         principal->CheckMayLoad(uri, PR_FALSE):
         secMan->CheckLoadURIWithPrincipal(principal, uri, 0);
    NS_ENSURE_SUCCESS(rv, rv);

    // We need to know which index we're on in OnStreamComplete so we know where
    // to put the result.
    nsCOMPtr<nsISupportsPRUint32> indexSupports =
      do_CreateInstance(NS_SUPPORTS_PRUINT32_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = indexSupports->SetData(index);
    NS_ENSURE_SUCCESS(rv, rv);

    // We don't care about progress so just use the simple stream loader for
    // OnStreamComplete notification only.
    nsCOMPtr<nsIStreamLoader> loader;
    rv = NS_NewStreamLoader(getter_AddRefs(loader), this);
    NS_ENSURE_SUCCESS(rv, rv);

    // Get Content Security Policy from parent document to pass into channel
    nsCOMPtr<nsIContentSecurityPolicy> csp;
    rv = principal->GetCsp(getter_AddRefs(csp));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIChannelPolicy> channelPolicy;
    if (csp) {
      channelPolicy = do_CreateInstance("@mozilla.org/nschannelpolicy;1", &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = channelPolicy->SetContentSecurityPolicy(csp);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = channelPolicy->SetLoadType(nsIContentPolicy::TYPE_SCRIPT);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = NS_NewChannel(getter_AddRefs(loadInfo.channel),
                       uri, ios, loadGroup, nsnull,
                       nsIRequest::LOAD_NORMAL | nsIChannel::LOAD_CLASSIFY_URI,
                       channelPolicy);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = loadInfo.channel->AsyncOpen(loader, indexSupports);
    if (NS_FAILED(rv)) {
      // Null this out so we don't try to cancel it later.
      loadInfo.channel = nsnull;
      return rv;
    }
  }

  return NS_OK;
}

nsresult
nsDOMWorkerScriptLoader::OnStreamCompleteInternal(nsIStreamLoader* aLoader,
                                                  nsISupports* aContext,
                                                  nsresult aStatus,
                                                  PRUint32 aStringLen,
                                                  const PRUint8* aString)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsISupportsPRUint32> indexSupports(do_QueryInterface(aContext));
  NS_ENSURE_TRUE(indexSupports, NS_ERROR_NO_INTERFACE);

  PRUint32 index = PR_UINT32_MAX;
  indexSupports->GetData(&index);

  if (index >= mScriptCount) {
    NS_NOTREACHED("This really can't fail or we'll hang!");
    return NS_ERROR_FAILURE;
  }

  ScriptLoadInfo& loadInfo = mLoadInfos[index];

  NS_ASSERTION(!loadInfo.done, "Got complete on the same load twice!");
  loadInfo.done = PR_TRUE;

  // Use an alias to keep rv and loadInfo.result in sync.
  nsresult& rv = loadInfo.result;

  if (NS_FAILED(aStatus)) {
    return rv = aStatus;
  }

  if (!(aStringLen && aString)) {
    return rv = NS_ERROR_UNEXPECTED;
  }

  // Make sure we're not seeing the result of a 404 or something by checking the
  // 'requestSucceeded' attribute on the http channel.
  nsCOMPtr<nsIRequest> request;
  rv = aLoader->GetRequest(getter_AddRefs(request));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(request);
  if (httpChannel) {
    bool requestSucceeded;
    rv = httpChannel->GetRequestSucceeded(&requestSucceeded);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!requestSucceeded) {
      return rv = NS_ERROR_NOT_AVAILABLE;
    }
  }

  // May be null.
  nsIDocument* parentDoc = mWorker->Pool()->ParentDocument();

  // Use the regular nsScriptLoader for this grunt work! Should be just fine
  // because we're running on the main thread.
  rv = nsScriptLoader::ConvertToUTF16(loadInfo.channel, aString, aStringLen,
                                      EmptyString(), parentDoc,
                                      loadInfo.scriptText);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (loadInfo.scriptText.IsEmpty()) {
    return rv = NS_ERROR_FAILURE;
  }

  nsCString filename;
  rv = loadInfo.finalURI->GetSpec(filename);
  NS_ENSURE_SUCCESS(rv, rv);

  if (filename.IsEmpty()) {
    filename.Assign(NS_LossyConvertUTF16toASCII(loadInfo.url));
  }
  else {
    // This will help callers figure out what their script url resolved to in
    // case of errors.
    loadInfo.url.Assign(NS_ConvertUTF8toUTF16(filename));
  }

  // Update the principal of the worker and its base URI if we just loaded the
  // worker's primary script.
  if (mForWorker) {
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
    NS_ASSERTION(channel, "This should never fail!");

    // Take care of the base URI first.
    nsCOMPtr<nsIURI> finalURI;
    rv = NS_GetFinalChannelURI(channel, getter_AddRefs(finalURI));
    NS_ENSURE_SUCCESS(rv, rv);

    mWorker->SetBaseURI(finalURI);

    // Now to figure out which principal to give this worker.
    nsRefPtr<nsDOMWorker> parent = mWorker->GetParent();
    NS_ASSERTION(mWorker->GetPrincipal() || parent, "Must have one of these!");

    nsCOMPtr<nsIPrincipal> loadPrincipal = mWorker->GetPrincipal() ?
                                           mWorker->GetPrincipal() :
                                           parent->GetPrincipal();

    nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
    NS_ASSERTION(ssm, "Should never be null!");

    nsCOMPtr<nsIPrincipal> channelPrincipal;
    rv = ssm->GetChannelPrincipal(channel, getter_AddRefs(channelPrincipal));
    NS_ENSURE_SUCCESS(rv, rv);

    // See if this is a resource URI. Since JSMs usually come from resource://
    // URIs we're currently considering all URIs with the URI_IS_UI_RESOURCE
    // flag as valid for creating privileged workers.
    if (!nsContentUtils::IsSystemPrincipal(channelPrincipal)) {
      bool isResource;
      rv = NS_URIChainHasFlags(finalURI,
                               nsIProtocolHandler::URI_IS_UI_RESOURCE,
                               &isResource);
      NS_ENSURE_SUCCESS(rv, rv);

      if (isResource) {
        rv = ssm->GetSystemPrincipal(getter_AddRefs(channelPrincipal));
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }

    // If the load principal is the system principal then the channel principal
    // must also be the system principal (we do not allow chrome code to create
    // workers with non-chrome scripts). Otherwise this channel principal must
    // be same origin with the load principal (we check again here in case
    // redirects changed the location of the script).
    if (nsContentUtils::IsSystemPrincipal(loadPrincipal)) {
      if (!nsContentUtils::IsSystemPrincipal(channelPrincipal)) {
        return rv = NS_ERROR_DOM_BAD_URI;
      }
    }
    else if (NS_FAILED(loadPrincipal->CheckMayLoad(finalURI, PR_FALSE))) {
      return rv = NS_ERROR_DOM_BAD_URI;
    }

    mWorker->SetPrincipal(channelPrincipal);
  }

  nsRefPtr<ScriptCompiler> compiler =
    new ScriptCompiler(this, loadInfo.scriptText, filename, loadInfo.scriptObj);
  NS_ASSERTION(compiler, "Out of memory!");
  if (!compiler) {
    return rv = NS_ERROR_OUT_OF_MEMORY;
  }

  rv = mTarget->Dispatch(compiler, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

void
nsDOMWorkerScriptLoader::NotifyDone()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mDoneRunnable) {
    // We've already completed, no need to cancel anything.
    return;
  }

  for (PRUint32 index = 0; index < mScriptCount; index++) {
    ScriptLoadInfo& loadInfo = mLoadInfos[index];
    // Null both of these out because they aren't threadsafe and must be
    // destroyed on this thread.
    loadInfo.channel = nsnull;
    loadInfo.finalURI = nsnull;

    if (mCanceled) {
      // Simulate a complete, yet failed, load.
      loadInfo.done = PR_TRUE;
      loadInfo.result = NS_BINDING_ABORTED;
    }
  }

#ifdef DEBUG
  nsresult rv =
#endif
  mTarget->Dispatch(mDoneRunnable, NS_DISPATCH_NORMAL);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Couldn't dispatch done event!");

  mDoneRunnable = nsnull;
}

void
nsDOMWorkerScriptLoader::SuspendWorkerEvents()
{
  NS_ASSERTION(mWorker, "No worker yet!");
  mWorker->SuspendFeatures();
}

void
nsDOMWorkerScriptLoader::ResumeWorkerEvents()
{
  NS_ASSERTION(mWorker, "No worker yet!");
  mWorker->ResumeFeatures();
}

nsDOMWorkerScriptLoader::
ScriptLoaderRunnable::ScriptLoaderRunnable(nsDOMWorkerScriptLoader* aLoader)
: mRevoked(PR_FALSE),
  mLoader(aLoader)
{
  MutexAutoLock lock(aLoader->GetLock());
#ifdef DEBUG
  nsDOMWorkerScriptLoader::ScriptLoaderRunnable** added =
#endif
  aLoader->mPendingRunnables.AppendElement(this);
  NS_ASSERTION(added, "This shouldn't fail because we SetCapacity earlier!");
}

nsDOMWorkerScriptLoader::
ScriptLoaderRunnable::~ScriptLoaderRunnable()
{
  if (!mRevoked) {
    MutexAutoLock lock(mLoader->GetLock());
#ifdef DEBUG
    bool removed =
#endif
    mLoader->mPendingRunnables.RemoveElement(this);
    NS_ASSERTION(removed, "Someone has changed the array!");
  }
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDOMWorkerScriptLoader::ScriptLoaderRunnable,
                              nsIRunnable)

void
nsDOMWorkerScriptLoader::ScriptLoaderRunnable::Revoke()
{
  mRevoked = PR_TRUE;
}

nsDOMWorkerScriptLoader::
ScriptCompiler::ScriptCompiler(nsDOMWorkerScriptLoader* aLoader,
                               const nsString& aScriptText,
                               const nsCString& aFilename,
                               nsAutoJSValHolder& aScriptObj)
: ScriptLoaderRunnable(aLoader),
  mScriptText(aScriptText),
  mFilename(aFilename),
  mScriptObj(aScriptObj)
{
  NS_ASSERTION(!aScriptText.IsEmpty(), "No script to compile!");
  NS_ASSERTION(aScriptObj.IsHeld(), "Should be held!");
}

NS_IMETHODIMP
nsDOMWorkerScriptLoader::ScriptCompiler::Run()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mRevoked) {
    return NS_OK;
  }

  NS_ASSERTION(!mScriptObj.ToJSObject(), "Already have a script object?!");
  NS_ASSERTION(mScriptObj.IsHeld(), "Not held?!");
  NS_ASSERTION(!mScriptText.IsEmpty(), "Shouldn't have empty source here!");

  JSContext* cx = nsDOMThreadService::GetCurrentContext();
  NS_ENSURE_STATE(cx);

  JSAutoRequest ar(cx);

  JSObject* global = JS_GetGlobalObject(cx);
  NS_ENSURE_STATE(global);

  // Because we may have nested calls to this function we don't want the
  // execution to automatically report errors. We let them propagate instead.
  uint32 oldOpts =
    JS_SetOptions(cx, JS_GetOptions(cx) | JSOPTION_DONT_REPORT_UNCAUGHT |
                      JSOPTION_NO_SCRIPT_RVAL);

  JSPrincipals* principal = nsDOMWorkerSecurityManager::WorkerPrincipal();

  JSObject* scriptObj =
    JS_CompileUCScriptForPrincipals(cx, global, principal,
                                    reinterpret_cast<const jschar*>
                                               (mScriptText.BeginReading()),
                                    mScriptText.Length(), mFilename.get(), 1);

  JS_SetOptions(cx, oldOpts);

  if (!scriptObj) {
    return NS_ERROR_FAILURE;
  }

  mScriptObj = scriptObj;

  return NS_OK;
}

nsDOMWorkerScriptLoader::
ScriptLoaderDone::ScriptLoaderDone(nsDOMWorkerScriptLoader* aLoader,
                                   volatile bool* aDoneFlag)
: ScriptLoaderRunnable(aLoader),
  mDoneFlag(aDoneFlag)
{
  NS_ASSERTION(aDoneFlag && !*aDoneFlag, "Bad setup!");
}

NS_IMETHODIMP
nsDOMWorkerScriptLoader::ScriptLoaderDone::Run()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mRevoked) {
    return NS_OK;
  }

  *mDoneFlag = PR_TRUE;
  return NS_OK;
}

nsDOMWorkerScriptLoader::
AutoSuspendWorkerEvents::AutoSuspendWorkerEvents(nsDOMWorkerScriptLoader* aLoader)
: mLoader(aLoader)
{
  NS_ASSERTION(aLoader, "Don't hand me null!");
  aLoader->SuspendWorkerEvents();
}

nsDOMWorkerScriptLoader::
AutoSuspendWorkerEvents::~AutoSuspendWorkerEvents()
{
  mLoader->ResumeWorkerEvents();
}
