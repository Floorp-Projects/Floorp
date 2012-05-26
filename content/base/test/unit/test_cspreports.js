/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Content Security Policy Data Structures testing code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation
 *
 * Contributor(s):
 *   Sid Stamm <sid@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
