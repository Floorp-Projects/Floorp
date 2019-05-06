/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_scriptloader_h__
#define mozilla_dom_workers_scriptloader_h__

#include "mozilla/dom/WorkerCommon.h"
#include "nsIContentPolicy.h"
#include "nsStringFwd.h"

class nsIPrincipal;
class nsIURI;

class nsILoadGroup;
class nsIChannel;
class nsICookieSettings;

namespace mozilla {

class ErrorResult;

namespace dom {

struct WorkerLoadInfo;
class WorkerPrivate;
class SerializedStackHolder;

enum WorkerScriptType { WorkerScript, DebuggerScript };

namespace workerinternals {

nsresult ChannelFromScriptURLMainThread(
    nsIPrincipal* aPrincipal, Document* aParentDoc, nsILoadGroup* aLoadGroup,
    nsIURI* aScriptURL, const Maybe<ClientInfo>& aClientInfo,
    nsContentPolicyType aContentPolicyType, nsICookieSettings* aCookieSettings,
    nsIChannel** aChannel);

nsresult ChannelFromScriptURLWorkerThread(JSContext* aCx,
                                          WorkerPrivate* aParent,
                                          const nsAString& aScriptURL,
                                          WorkerLoadInfo& aLoadInfo);

void ReportLoadError(ErrorResult& aRv, nsresult aLoadResult,
                     const nsAString& aScriptURL);

void LoadMainScript(WorkerPrivate* aWorkerPrivate,
                    UniquePtr<SerializedStackHolder> aOriginStack,
                    const nsAString& aScriptURL,
                    WorkerScriptType aWorkerScriptType, ErrorResult& aRv);

void Load(WorkerPrivate* aWorkerPrivate,
          UniquePtr<SerializedStackHolder> aOriginStack,
          const nsTArray<nsString>& aScriptURLs,
          WorkerScriptType aWorkerScriptType, ErrorResult& aRv);

}  // namespace workerinternals

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_workers_scriptloader_h__ */
