/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

var gExpectedStatus = null;
var gNextTestFunc   = null;

var prefs = Components.classes["@mozilla.org/preferences-service;1"].
    getService(Components.interfaces.nsIPrefBranch);

var asyncXHR = {
  load: function() {
    var request = Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"]
                            .createInstance(Components.interfaces.nsIXMLHttpRequest);
    request.open("GET", "http://localhost:4444/test_error_code.xml", true);

    var self = this;
    request.addEventListener("error", function(event) { self.onError(event); }, false);
    request.send(null);
  },
  onError: function doAsyncRequest_onError(event) {
    var request = event.target.channel.QueryInterface(Components.interfaces.nsIRequest);
    do_check_eq(request.status, gExpectedStatus);
    gNextTestFunc();
  }
}

function run_test() {
  do_test_pending();
  do_timeout(0, run_test_pt1);
}

// network offline
function run_test_pt1() {
  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                            .getService(Components.interfaces.nsIIOService);

  try {
    ioService.manageOfflineStatus = false;
  }
  catch (e) {
  }
  ioService.offline = true;
  prefs.setBoolPref("network.dns.offline-localhost", false);

  gExpectedStatus = Components.results.NS_ERROR_OFFLINE;
  gNextTestFunc = run_test_pt2;
  dump("Testing error returned by async XHR when the network is offline\n");
  asyncXHR.load();
}

// connection refused
function run_test_pt2() {
  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                            .getService(Components.interfaces.nsIIOService);
  ioService.offline = false;
  prefs.clearUserPref("network.dns.offline-localhost");

  gExpectedStatus = Components.results.NS_ERROR_CONNECTION_REFUSED;
  gNextTestFunc = end_test;
  dump("Testing error returned by aync XHR when the connection is refused\n");
  asyncXHR.load();
}

function end_test() {
  do_test_finished();
}
