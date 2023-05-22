/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ChromeWorker_h
#define mozilla_dom_ChromeWorker_h

#include "mozilla/dom/Worker.h"

namespace mozilla::dom {

class ChromeWorker final : public Worker {
 public:
  static already_AddRefed<ChromeWorker> Constructor(
      const GlobalObject& aGlobal, const nsAString& aScriptURL,
      const WorkerOptions& aOptions, ErrorResult& aRv);

  static bool WorkerAvailable(JSContext* aCx, JSObject* /* unused */);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

 private:
  ChromeWorker(nsIGlobalObject* aGlobalObject,
               already_AddRefed<WorkerPrivate> aWorkerPrivate);
  ~ChromeWorker();
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_ChromeWorker_h */
