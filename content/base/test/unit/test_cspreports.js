/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import('resource://gre/modules/CSPUtils.jsm');
Components.utils.import('resource://gre/modules/NetUtil.jsm');

// load the HTTP server
do_load_httpd_js();

const REPORT_SERVER_PORT = 9000;
const REPORT_SERVER_URI = "http://localhost";
const REPORT_SERVER_PATH = "/report";

var httpServer = null;
var testsToFinish = 0;

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

    // self-destroy
    testsToFinish--;
    httpServer.registerPathHandler(testpath, null);
    if (testsToFinish < 1)
      httpServer.stop(do_test_finished);
    else
      do_test_finished();
  };
}

function makeTest(id, expectedJSON, callback) {
  testsToFinish++;
  do_test_pending();

  // set up a new CSP instance for each test.
  var csp = Cc["@mozilla.org/contentsecuritypolicy;1"]
              .createInstance(Ci.nsIContentSecurityPolicy);
  var policy = "allow 'none'; " +
               "report-uri " + REPORT_SERVER_URI +
                               ":" + REPORT_SERVER_PORT +
                               "/test" + id;
  var selfuri = NetUtil.newURI(REPORT_SERVER_URI +
                               ":" + REPORT_SERVER_PORT +
                               "/foo/self");
  var selfchan = NetUtil.newChannel(selfuri);

  dump("Created test " + id + " : " + policy + "\n\n");

  // make the reports seem authentic by "binding" them to a channel.
  csp.scanRequestData(selfchan);

  // Load up the policy
  csp.refinePolicy(policy, selfuri);

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

  httpServer = new nsHttpServer();
  httpServer.start(REPORT_SERVER_PORT);

  // test that inline script violations cause a report.
  makeTest(0, {"blocked-uri": "self"},
      function(csp) {
        if(!csp.allowsInlineScript) {
          // force the logging, since the getter doesn't.
          csp.logViolationDetails(Ci.nsIContentSecurityPolicy.VIOLATION_TYPE_INLINE_SCRIPT,
                                  selfuri.asciiSpec,
                                  "script sample",
                                  0);
        }
      });

  makeTest(1, {"blocked-uri": "self"},
      function(csp) {
        if(!csp.allowsEval) {
          // force the logging, since the getter doesn't.
          csp.logViolationDetails(Ci.nsIContentSecurityPolicy.VIOLATION_TYPE_INLINE_SCRIPT,
                                  selfuri.asciiSpec,
                                  "script sample",
                                  1);
        }
      });

  makeTest(2, {"blocked-uri": "http://blocked.test/foo.js"},
      function(csp) {
        csp.shouldLoad(Ci.nsIContentPolicy.TYPE_SCRIPT,
                      NetUtil.newURI("http://blocked.test/foo.js"),
                      null, null, null, null);
      });
}
