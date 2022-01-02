/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * For more information on this interface, please see
 * https://w3c.github.io/webappsec-feature-policy/#idl-index
 */

[LegacyNoInterfaceObject,
 Exposed=Window]
interface FeaturePolicy {
  boolean allowsFeature(DOMString feature, optional DOMString origin);
  sequence<DOMString> features();
  sequence<DOMString> allowedFeatures();
  sequence<DOMString> getAllowlistForFeature(DOMString feature);
};

[Pref="dom.reporting.featurePolicy.enabled",
 Exposed=Window]
interface FeaturePolicyViolationReportBody : ReportBody {
  readonly attribute DOMString featureId;
  readonly attribute DOMString? sourceFile;
  readonly attribute long? lineNumber;
  readonly attribute long? columnNumber;
  readonly attribute DOMString disposition;
};
