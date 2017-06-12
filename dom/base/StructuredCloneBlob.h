/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StructuredCloneBlob_h
#define mozilla_dom_StructuredCloneBlob_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/StructuredCloneHolder.h"
#include "mozilla/dom/StructuredCloneHolderBinding.h"
#include "mozilla/RefCounted.h"

#include "jsapi.h"

#include "nsISupports.h"

namespace mozilla {
namespace dom {

class StructuredCloneBlob : public StructuredCloneHolder
                          , public RefCounted<StructuredCloneBlob>
{
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(StructuredCloneBlob)

  explicit StructuredCloneBlob();

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
  template <typename T, detail::RefCountAtomicity>
  friend class detail::RefCounted;

  ~StructuredCloneBlob() = default;

private:
  bool ReadStructuredCloneInternal(JSContext* aCx, JSStructuredCloneReader* aReader,
                                   StructuredCloneHolder* aHolder);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_StructuredCloneBlob_h

