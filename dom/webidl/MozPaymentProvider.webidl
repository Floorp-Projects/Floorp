/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

callback SilentSmsCallback = void (optional MozSmsMessage message);

dictionary PaymentIccInfo {
  DOMString mcc;
  DOMString mnc;
  DOMString iccId;
  boolean dataPrimary;
};

[NavigatorProperty="mozPaymentProvider",
 NoInterfaceObject,
 HeaderFile="mozilla/dom/PaymentProviderUtils.h",
 Func="mozilla::dom::PaymentProviderUtils::EnabledForScope",
 JSImplementation="@mozilla.org/payment/provider;1"]
interface PaymentProvider {
  readonly attribute DOMString? paymentServiceId;
  // We expose to the payment provider the information of all the SIMs
  // available in the device.
  [Cached, Pure] readonly attribute sequence<PaymentIccInfo>? iccInfo;

  void paymentSuccess(optional DOMString result);
  void paymentFailed(optional DOMString error);

  DOMRequest sendSilentSms(DOMString number, DOMString message);
  void observeSilentSms(DOMString number, SilentSmsCallback callback);
  void removeSilentSmsObserver(DOMString number, SilentSmsCallback callback);
};
