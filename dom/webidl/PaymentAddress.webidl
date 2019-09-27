/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this WebIDL file is
 *   https://www.w3.org/TR/payment-request/#paymentaddress-interface
 *
 * Copyright © 2018 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

[SecureContext,
 Func="mozilla::dom::PaymentRequest::PrefEnabled",
 Exposed=Window]
interface PaymentAddress {
  [Default] object toJSON();

  readonly attribute DOMString              country;
  // TODO: Use FrozenArray once available. (Bug 1236777)
  // readonly attribute FrozenArray<DOMString> addressLine;
  [Frozen, Cached, Pure]
  readonly attribute sequence<DOMString>    addressLine;
  readonly attribute DOMString              region;
  readonly attribute DOMString              regionCode;
  readonly attribute DOMString              city;
  readonly attribute DOMString              dependentLocality;
  readonly attribute DOMString              postalCode;
  readonly attribute DOMString              sortingCode;
  readonly attribute DOMString              organization;
  readonly attribute DOMString              recipient;
  readonly attribute DOMString              phone;
};
