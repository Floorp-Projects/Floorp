/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/reporting/#interface-reporting-observer
 */

[Pref="dom.reporting.enabled"]
interface ReportBody {
};

[Pref="dom.reporting.enabled"]
interface Report {
  readonly attribute DOMString type;
  readonly attribute DOMString url;
  readonly attribute ReportBody? body;
};

[Pref="dom.reporting.enabled"]
interface ReportingObserver {
  [Throws]
  constructor(ReportingObserverCallback callback,
              optional ReportingObserverOptions options = {});

  void observe();
  void disconnect();
  ReportList takeRecords();
};

callback ReportingObserverCallback = void (sequence<Report> reports, ReportingObserver observer);

dictionary ReportingObserverOptions {
  sequence<DOMString> types;
  boolean buffered = false;
};

typedef sequence<Report> ReportList;

[Pref="dom.reporting.enabled"]
interface DeprecationReportBody : ReportBody {
  readonly attribute DOMString id;
  readonly attribute Date? anticipatedRemoval;
  readonly attribute DOMString message;
  readonly attribute DOMString? sourceFile;
  readonly attribute unsigned long? lineNumber;
  readonly attribute unsigned long? columnNumber;
};

[Deprecated="DeprecatedTestingInterface",
 Pref="dom.reporting.testing.enabled",
 Exposed=(Window,DedicatedWorker)]
interface TestingDeprecatedInterface {
  constructor();

  [Deprecated="DeprecatedTestingMethod"]
  void deprecatedMethod();

  [Deprecated="DeprecatedTestingAttribute"]
  readonly attribute boolean deprecatedAttribute;
};

// Used internally to process the JSON
dictionary ReportingHeaderValue {
  sequence<ReportingItem> items;
};

// Used internally to process the JSON
dictionary ReportingItem {
  // This is a long.
  any max_age;
  // This is a sequence of ReportingEndpoint.
  any endpoints;
  // This is a string. If missing, the value is 'default'.
  any group;
  boolean include_subdomains = false;
};

// Used internally to process the JSON
dictionary ReportingEndpoint {
  // This is a string.
  any url;
  // This is an unsigned long.
  any priority;
  // This is an unsigned long.
  any weight;
};
