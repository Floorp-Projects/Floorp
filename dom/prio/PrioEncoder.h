/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PrioEncoder_h
#define mozilla_dom_PrioEncoder_h

#include "mozilla/dom/PrioEncoderBinding.h"
#include "mozilla/dom/RootedDictionary.h"

#include "mprio.h"

class nsIGlobalObject;

namespace mozilla {
namespace dom {

class PrioEncoder
{
public:
  NS_INLINE_DECL_REFCOUNTING(PrioEncoder)

  static
  void Encode(GlobalObject& aGlobal,
              const nsCString& aBatchID,
              const PrioParams& aPrioParams,
              RootedDictionary<PrioEncodedData>& aData,
              ErrorResult& aRv);

private:
  PrioEncoder();
  ~PrioEncoder();

  static PublicKey
  sPublicKeyA;

  static PublicKey
  sPublicKeyB;

  static StaticRefPtr<PrioEncoder>
  sSingleton;

  static bool
  IsValidHexPublicKey(mozilla::Span<const char>);
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_PrioEncoder_h
