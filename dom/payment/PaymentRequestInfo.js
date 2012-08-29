/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const PAYMENTREQUESTINFO_CID =
  Components.ID("{0a58c67d-f003-48da-81d1-bd8f605f4b1c}");
const PAYMENTPRODUCTPRICE_CID =
  Components.ID("{3d7dcabf-b77c-4bb3-b225-46011898ec32}");
const PAYMENTREQUESTPAYMENTINFO_CID =
  Components.ID("{7f2e3274-3956-42e1-b7ce-59b8cd23d177}");
const PAYMENTREQUESTREFUNDINFO_CID =
  Components.ID("{e75566c6-dfb1-4f6b-b21d-078536c883b0}");

// nsIDOMPaymentProductPrice

function PaymentProductPrice() {
  this.wrappedJSObject = this;
};

PaymentProductPrice.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMPaymentProductPrice]),
  classID: PAYMENTPRODUCTPRICE_CID,
  classInfo: XPCOMUtils.generateCI({
    classID: PAYMENTPRODUCTPRICE_CID,
    contractID: "@mozilla.org/payment/product-price;1",
    classDescription: "Payment product price",
    flags: Ci.nsIClassInfo.DOM_OBJECT,
    interfaces: [Ci.nsIDOMPaymentProductPrice]
  }),
  country: null,
  currency: null,
  amount: null,

  init: function init(aCountry, aCurrency, aAmount) {
    this.country = aCountry;
    this.currency = aCurrency;
    this.amount = aAmount;
  }
};


// nsIDOMPaymentRequestInfo

function PaymentRequestInfo() {
};

PaymentRequestInfo.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMPaymentRequestInfo]),
  classID: PAYMENTREQUESTINFO_CID,
  classInfo: XPCOMUtils.generateCI({
    classID: PAYMENTREQUESTINFO_CID,
    contractID: "@mozilla.org/payment/request-info;1",
    classDescription: "Payment request information",
    flags: Ci.nsIClassInfo.DOM_OBJECT,
    interfaces: [Ci.nsIDOMPaymentRequestInfo]
  }),
  jwt: null,
  type: null,
  providerName: null,

  initRequest: function initRequest(aJwt, aType, aProviderName) {
    this.jwt = aJwt;
    this.type = aType;
    this.providerName = aProviderName;
  }
};

// nsIDOMPaymentRequestPaymentInfo

function PaymentRequestPaymentInfo() {
  this.wrappedJSObject = this;
};

PaymentRequestPaymentInfo.prototype = {
  __proto__: PaymentRequestInfo.prototype,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMPaymentRequestPaymentInfo]),
  classID: PAYMENTREQUESTPAYMENTINFO_CID,
  classInfo: XPCOMUtils.generateCI({
    classID: PAYMENTREQUESTPAYMENTINFO_CID,
    contractID: "@mozilla.org/payment/request-payment-info;1",
    classDescription: "Payment request payment information",
    flags: Ci.nsIClassInfo.DOM_OBJECT,
    interfaces: [Ci.nsIDOMPaymentRequestPaymentInfo]
  }),
  productName: null,
  productDescription: null,
  productPrice: null,

  init: function init(aJwt, aType, aProviderName,
                      aProductName, aProductDescription, aProductPrice) {
    this.__proto__.initRequest.call(this, aJwt, aType, aProviderName);
    this.productName = aProductName;
    this.productDescription = aProductDescription;
    this.productPrice = aProductPrice;
  }
};

// nsIDOMPaymentRequestRefundInfo

function PaymentRequestRefundInfo() {
  this.wrappedJSObject = this;
};

PaymentRequestRefundInfo.prototype = {
  __proto__: PaymentRequestInfo.prototype,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMPaymentRequestRefundInfo]),
  classID: PAYMENTREQUESTREFUNDINFO_CID,
  classInfo: XPCOMUtils.generateCI({
    classID: PAYMENTREQUESTREFUNDINFO_CID,
    contractID: "@mozilla.org/payment/request-refund-info;1",
    classDescription: "Payment request refund information",
    flags: Ci.nsIClassInfo.DOM_OBJECT,
    interfaces: [Ci.nsIDOMPaymentRequestRefundInfo]
  }),
  reason: null,

  init: function init(aJwt, aType, aProviderName, aReason) {
    this.__proto__.initRequest.call(this, aJwt, aType, aProviderName);
    this.reason = aReason;
  }
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([
  PaymentProductPrice,
  PaymentRequestInfo,
  PaymentRequestPaymentInfo,
  PaymentRequestRefundInfo
]);
