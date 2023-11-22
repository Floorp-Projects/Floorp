/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { NetUtil } = ChromeUtils.importESModule(
  "resource://gre/modules/NetUtil.sys.mjs"
);
const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

var httpServer = new HttpServer();
httpServer.start(-1);
var testsToFinish = 0;

var principal;

const REPORT_SERVER_PORT = httpServer.identity.primaryPort;
const REPORT_SERVER_URI = "http://localhost";

/**
 * Construct a callback that listens to a report submission and either passes
 * or fails a test based on what it gets.
 */
function makeReportHandler(testpath, message, expectedJSON) {
  return function (request, response) {
    // we only like "POST" submissions for reports!
    if (request.method !== "POST") {
      do_throw("violation report should be a POST request");
      return;
    }

    // check content-type of report is "application/csp-report"
    var contentType = request.hasHeader("Content-Type")
      ? request.getHeader("Content-Type")
      : undefined;
    if (contentType !== "application/csp-report") {
      do_throw(
        "violation report should have the 'application/csp-report' " +
          "content-type, when in fact it is " +
          contentType.toString()
      );
    }

    // obtain violation report
    var reportObj = JSON.parse(
      NetUtil.readInputStreamToString(
        request.bodyInputStream,
        request.bodyInputStream.available()
      )
    );

    // dump("GOT REPORT:\n" + JSON.stringify(reportObj) + "\n");
    // dump("TESTPATH:    " + testpath + "\n");
    // dump("EXPECTED:  \n" + JSON.stringify(expectedJSON) + "\n\n");

    for (var i in expectedJSON) {
      Assert.equal(expectedJSON[i], reportObj["csp-report"][i]);
    }

    testsToFinish--;
    httpServer.registerPathHandler(testpath, null);
    if (testsToFinish < 1) {
      httpServer.stop(do_test_finished);
    } else {
      do_test_finished();
    }
  };
}

/**
 * Everything created by this assumes it will cause a report.  If you want to
 * add a test here that will *not* cause a report to go out, you're gonna have
 * to make sure the test cleans up after itself.
 */
function makeTest(id, expectedJSON, useReportOnlyPolicy, callback) {
  testsToFinish++;
  do_test_pending();

  // set up a new CSP instance for each test.
  var csp = Cc["@mozilla.org/cspcontext;1"].createInstance(
    Ci.nsIContentSecurityPolicy
  );
  var policy =
    "default-src 'none' 'report-sample'; " +
    "report-uri " +
    REPORT_SERVER_URI +
    ":" +
    REPORT_SERVER_PORT +
    "/test" +
    id;
  var selfuri = NetUtil.newURI(
    REPORT_SERVER_URI + ":" + REPORT_SERVER_PORT + "/foo/self"
  );

  dump("Created test " + id + " : " + policy + "\n\n");

  principal = Services.scriptSecurityManager.createContentPrincipal(
    selfuri,
    {}
  );
  csp.setRequestContextWithPrincipal(principal, selfuri, "", 0);

  // Load up the policy
  // set as report-only if that's the case
  csp.appendPolicy(policy, useReportOnlyPolicy, false);

  // prime the report server
  var handler = makeReportHandler("/test" + id, "Test " + id, expectedJSON);
  httpServer.registerPathHandler("/test" + id, handler);

  // trigger the violation
  callback(csp);
}

