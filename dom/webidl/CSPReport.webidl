/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
  * This dictionary holds the parameters used to send
  * CSP reports in JSON format.
  *
  * Based on https://w3c.github.io/webappsec-csp/#deprecated-serialize-violation
  */

dictionary CSPReportProperties {
  DOMString document-uri = "";
  DOMString referrer = "";
  DOMString blocked-uri = "";
  DOMString effective-directive = "";
  DOMString violated-directive = "";
  DOMString original-policy= "";
  SecurityPolicyViolationEventDisposition disposition = "report";
  long status-code = 0;

  DOMString source-file;
  DOMString script-sample;
  long line-number;
  long column-number;
};

[GenerateToJSON]
dictionary CSPReport {
  // We always want to have a "csp-report" property, so just pre-initialize it
  // to an empty dictionary..
  CSPReportProperties csp-report = {};
};
