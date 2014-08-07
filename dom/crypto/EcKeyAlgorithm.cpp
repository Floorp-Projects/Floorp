/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/EcKeyAlgorithm.h"
#include "mozilla/dom/SubtleCryptoBinding.h"
#include "mozilla/dom/WebCryptoCommon.h"

namespace mozilla {
namespace dom {

JSObject*
EcKeyAlgorithm::WrapObject(JSContext* aCx)
{
  return EcKeyAlgorithmBinding::Wrap(aCx, this);
}

bool
EcKeyAlgorithm::WriteStructuredClone(JSStructuredCloneWriter* aWriter) const
{
  return JS_WriteUint32Pair(aWriter, SCTAG_ECKEYALG, 0) &&
         WriteString(aWriter, mNamedCurve) &&
         WriteString(aWriter, mName);
}

KeyAlgorithm*
EcKeyAlgorithm::Create(nsIGlobalObject* aGlobal, JSStructuredCloneReader* aReader)
{
  nsString name;
  nsString namedCurve;
  bool read = ReadString(aReader, namedCurve) &&
              ReadString(aReader, name);
  if (!read) {
    return nullptr;
  }

  return new EcKeyAlgorithm(aGlobal, name, namedCurve);
}

} // namespace dom
} // namespace mozilla
