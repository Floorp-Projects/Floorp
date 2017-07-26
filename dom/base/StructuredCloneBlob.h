/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StructuredCloneBlob_h
#define mozilla_dom_StructuredCloneBlob_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/StructuredCloneHolder.h"
#include "mozilla/dom/StructuredCloneHolderBinding.h"

#include "jsapi.h"

#include "nsIMemoryReporter.h"
#include "nsISupports.h"

namespace mozilla {
namespace dom {

class StructuredCloneBlob final : public nsIMemoryReporter
                                , public StructuredCloneHolder
{
  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf)

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

  static JSObject* ReadStructuredClone(JSContext* aCx, JSStructuredCloneReader* aReader,
                                       StructuredCloneHolder* aHolder);
  bool WriteStructuredClone(JSContext* aCx, JSStructuredCloneWriter* aWriter,
                            StructuredCloneHolder* aHolder);

  static already_AddRefed<StructuredCloneBlob>
  Constructor(GlobalObject& aGlobal, JS::HandleValue aValue, JS::HandleObject aTargetGlobal, ErrorResult& aRv);

  void Deserialize(JSContext* aCx, JS::HandleObject aTargetScope,
                   JS::MutableHandleValue aResult, ErrorResult& aRv);

  nsISupports* GetParentObject() const { return nullptr; }
  JSObject* GetWrapper() const { return nullptr; }

  bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto, JS::MutableHandleObject aResult);

protected:
  virtual ~StructuredCloneBlob();

private:
  explicit StructuredCloneBlob();

  static already_AddRefed<StructuredCloneBlob> Create()
  {
    RefPtr<StructuredCloneBlob> holder = new StructuredCloneBlob();
    RegisterWeakMemoryReporter(holder);
    return holder.forget();
  }

  bool ReadStructuredCloneInternal(JSContext* aCx, JSStructuredCloneReader* aReader,
                                   StructuredCloneHolder* aHolder);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_StructuredCloneBlob_h

