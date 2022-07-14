/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_loader_BaseLoadContext_h
#define js_loader_BaseLoadContext_h

#include "js/loader/ScriptLoadRequest.h"
#include "nsIStringBundle.h"

namespace mozilla::dom {
class ScriptLoadContext;
class WorkerLoadContext;
}  // namespace mozilla::dom

namespace mozilla::loader {
class ComponentLoadContext;
}

namespace JS::loader {

class ScriptLoadRequest;

/*
 * LoadContextBase
 *
 * LoadContexts augment the loading of a ScriptLoadRequest.  This class
 * is used as a base for all LoadContexts, and provides shared functionality.
 *
 * Different loading environments have different rules applied to how a script
 * is loaded. In DOM scripts, there are flags controlling load order (Async,
 * Deferred, normal) as well as other elements that impact the loading of a
 * script (<preload>). In the case of workers, service workers are potentially
 * loaded from the Cache. For more detailed information per context see
 *     * The ScriptLoadContext: dom/script/ScriptLoadContext.h
 *
 */

enum class ContextKind { Window, Component, Worker };

class LoadContextBase : public nsISupports {
 private:
  ContextKind mKind;

 protected:
  virtual ~LoadContextBase() = default;

 public:
  explicit LoadContextBase(ContextKind kind);
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(LoadContextBase)

  void SetRequest(JS::loader::ScriptLoadRequest* aRequest);

  // Used to output a string for the Gecko Profiler.
  virtual void GetProfilerLabel(nsACString& aOutString);

  // Casting to the different contexts
  bool IsWindowContext() const { return mKind == ContextKind::Window; }
  mozilla::dom::ScriptLoadContext* AsWindowContext();

  bool IsComponentContext() const { return mKind == ContextKind::Component; }
  mozilla::loader::ComponentLoadContext* AsComponentContext();

  bool IsWorkerContext() const { return mKind == ContextKind::Worker; }
  mozilla::dom::WorkerLoadContext* AsWorkerContext();

  RefPtr<JS::loader::ScriptLoadRequest> mRequest;
};

}  // namespace JS::loader

#endif  // js_loader_BaseLoadContext_h
