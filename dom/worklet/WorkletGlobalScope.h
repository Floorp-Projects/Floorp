/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WorkletGlobalScope_h
#define mozilla_dom_WorkletGlobalScope_h

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsDOMNavigationTiming.h"
#include "nsIGlobalObject.h"
#include "nsWrapperCache.h"

#define WORKLET_IID                                  \
  {                                                  \
    0x1b3f62e7, 0xe357, 0x44be, {                    \
      0xbf, 0xe0, 0xdf, 0x85, 0xe6, 0x56, 0x85, 0xac \
    }                                                \
  }

namespace mozilla {

class ErrorResult;
class WorkletImpl;

namespace dom {

class Console;

class WorkletGlobalScope : public nsIGlobalObject, public nsWrapperCache {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(WORKLET_IID)

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(WorkletGlobalScope)

  WorkletGlobalScope(WorkletImpl*);

  nsIGlobalObject* GetParentObject() const { return nullptr; }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  virtual bool WrapGlobalObject(JSContext* aCx,
                                JS::MutableHandle<JSObject*> aReflector) = 0;

  JSObject* GetGlobalJSObject() override { return GetWrapper(); }
  JSObject* GetGlobalJSObjectPreserveColor() const override {
    return GetWrapperPreserveColor();
  }

  already_AddRefed<Console> GetConsole(JSContext* aCx, ErrorResult& aRv);

  WorkletImpl* Impl() const { return mImpl.get(); }

  void Dump(const Optional<nsAString>& aString) const;

  DOMHighResTimeStamp TimeStampToDOMHighRes(const TimeStamp& aTimeStamp) const {
    MOZ_ASSERT(!aTimeStamp.IsNull());
    TimeDuration duration = aTimeStamp - mCreationTimeStamp;
    return duration.ToMilliseconds();
  }

  OriginTrials Trials() const override;
  Maybe<nsID> GetAgentClusterId() const override;
  bool IsSharedMemoryAllowed() const override;

 protected:
  ~WorkletGlobalScope();

  const RefPtr<WorkletImpl> mImpl;

 private:
  TimeStamp mCreationTimeStamp;
  RefPtr<Console> mConsole;
};

NS_DEFINE_STATIC_IID_ACCESSOR(WorkletGlobalScope, WORKLET_IID)

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_WorkletGlobalScope_h
