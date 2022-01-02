/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this WebIDL file is
 *   https://w3c.github.io/payment-request/#paymentrequestupdateevent-interface
 *
 * Copyright © 2018 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

[SecureContext,
 Func="mozilla::dom::PaymentRequest::PrefEnabled",
 Exposed=Window]
interface PaymentRequestUpdateEvent : Event {
  constructor(DOMString type,
              optional PaymentRequestUpdateEventInit eventInitDict = {});

  [Throws]
  void updateWith(Promise<PaymentDetailsUpdate> detailsPromise);
};

dictionary PaymentRequestUpdateEventInit : EventInit {
};
