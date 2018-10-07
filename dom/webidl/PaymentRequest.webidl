/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this WebIDL file is
 *   https://www.w3.org/TR/payment-request/#paymentrequest-interface
 *
 * Copyright © 2018 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

dictionary PaymentMethodData {
  required DOMString           supportedMethods;
           object              data;
};

dictionary PaymentCurrencyAmount {
  required DOMString currency;
  required DOMString value;
};

enum PaymentItemType {
  "tax"
};

dictionary PaymentItem {
  required DOMString             label;
  required PaymentCurrencyAmount amount;
           boolean               pending = false;
           PaymentItemType       type;
};

dictionary PaymentShippingOption {
  required DOMString             id;
  required DOMString             label;
  required PaymentCurrencyAmount amount;
           boolean               selected = false;
};

dictionary PaymentDetailsModifier {
  required DOMString             supportedMethods;
  // FIXME: bug 1493860: should this "= null" be here?
           PaymentItem           total = null;
           sequence<PaymentItem> additionalDisplayItems;
           object                data;
};

dictionary PaymentDetailsBase {
  sequence<PaymentItem>            displayItems;
  sequence<PaymentShippingOption>  shippingOptions;
  sequence<PaymentDetailsModifier> modifiers;
};

dictionary PaymentDetailsInit : PaymentDetailsBase {
           DOMString   id;
  required PaymentItem total;
};

dictionary AddressErrors {
  DOMString addressLine;
  DOMString city;
  DOMString country;
  DOMString dependentLocality;
  DOMString organization;
  DOMString phone;
  DOMString postalCode;
  DOMString recipient;
  DOMString region;
  DOMString regionCode;
  DOMString sortingCode;
};

dictionary PaymentValidationErrors {
  // FIXME: bug 1493860: should this "= null" be here?
  PayerErrors payer = null;
  // FIXME: bug 1493860: should this "= null" be here?
  AddressErrors shippingAddress = null;
  DOMString error;
  object paymentMethod;
};

dictionary PayerErrors {
  DOMString email;
  DOMString name;
  DOMString phone;
};

dictionary PaymentDetailsUpdate : PaymentDetailsBase {
  DOMString     error;
  // FIXME: bug 1493860: should this "= null" be here?
  AddressErrors shippingAddressErrors = null;
  // FIXME: bug 1493860: should this "= null" be here?
  PayerErrors payerErrors = null;
  object paymentMethodErrors;
  // FIXME: bug 1493860: should this "= null" be here?
  PaymentItem   total = null;
};

enum PaymentShippingType {
  "shipping",
  "delivery",
  "pickup"
};

dictionary PaymentOptions {
  boolean             requestPayerName = false;
  boolean             requestPayerEmail = false;
  boolean             requestPayerPhone = false;
  boolean             requestShipping = false;
  PaymentShippingType shippingType = "shipping";
};

[Constructor(sequence<PaymentMethodData> methodData, PaymentDetailsInit details,
             optional PaymentOptions options),
 SecureContext,
 Func="mozilla::dom::PaymentRequest::PrefEnabled"]
interface PaymentRequest : EventTarget {
  [NewObject]
  Promise<PaymentResponse> show(optional Promise<PaymentDetailsUpdate> detailsPromise);
  [NewObject]
  Promise<void>            abort();
  [NewObject]
  Promise<boolean>         canMakePayment();

  readonly attribute DOMString            id;
  readonly attribute PaymentAddress?      shippingAddress;
  readonly attribute DOMString?           shippingOption;
  readonly attribute PaymentShippingType? shippingType;

           attribute EventHandler         onmerchantvalidation;
           attribute EventHandler         onshippingaddresschange;
           attribute EventHandler         onshippingoptionchange;
           attribute EventHandler         onpaymentmethodchange;
};
