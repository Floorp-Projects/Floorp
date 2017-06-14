/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TextEncoder.h"
#include "mozilla/Encoding.h"

namespace mozilla {
namespace dom {

void
TextEncoder::Init()
{
}

void
TextEncoder::Encode(JSContext* aCx,
                    JS::Handle<JSObject*> aObj,
                    const nsAString& aString,
                    JS::MutableHandle<JSObject*> aRetval,
                    ErrorResult& aRv)
{
  nsAutoCString utf8;
  nsresult rv;
  const Encoding* ignored;
  Tie(rv, ignored) = UTF_8_ENCODING->Encode(aString, utf8);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  JSAutoCompartment ac(aCx, aObj);
  JSObject* outView = Uint8Array::Create(
    aCx, utf8.Length(), reinterpret_cast<const uint8_t*>(utf8.BeginReading()));
  if (!outView) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  aRetval.set(outView);
}

void
TextEncoder::GetEncoding(nsAString& aEncoding)
{
  aEncoding.AssignLiteral("utf-8");
}

} // namespace dom
} // namespace mozilla
