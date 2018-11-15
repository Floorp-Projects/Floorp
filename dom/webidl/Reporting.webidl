/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/reporting/#interface-reporting-observer
 */

[Func="mozilla::dom::DOMPrefs::dom_reporting_enabled"]
interface ReportBody {
};

[Func="mozilla::dom::DOMPrefs::dom_reporting_enabled"]
interface Report {
  readonly attribute DOMString type;
  readonly attribute DOMString url;
  readonly attribute ReportBody? body;
};

[Constructor(ReportingObserverCallback callback, optional ReportingObserverOptions options),
 Func="mozilla::dom::DOMPrefs::dom_reporting_enabled"]
interface ReportingObserver {
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

[Func="mozilla::dom::DOMPrefs::dom_reporting_enabled"]
interface DeprecationReportBody : ReportBody {
  readonly attribute DOMString id;
  readonly attribute Date? anticipatedRemoval;
  readonly attribute DOMString message;
  readonly attribute DOMString? sourceFile;
  readonly attribute unsigned long? lineNumber;
  readonly attribute unsigned long? columnNumber;
};

[Constructor(), Deprecated="DeprecatedTestingInterface",
 Func="mozilla::dom::DOMPrefs::dom_reporting_testing_enabled",
 Exposed=(Window,DedicatedWorker)]
interface TestingDeprecatedInterface {
  [Deprecated="DeprecatedTestingMethod"]
  void deprecatedMethod();

  [Deprecated="DeprecatedTestingAttribute"]
  readonly attribute boolean deprecatedAttribute;
};
