/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AesKeyAlgorithm.h"
#include "mozilla/dom/SubtleCryptoBinding.h"
#include "mozilla/dom/WebCryptoCommon.h"

namespace mozilla {
namespace dom {

JSObject*
AesKeyAlgorithm::WrapObject(JSContext* aCx)
{
  return AesKeyAlgorithmBinding::Wrap(aCx, this);
}

bool
AesKeyAlgorithm::WriteStructuredClone(JSStructuredCloneWriter* aWriter) const
{
  return JS_WriteUint32Pair(aWriter, SCTAG_AESKEYALG, 0) &&
         JS_WriteUint32Pair(aWriter, mLength, 0) &&
         WriteString(aWriter, mName);
}

KeyAlgorithm*
AesKeyAlgorithm::Create(nsIGlobalObject* aGlobal, JSStructuredCloneReader* aReader)
{
  uint32_t length, zero;
  nsString name;
  bool read = JS_ReadUint32Pair(aReader, &length, &zero) &&
              ReadString(aReader, name);
  if (!read) {
    return nullptr;
  }

  return new AesKeyAlgorithm(aGlobal, name, length);
}


} // namespace dom
} // namespace mozilla
