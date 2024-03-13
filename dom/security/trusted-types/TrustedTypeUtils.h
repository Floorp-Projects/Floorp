/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SECURITY_TRUSTED_TYPES_TRUSTEDTYPEUTILS_H_
#define DOM_SECURITY_TRUSTED_TYPES_TRUSTEDTYPEUTILS_H_

#include "mozilla/dom/DOMString.h"
#include "mozilla/dom/NonRefcountedDOMObject.h"
#include "mozilla/dom/TrustedTypesBinding.h"
#include "nsStringFwd.h"

#define DECL_TRUSTED_TYPE_CLASS(_class)                                \
  class _class : public mozilla::dom::NonRefcountedDOMObject {         \
   public:                                                             \
    /* Required for Web IDL binding. */                                \
    bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto, \
                    JS::MutableHandle<JSObject*> aObject);             \
                                                                       \
    void Stringify(nsAString& aResult) const { /* TODO: impl. */       \
    }                                                                  \
                                                                       \
    void ToJSON(DOMString& aResult) const { /* TODO: impl. */          \
    }                                                                  \
  };

#define IMPL_TRUSTED_TYPE_CLASS(_class)                                      \
  bool _class::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto, \
                          JS::MutableHandle<JSObject*> aObject) {            \
    return _class##_Binding::Wrap(aCx, this, aGivenProto, aObject);          \
  }

#endif  // DOM_SECURITY_TRUSTED_TYPES_TRUSTEDTYPEUTILS_H_
