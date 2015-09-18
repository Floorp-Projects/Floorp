/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import('resource://gre/modules/NetUtil.jsm');
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://testing-common/httpd.js");

var httpServer = new HttpServer();
httpServer.start(-1);
var testsToFinish = 0;

const REPORT_SERVER_PORT = httpServer.identity.primaryPort;
const REPORT_SERVER_URI = "http://localhost";
const REPORT_SERVER_PATH = "/report";

/**
 * Construct a callback that listens to a report submission and either passes
 * or fails a test based on what it gets.
 */
function makeReportHandler(testpath, message, expectedJSON) {
  return function(request, response) {
    // we only like "POST" submissions for reports!
    if (request.method !== "POST") {
      do_throw("violation report should be a POST request");
      return;
    }

    // obtain violation report
    var reportObj = JSON.parse(
          NetUtil.readInputStreamToString(
            request.bodyInputStream,
            request.bodyInputStream.available()));

    dump("GOT REPORT:\n" + JSON.stringify(reportObj) + "\n");
    dump("TESTPATH:    " + testpath + "\n");
    dump("EXPECTED:  \n" + JSON.stringify(expectedJSON) + "\n\n");

    for (var i in expectedJSON)
      do_check_eq(expectedJSON[i], reportObj['csp-report'][i]);

    testsToFinish--;
    httpServer.registerPathHandler(testpath, null);
    if (testsToFinish < 1)
      httpServer.stop(do_test_finished);
    else
      do_test_finished();
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
  var csp = Cc["@mozilla.org/cspcontext;1"]
              .createInstance(Ci.nsIContentSecurityPolicy);
  var policy = "default-src 'none'; " +
               "report-uri " + REPORT_SERVER_URI +
                               ":" + REPORT_SERVER_PORT +
                               "/test" + id;
  var selfuri = NetUtil.newURI(REPORT_SERVER_URI +
                               ":" + REPORT_SERVER_PORT +
                               "/foo/self");
  var selfchan = NetUtil.newChannel({
    uri: selfuri,
    loadUsingSystemPrincipal: true});

  dump("Created test " + id + " : " + policy + "\n\n");

  // make the reports seem authentic by "binding" them to a channel.
  csp.setRequestContext(selfuri, null, selfchan);

  // Load up the policy
  // set as report-only if that's the case
  csp.appendPolicy(policy, useReportOnlyPolicy);

  // prime the report server
  var handler = makeReportHandler("/test" + id, "Test " + id, expectedJSON);
  httpServer.registerPathHandler("/test" + id, handler);

  //trigger the violation
  callback(csp);
}

function run_test() {
  var selfuri = NetUtil.newURI(REPORT_SERVER_URI +
                               ":" + REPORT_SERVER_PORT +
                               "/foo/self");

  // test that inline script violations cause a report.
  makeTest(0, {"blocked-uri": "self"}, false,
      function(csp) {
        let inlineOK = true;
        inlineOK = csp.getAllowsInline(Ci.nsIContentPolicy.TYPE_SCRIPT,
                                       "", // aNonce
                                       "", // aContent
                                       0); // aLineNumber

        // this is not a report only policy, so it better block inline scripts
        do_check_false(inlineOK);
      });

  // test that eval violations cause a report.
  makeTest(1, {"blocked-uri": "self"}, false,
      function(csp) {
        let evalOK = true, oReportViolation = {'value': false};
        evalOK = csp.getAllowsEval(oReportViolation);

        // this is not a report only policy, so it better block eval
        do_check_false(evalOK);
        // ... and cause reports to go out
        do_check_true(oReportViolation.value);

        if (oReportViolation.value) {
          // force the logging, since the getter doesn't.
          csp.logViolationDetails(Ci.nsIContentSecurityPolicy.VIOLATION_TYPE_EVAL,
                                  selfuri.asciiSpec,
                                  "script sample",
                                  1);
        }
      });

  makeTest(2, {"blocked-uri": "http://blocked.test/foo.js"}, false,
      function(csp) {
        // shouldLoad creates and sends out the report here.
        csp.shouldLoad(Ci.nsIContentPolicy.TYPE_SCRIPT,
                      NetUtil.newURI("http://blocked.test/foo.js"),
                      null, null, null, null);
      });

  // test that inline script violations cause a report in report-only policy
  makeTest(3, {"blocked-uri": "self"}, true,
      function(csp) {
        let inlineOK = true;
        inlineOK = csp.getAllowsInline(Ci.nsIContentPolicy.TYPE_SCRIPT,
                                       "", // aNonce
                                       "", // aContent
                                       0); // aLineNumber

        // this is a report only policy, so it better allow inline scripts
        do_check_true(inlineOK);
      });

  // test that eval violations cause a report in report-only policy
  makeTest(4, {"blocked-uri": "self"}, true,
      function(csp) {
        let evalOK = true, oReportViolation = {'value': false};
        evalOK = csp.getAllowsEval(oReportViolation);

        // this is a report only policy, so it better allow eval
        do_check_true(evalOK);
        // ... but still cause reports to go out
        do_check_true(oReportViolation.value);

        if (oReportViolation.value) {
          // force the logging, since the getter doesn't.
          csp.logViolationDetails(Ci.nsIContentSecurityPolicy.VIOLATION_TYPE_INLINE_SCRIPT,
                                  selfuri.asciiSpec,
                                  "script sample",
                                  4);
        }
      });
}
