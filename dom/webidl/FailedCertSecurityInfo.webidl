/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * This dictionary is used for exposing failed channel certificate information
 * to about:certerror to display information.
 */

dictionary FailedCertSecurityInfo {
  DOMString errorCodeString = "";
  boolean isUntrusted = false;
  boolean isDomainMismatch = false;
  boolean isNotValidAtThisTime = false;
  DOMTimeStamp validNotBefore = 0;
  DOMTimeStamp validNotAfter = 0;
  DOMString issuerCommonName = "";
  DOMTimeStamp certValidityRangeNotAfter = 0;
  DOMTimeStamp certValidityRangeNotBefore = 0;
  DOMString errorMessage = "";
  boolean hasHSTS = true;
  boolean hasHPKP = true;
  sequence<DOMString> certChainStrings;
};
