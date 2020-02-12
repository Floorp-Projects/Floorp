/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WindowProxyHolder_h__
#define mozilla_dom_WindowProxyHolder_h__

#include "mozilla/dom/BrowsingContext.h"

struct JSContext;
class JSObject;

namespace JS {
template <typename T>
class MutableHandle;
}  // namespace JS

namespace mozilla {
namespace dom {

/**
 * This class is used for passing arguments and the return value for WebIDL
 * binding code that takes/returns a WindowProxy object and for WebIDL
 * unions/dictionaries that contain a WindowProxy member. It should never
 * contain null; if the value in WebIDL is nullable the binding code will use a
 * Nullable<WindowProxyHolder>.
 */
class WindowProxyHolder {
 public:
  WindowProxyHolder() = default;
  explicit WindowProxyHolder(BrowsingContext* aBC) : mBrowsingContext(aBC) {
    MOZ_ASSERT(mBrowsingContext, "Don't set WindowProxyHolder to null.");
  }
  explicit WindowProxyHolder(already_AddRefed<BrowsingContext>&& aBC)
      : mBrowsingContext(std::move(aBC)) {
    MOZ_ASSERT(mBrowsingContext, "Don't set WindowProxyHolder to null.");
  }
  WindowProxyHolder& operator=(BrowsingContext* aBC) {
    mBrowsingContext = aBC;
    MOZ_ASSERT(mBrowsingContext, "Don't set WindowProxyHolder to null.");
    return *this;
  }
  WindowProxyHolder& operator=(already_AddRefed<BrowsingContext>&& aBC) {
    mBrowsingContext = std::move(aBC);
    MOZ_ASSERT(mBrowsingContext, "Don't set WindowProxyHolder to null.");
    return *this;
  }

  BrowsingContext* get() const {
    MOZ_ASSERT(mBrowsingContext, "WindowProxyHolder hasn't been initialized.");
    return mBrowsingContext;
  }

 private:
  friend void ImplCycleCollectionUnlink(WindowProxyHolder& aProxy);

  RefPtr<BrowsingContext> mBrowsingContext;
};

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback, WindowProxyHolder& aProxy,
    const char* aName, uint32_t aFlags = 0) {
  CycleCollectionNoteChild(aCallback, aProxy.get(), "mBrowsingContext", aFlags);
}

inline void ImplCycleCollectionUnlink(WindowProxyHolder& aProxy) {
  aProxy.mBrowsingContext = nullptr;
}

extern bool GetRemoteOuterWindowProxy(JSContext* aCx, BrowsingContext* aContext,
                                      JS::Handle<JSObject*> aTransplantTo,
                                      JS::MutableHandle<JSObject*> aValue);

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_WindowProxyHolder_h__ */
