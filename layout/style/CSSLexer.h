/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CSSLexer_h___
#define CSSLexer_h___

#include "mozilla/UniquePtr.h"
#include "nsCSSScanner.h"
#include "mozilla/dom/CSSLexerBinding.h"

namespace mozilla {
namespace dom {

class CSSLexer : public NonRefcountedDOMObject
{
public:
  explicit CSSLexer(const nsAString&);
  ~CSSLexer();

  bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto,
                  JS::MutableHandle<JSObject*> aReflector);

  uint32_t LineNumber();
  uint32_t ColumnNumber();
  void PerformEOFFixup(const nsAString& aInputString, bool aPreserveBackslash,
                       nsAString& aResult);
  void NextToken(Nullable<CSSToken>& aResult);

private:
  nsString mInput;
  nsCSSScanner mScanner;
};

} // namespace dom
} // namespace mozilla

#endif /* CSSLexer_h___ */
