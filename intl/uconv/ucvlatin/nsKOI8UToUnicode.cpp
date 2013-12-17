/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUCConstructors.h"
#include "nsKOI8UToUnicode.h"
#include "mozilla/Telemetry.h"

using namespace mozilla;

//----------------------------------------------------------------------
// Global functions and data [declaration]

nsresult
nsKOI8UToUnicodeConstructor(nsISupports *aOuter, REFNSIID aIID,
                            void **aResult) 
{
  static const uint16_t g_utMappingTable[] = {
#include "koi8u.ut"
  };

  Telemetry::Accumulate(Telemetry::DECODER_INSTANTIATED_KOI8U, true);
  return CreateOneByteDecoder((uMappingTable*) &g_utMappingTable,
                              aOuter, aIID, aResult);
}
