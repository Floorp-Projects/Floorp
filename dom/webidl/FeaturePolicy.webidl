/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * For more information on this interface, please see
 * https://wicg.github.io/feature-policy/#policy
 */

[NoInterfaceObject]
interface Policy {
  boolean allowsFeature(DOMString feature, optional DOMString origin);
  sequence<DOMString> allowedFeatures();
  sequence<DOMString> getAllowlistForFeature(DOMString feature);
};

[Func="mozilla::dom::DOMPrefs::dom_reporting_featurePolicy_enabled"]
interface FeaturePolicyViolationReportBody : ReportBody {
  readonly attribute DOMString featureId;
  readonly attribute DOMString? sourceFile;
  readonly attribute long? lineNumber;
  readonly attribute long? columnNumber;
  readonly attribute DOMString disposition;
};
