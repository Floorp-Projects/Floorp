/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMSerializer_h_
#define nsDOMSerializer_h_

#include "mozilla/dom/NonRefcountedDOMObject.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/XMLSerializerBinding.h"

class nsINode;
class nsIOutputStream;

class nsDOMSerializer final : public mozilla::dom::NonRefcountedDOMObject {
 public:
  nsDOMSerializer();

  // WebIDL API
  static nsDOMSerializer* Constructor(
      const mozilla::dom::GlobalObject& aOwner) {
    return new nsDOMSerializer();
  }

  void SerializeToString(nsINode& aRoot, nsAString& aStr,
                         mozilla::ErrorResult& rv);

  void SerializeToStream(nsINode& aRoot, nsIOutputStream* aStream,
                         const nsAString& aCharset, mozilla::ErrorResult& aRv);

  bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto,
                  JS::MutableHandle<JSObject*> aReflector) {
    return mozilla::dom::XMLSerializer_Binding::Wrap(aCx, this, aGivenProto,
                                                     aReflector);
  }
};

#endif
