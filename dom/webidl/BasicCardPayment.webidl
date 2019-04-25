/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this WebIDL file is
 *   https://www.w3.org/TR/payment-method-basic-card/
 */

dictionary BasicCardRequest {
  sequence<DOMString> supportedNetworks = [];
  boolean requestSecurityCode = true;
};

dictionary BasicCardResponse {
           DOMString cardholderName = "";
  required DOMString cardNumber;
           DOMString expiryMonth = "";
           DOMString expiryYear = "";
           DOMString cardSecurityCode = "";
           PaymentAddress? billingAddress = null;
};

dictionary BasicCardChangeDetails {
  PaymentAddress? billingAddress = null;
};

dictionary BasicCardErrors {
  DOMString cardNumber;
  DOMString cardholderName;
  DOMString cardSecurityCode;
  DOMString expiryMonth;
  DOMString expiryYear;
  AddressErrors billingAddress;
};
