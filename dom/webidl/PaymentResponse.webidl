/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this WebIDL file is
 *   https:/w3c.github.io/payment-request/#paymentresponse-interface
 *
 * Copyright © 2018 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

enum PaymentComplete {
  "success",
  "fail",
  "unknown"
};

[SecureContext,
 Func="mozilla::dom::PaymentRequest::PrefEnabled"]
interface PaymentResponse : EventTarget {
  [Default] object toJSON();

  readonly attribute DOMString       requestId;
  readonly attribute DOMString       methodName;
  readonly attribute object          details;
  readonly attribute PaymentAddress? shippingAddress;
  readonly attribute DOMString?      shippingOption;
  readonly attribute DOMString?      payerName;
  readonly attribute DOMString?      payerEmail;
  readonly attribute DOMString?      payerPhone;

  [NewObject]
  Promise<void> complete(optional PaymentComplete result = "unknown");

  // If the dictionary argument has no required members, it must be optional.
  [NewObject]
  Promise<void> retry(optional PaymentValidationErrors errorFields = {});

  attribute EventHandler onpayerdetailchange;
};
