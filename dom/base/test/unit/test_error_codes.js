/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

var prefs = Services.prefs;

function asyncXHR(expectedStatus, nextTestFunc) {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "http://localhost:4444/test_error_code.xml", true);

  var sawError = false;
  xhr.addEventListener("loadend", function doAsyncRequest_onLoad(event) {
    Assert.ok(sawError, "Should have received an error");
    nextTestFunc();
  });
  xhr.addEventListener("error", function doAsyncRequest_onError(event) {
    var request = event.target.channel.QueryInterface(Ci.nsIRequest);
    Assert.equal(request.status, expectedStatus);
    sawError = true;
  });
  xhr.send(null);
}

function run_test() {
  do_test_pending();
  do_timeout(0, run_test_pt1);
}

// network offline
function run_test_pt1() {
  try {
    Services.io.manageOfflineStatus = false;
  } catch (e) {}
  Services.io.offline = true;
  prefs.setBoolPref("network.dns.offline-localhost", false);
  // We always resolve localhost as it's hardcoded without the following pref:
  prefs.setBoolPref("network.proxy.allow_hijacking_localhost", true);

  dump("Testing error returned by async XHR when the network is offline\n");
  asyncXHR(Cr.NS_ERROR_OFFLINE, run_test_pt2);
}

// connection refused
function run_test_pt2() {
  Services.io.offline = false;
  prefs.clearUserPref("network.dns.offline-localhost");
  prefs.clearUserPref("network.proxy.allow_hijacking_localhost");

  dump("Testing error returned by aync XHR when the connection is refused\n");
  asyncXHR(Cr.NS_ERROR_CONNECTION_REFUSED, end_test);
}

function end_test() {
  do_test_finished();
}
