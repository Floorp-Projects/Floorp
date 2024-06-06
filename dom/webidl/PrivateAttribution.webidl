/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

enum PrivateAttributionImpressionType { "view", "click" };

dictionary PrivateAttributionImpressionOptions {
  PrivateAttributionImpressionType type = "view";
  required unsigned long index;
  required DOMString ad;
  required UTF8String target;
};

dictionary PrivateAttributionConversionOptions {
  required DOMString task;
  required unsigned long histogramSize;

  unsigned long lookbackDays;
  PrivateAttributionImpressionType impression;
  sequence<DOMString> ads = [];
  sequence<UTF8String> sources = [];
};

[Trial="PrivateAttribution", SecureContext, Exposed=Window]
interface PrivateAttribution {
  [Throws] undefined saveImpression(PrivateAttributionImpressionOptions options);
  [Throws] undefined measureConversion(PrivateAttributionConversionOptions options);
};