function run_test() {
  do_get_profile();

  var selfuri = NetUtil.newURI(
    REPORT_SERVER_URI + ":" + REPORT_SERVER_PORT + "/foo/self"
  );

  // test that inline script violations cause a report.
  makeTest(
    0,
    {
      "blocked-uri": "inline",
      "effective-directive": "script-src-elem",
      disposition: "enforce",
    },
    false,
    function (csp) {
      let inlineOK = true;
      inlineOK = csp.getAllowsInline(
        Ci.nsIContentSecurityPolicy.SCRIPT_SRC_ELEM_DIRECTIVE,
        false, // aHasUnsafeHash
        "", // aNonce
        false, // aParserCreated
        null, // aTriggeringElement
        null, // nsICSPEventListener
        "", // aContentOfPseudoScript
        0, // aLineNumber
        1 // aColumnNumber
      );

      // this is not a report only policy, so it better block inline scripts
      Assert.ok(!inlineOK);
    }
  );

  // test that eval violations cause a report.
  makeTest(
    1,
    {
      "blocked-uri": "eval",
      // JSON script-sample is UTF8 encoded
      "script-sample": "\xc2\xa3\xc2\xa5\xc2\xb5\xe5\x8c\x97\xf0\xa0\x9d\xb9",
      "line-number": 1,
      "column-number": 2,
    },
    false,
    function (csp) {
      let evalOK = true,
        oReportViolation = { value: false };
      evalOK = csp.getAllowsEval(oReportViolation);

      // this is not a report only policy, so it better block eval
      Assert.ok(!evalOK);
      // ... and cause reports to go out
      Assert.ok(oReportViolation.value);

      if (oReportViolation.value) {
        // force the logging, since the getter doesn't.
        csp.logViolationDetails(
          Ci.nsIContentSecurityPolicy.VIOLATION_TYPE_EVAL,
          null, // aTriggeringElement
          null, // nsICSPEventListener
          selfuri.asciiSpec,
          // sending UTF-16 script sample to make sure
          // csp report in JSON is not cut-off, please
          // note that JSON is UTF8 encoded.
          "\u00a3\u00a5\u00b5\u5317\ud841\udf79",
          1, // line number
          2 // column number
        );
      }
    }
  );

  makeTest(
    2,
    { "blocked-uri": "http://blocked.test/foo.js" },
    false,
    function (csp) {
      // shouldLoad creates and sends out the report here.
      csp.shouldLoad(
        Ci.nsIContentPolicy.TYPE_SCRIPT,
        null, // nsICSPEventListener
        null, // aLoadInfo
        NetUtil.newURI("http://blocked.test/foo.js"),
        null,
        true
      );
    }
  );

  // test that inline script violations cause a report in report-only policy
  makeTest(
    3,
    { "blocked-uri": "inline", disposition: "report" },
    true,
    function (csp) {
      let inlineOK = true;
      inlineOK = csp.getAllowsInline(
        Ci.nsIContentSecurityPolicy.SCRIPT_SRC_ELEM_DIRECTIVE,
        false, // aHasUnsafeHash
        "", // aNonce
        false, // aParserCreated
        null, // aTriggeringElement
        null, // nsICSPEventListener
        "", // aContentOfPseudoScript
        0, // aLineNumber
        1 // aColumnNumber
      );

      // this is a report only policy, so it better allow inline scripts
      Assert.ok(inlineOK);
    }
  );

  // test that eval violations cause a report in report-only policy
  makeTest(4, { "blocked-uri": "eval" }, true, function (csp) {
    let evalOK = true,
      oReportViolation = { value: false };
    evalOK = csp.getAllowsEval(oReportViolation);

    // this is a report only policy, so it better allow eval
    Assert.ok(evalOK);
    // ... but still cause reports to go out
    Assert.ok(oReportViolation.value);

    if (oReportViolation.value) {
      // force the logging, since the getter doesn't.
      csp.logViolationDetails(
        Ci.nsIContentSecurityPolicy.VIOLATION_TYPE_EVAL,
        null, // aTriggeringElement
        null, // nsICSPEventListener
        selfuri.asciiSpec,
        "script sample",
        4, // line number
        5 // column number
      );
    }
  });

  // test that only the uri's scheme is reported for globally unique identifiers
  makeTest(5, { "blocked-uri": "data" }, false, function (csp) {
    var base64data =
      "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12" +
      "P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==";
    // shouldLoad creates and sends out the report here.
    csp.shouldLoad(
      Ci.nsIContentPolicy.TYPE_IMAGE,
      null, // nsICSPEventListener
      null, // nsILoadInfo
      NetUtil.newURI("data:image/png;base64," + base64data),
      null,
      true
    );
  });

  // test that only the uri's scheme is reported for globally unique identifiers
  makeTest(6, { "blocked-uri": "intent" }, false, function (csp) {
    // shouldLoad creates and sends out the report here.
    csp.shouldLoad(
      Ci.nsIContentPolicy.TYPE_SUBDOCUMENT,
      null, // nsICSPEventListener
      null, // nsILoadInfo
      NetUtil.newURI("intent://mymaps.com/maps?um=1&ie=UTF-8&fb=1&sll"),
      null,
      true
    );
  });

  // test fragment removal
  var selfSpec =
    REPORT_SERVER_URI + ":" + REPORT_SERVER_PORT + "/foo/self/foo.js";
  makeTest(7, { "blocked-uri": selfSpec }, false, function (csp) {
    // shouldLoad creates and sends out the report here.
    csp.shouldLoad(
      Ci.nsIContentPolicy.TYPE_SCRIPT,
      null, // nsICSPEventListener
      null, // nsILoadInfo
      NetUtil.newURI(selfSpec + "#bar"),
      null,
      true
    );
  });
}
