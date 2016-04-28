/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_scriptloader_h__
#define mozilla_dom_workers_scriptloader_h__

#include "Workers.h"
#include "nsIContentPolicyBase.h"

class nsIPrincipal;
class nsIURI;
class nsIDocument;
class nsILoadGroup;
class nsString;
class nsIChannel;

namespace mozilla {

class ErrorResult;

} // namespace mozilla

BEGIN_WORKERS_NAMESPACE

enum WorkerScriptType {
  WorkerScript,
  DebuggerScript
};

namespace scriptloader {

nsresult
ChannelFromScriptURLMainThread(nsIPrincipal* aPrincipal,
                               nsIURI* aBaseURI,
                               nsIDocument* aParentDoc,
                               nsILoadGroup* aLoadGroup,
                               const nsAString& aScriptURL,
                               nsContentPolicyType aContentPolicyType,
                               bool aDefaultURIEncoding,
                               nsIChannel** aChannel);

nsresult
ChannelFromScriptURLWorkerThread(JSContext* aCx,
                                 WorkerPrivate* aParent,
                                 const nsAString& aScriptURL,
                                 nsIChannel** aChannel);

void ReportLoadError(ErrorResult& aRv, nsresult aLoadResult,
                     const nsAString& aScriptURL);

void LoadMainScript(WorkerPrivate* aWorkerPrivate,
                    const nsAString& aScriptURL,
                    WorkerScriptType aWorkerScriptType,
                    ErrorResult& aRv);

void Load(WorkerPrivate* aWorkerPrivate,
          const nsTArray<nsString>& aScriptURLs,
          WorkerScriptType aWorkerScriptType,
          mozilla::ErrorResult& aRv);

} // namespace scriptloader

END_WORKERS_NAMESPACE

#endif /* mozilla_dom_workers_scriptloader_h__ */
