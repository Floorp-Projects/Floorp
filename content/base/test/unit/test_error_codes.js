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
 * The Original Code is Mozilla.org.
 *
 * The Initial Developer of the Original Code is
 * Robert Strong <robert.bugzilla@gmail.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Mozilla Foundation <http://www.mozilla.org/>. All Rights Reserved.
 *
 * Contributor(s):
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
 * ***** END LICENSE BLOCK *****
 */

var gExpectedStatus = null;
var gNextTestFunc   = null;

var asyncXHR = {
  load: function() {
    var request = Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"]
                            .createInstance(Components.interfaces.nsIXMLHttpRequest);
    request.open("GET", "http://localhost:4444/test_error_code.xml", true);

    var self = this;
    request.onerror = function(event) { self.onError(event); };
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
  do_timeout(0, "run_test_pt1()");
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

  gExpectedStatus = Components.results.NS_ERROR_DOCUMENT_NOT_CACHED;
  gNextTestFunc = run_test_pt2;
  dump("Testing error returned by async XHR when the network is offline\n");
  asyncXHR.load();
}

// connection refused
function run_test_pt2() {
  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                            .getService(Components.interfaces.nsIIOService);
  ioService.offline = false;

  gExpectedStatus = Components.results.NS_ERROR_CONNECTION_REFUSED;
  gNextTestFunc = end_test;
  dump("Testing error returned by aync XHR when the connection is refused\n");
  asyncXHR.load();
}

function end_test() {
  do_test_finished();
}
