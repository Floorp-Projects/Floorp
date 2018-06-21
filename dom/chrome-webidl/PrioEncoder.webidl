/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[ChromeOnly, Exposed=(Window,System)]
namespace PrioEncoder {
  [Throws, NewObject]
  Promise<PrioEncodedData> encode(ByteString batchID, PrioParams params);
};

dictionary PrioParams {
  required boolean startupCrashDetected;
  required boolean safeModeUsage;
  required boolean browserIsUserDefault;
};

dictionary PrioEncodedData {
  Uint8Array a;
  Uint8Array b;
};