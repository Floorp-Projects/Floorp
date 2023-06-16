/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StructuredCloneBlob_h
#define mozilla_dom_StructuredCloneBlob_h

#include "js/TypeDecls.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/StructuredCloneHolder.h"
#include "nsIMemoryReporter.h"
#include "nsISupports.h"

struct JSStructuredCloneReader;
struct JSStructuredCloneWriter;

namespace mozilla {

class ErrorResult;

namespace dom {

class GlobalObject;

class StructuredCloneBlob final : public nsIMemoryReporter {
  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf)

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

  static JSObject* ReadStructuredClone(JSContext* aCx,
                                       JSStructuredCloneReader* aReader,
                                       StructuredCloneHolder* aHolder);
  bool WriteStructuredClone(JSContext* aCx, JSStructuredCloneWriter* aWriter,
                            StructuredCloneHolder* aHolder);

  static already_AddRefed<StructuredCloneBlob> Constructor(
      GlobalObject& aGlobal, const nsACString& aName,
      const nsACString& aAnonymizedName, JS::Handle<JS::Value> aValue,
      JS::Handle<JSObject*> aTargetGlobal, ErrorResult& aRv);

  void Deserialize(JSContext* aCx, JS::Handle<JSObject*> aTargetScope,
                   bool aKeepData, JS::MutableHandle<JS::Value> aResult,
                   ErrorResult& aRv);

  nsISupports* GetParentObject() const { return nullptr; }
  JSObject* GetWrapper() const { return nullptr; }

  bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto,
                  JS::MutableHandle<JSObject*> aResult);

 protected:
  virtual ~StructuredCloneBlob();

 private:
  explicit StructuredCloneBlob();

  class Holder : public StructuredCloneHolder {
   public:
    using StructuredCloneHolder::StructuredCloneHolder;

    bool ReadStructuredCloneInternal(JSContext* aCx,
                                     JSStructuredCloneReader* aReader,
                                     StructuredCloneHolder* aHolder);

    bool WriteStructuredClone(JSContext* aCx, JSStructuredCloneWriter* aWriter,
                              StructuredCloneHolder* aHolder);
  };

  nsCString mName;
  nsCString mAnonymizedName;
  Maybe<Holder> mHolder;

  static already_AddRefed<StructuredCloneBlob> Create() {
    RefPtr<StructuredCloneBlob> holder = new StructuredCloneBlob();
    RegisterWeakMemoryReporter(holder);
    return holder.forget();
  }
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_StructuredCloneBlob_h
