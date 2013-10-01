/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_scriptloader_h__
#define mozilla_dom_workers_scriptloader_h__

#include "Workers.h"

class nsIPrincipal;
class nsIURI;
class nsIDocument;
class nsString;
class nsIChannel;

BEGIN_WORKERS_NAMESPACE

namespace scriptloader {

nsresult
ChannelFromScriptURLMainThread(nsIPrincipal* aPrincipal,
                               nsIURI* aBaseURI,
                               nsIDocument* aParentDoc,
                               const nsString& aScriptURL,
                               nsIChannel** aChannel);

nsresult
ChannelFromScriptURLWorkerThread(JSContext* aCx,
                                 WorkerPrivate* aParent,
                                 const nsString& aScriptURL,
                                 nsIChannel** aChannel);

void ReportLoadError(JSContext* aCx, const nsString& aURL,
                     nsresult aLoadResult, bool aIsMainThread);

bool LoadWorkerScript(JSContext* aCx);

bool Load(JSContext* aCx, unsigned aURLCount, jsval* aURLs);

} // namespace scriptloader

END_WORKERS_NAMESPACE

#endif /* mozilla_dom_workers_scriptloader_h__ */
