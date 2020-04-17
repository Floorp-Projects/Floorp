let testingInterface;

// eslint-disable-next-line no-unused-vars
function test_deprecatedInterface() {
  info("Testing DeprecatedTestingInterface report");
  return new Promise(resolve => {
    let obs = new ReportingObserver((reports, o) => {
      is(obs, o, "Same observer!");
      ok(reports.length == 1, "We have 1 report");

      let report = reports[0];
      is(report.type, "deprecation", "Deprecation report received");
      is(report.url, location.href, "URL is location");
      ok(!!report.body, "The report has a body");
      ok(
        report.body instanceof DeprecationReportBody,
        "Correct type for the body"
      );
      is(
        report.body.id,
        "DeprecatedTestingInterface",
        "report.body.id matches DeprecatedTestingMethod"
      );
      ok(!report.body.anticipatedRemoval, "We don't have a anticipatedRemoval");
      ok(
        report.body.message.includes("TestingDeprecatedInterface"),
        "We have a message"
      );
      is(
        report.body.sourceFile,
        location.href
          .replace("test_deprecated.html", "common_deprecated.js")
          .replace("worker_deprecated.js", "common_deprecated.js"),
        "We have a sourceFile"
      );
      is(report.body.lineNumber, 47, "We have a lineNumber");
      is(report.body.columnNumber, 23, "We have a columnNumber");

      obs.disconnect();
      resolve();
    });
    ok(!!obs, "ReportingObserver is a thing");

    obs.observe();
    ok(true, "ReportingObserver.observe() is callable");

    testingInterface = new TestingDeprecatedInterface();
    ok(true, "Created a deprecated interface");
  });
}

// eslint-disable-next-line no-unused-vars
function test_deprecatedMethod() {
  info("Testing DeprecatedTestingMethod report");
  return new Promise(resolve => {
    let obs = new ReportingObserver((reports, o) => {
      is(obs, o, "Same observer!");
      ok(reports.length == 1, "We have 1 report");

      let report = reports[0];
      is(report.type, "deprecation", "Deprecation report received");
      is(report.url, location.href, "URL is location");
      ok(!!report.body, "The report has a body");
      ok(
        report.body instanceof DeprecationReportBody,
        "Correct type for the body"
      );
      is(
        report.body.id,
        "DeprecatedTestingMethod",
        "report.body.id matches DeprecatedTestingMethod"
      );
      ok(!report.body.anticipatedRemoval, "We don't have a anticipatedRemoval");
      ok(
        report.body.message.includes(
          "TestingDeprecatedInterface.deprecatedMethod"
        ),
        "We have a message"
      );
      is(
        report.body.sourceFile,
        location.href
          .replace("test_deprecated.html", "common_deprecated.js")
          .replace("worker_deprecated.js", "common_deprecated.js"),
        "We have a sourceFile"
      );
      is(report.body.lineNumber, 98, "We have a lineNumber");
      is(report.body.columnNumber, 21, "We have a columnNumber");

      obs.disconnect();
      resolve();
    });
    ok(!!obs, "ReportingObserver is a thing");

    obs.observe();
    ok(true, "ReportingObserver.observe() is callable");

    testingInterface.deprecatedMethod();
    ok(true, "Run a deprecated method.");
  });
}

// eslint-disable-next-line no-unused-vars
function test_deprecatedAttribute() {
  info("Testing DeprecatedTestingAttribute report");
  return new Promise(resolve => {
    let obs = new ReportingObserver((reports, o) => {
      is(obs, o, "Same observer!");
      ok(reports.length == 1, "We have 1 report");

      let report = reports[0];
      is(report.type, "deprecation", "Deprecation report received");
      is(report.url, location.href, "URL is location");
      ok(!!report.body, "The report has a body");
      ok(
        report.body instanceof DeprecationReportBody,
        "Correct type for the body"
      );
      is(
        report.body.id,
        "DeprecatedTestingAttribute",
        "report.body.id matches DeprecatedTestingAttribute"
      );
      ok(!report.body.anticipatedRemoval, "We don't have a anticipatedRemoval");
      ok(
        report.body.message.includes(
          "TestingDeprecatedInterface.deprecatedAttribute"
        ),
        "We have a message"
      );
      is(
        report.body.sourceFile,
        location.href
          .replace("test_deprecated.html", "common_deprecated.js")
          .replace("worker_deprecated.js", "common_deprecated.js"),
        "We have a sourceFile"
      );
      is(report.body.lineNumber, 149, "We have a lineNumber");
      is(report.body.columnNumber, 4, "We have a columnNumber");

      obs.disconnect();
      resolve();
    });
    ok(!!obs, "ReportingObserver is a thing");

    obs.observe();
    ok(true, "ReportingObserver.observe() is callable");

    ok(testingInterface.deprecatedAttribute, "Attributed called");
  });
}

// eslint-disable-next-line no-unused-vars
function test_takeRecords() {
  info("Testing ReportingObserver.takeRecords()");
  let p = new Promise(resolve => {
    let obs = new ReportingObserver((reports, o) => {
      is(obs, o, "Same observer!");
      resolve(obs);
    });
    ok(!!obs, "ReportingObserver is a thing");

    obs.observe();
    ok(true, "ReportingObserver.observe() is callable");

    testingInterface.deprecatedMethod();
    ok(true, "Run a deprecated method.");
  });

  return p.then(obs => {
    let reports = obs.takeRecords();
    is(reports.length, 0, "No reports after an callback");

    testingInterface.deprecatedAttribute + 1;

    reports = obs.takeRecords();
    ok(reports.length >= 1, "We have at least 1 report");

    reports = obs.takeRecords();
    is(reports.length, 0, "No more reports");
  });
}
