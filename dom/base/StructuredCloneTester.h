/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StructuredCloneTester_h
#define mozilla_dom_StructuredCloneTester_h

#include "mozilla/AlreadyAddRefed.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

struct JSStructuredCloneReader;
struct JSStructuredCloneWriter;
class nsIGlobalObject;

namespace mozilla {

class ErrorResult;

namespace dom {

class GlobalObject;

class StructuredCloneTester final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(StructuredCloneTester)

  static already_AddRefed<StructuredCloneTester> Constructor(
      const GlobalObject& aGlobal, const bool aSerializable,
      const bool aDeserializable);

  bool Serializable() const;

  bool Deserializable() const;

  static already_AddRefed<StructuredCloneTester> ReadStructuredClone(
      JSContext* aCx, nsIGlobalObject* aGlobal,
      JSStructuredCloneReader* aReader);

  bool WriteStructuredClone(JSContext* aCx,
                            JSStructuredCloneWriter* aWriter) const;

  nsISupports* GetParentObject() const;

  // nsWrapperCache interface
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

 private:
  StructuredCloneTester(nsISupports* aParent, const bool aSerializable,
                        const bool aDeserializable);
  ~StructuredCloneTester() = default;

  nsCOMPtr<nsISupports> mParent;
  bool mSerializable;
  bool mDeserializable;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_StructuredCloneTester_h
