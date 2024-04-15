/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef InspectorCSSParser_h___
#define InspectorCSSParser_h___

#include "mozilla/dom/InspectorUtilsBinding.h"
#include "mozilla/dom/NonRefcountedDOMObject.h"

namespace mozilla {

class StyleParserState;

namespace dom {

class InspectorCSSParser final : public NonRefcountedDOMObject {
 public:
  explicit InspectorCSSParser(const nsACString&);
  // The WebIDL constructor.
  static UniquePtr<InspectorCSSParser> Constructor(const GlobalObject& aGlobal,
                                                   const nsACString& aText);

  ~InspectorCSSParser();

  bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto,
                  JS::MutableHandle<JSObject*> aReflector) {
    return InspectorCSSParser_Binding::Wrap(aCx, this, aGivenProto, aReflector);
  }

  uint32_t LineNumber() const;
  uint32_t ColumnNumber() const;
  void NextToken(Nullable<InspectorCSSToken>& aResult);

 private:
  const nsCString mInput;
  StyleParserState* mParserState;
  uint32_t mLineNumber = 0;
  uint32_t mColumnNumber = 0;
};

}  // namespace dom
}  // namespace mozilla

#endif /* InspectorCSSParser_h___ */
