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
 * The Original Code is XMLHttpRequest testing code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Josh Matthews <josh@joshmatthews.net> (original author)
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

do_load_httpd_js();

var server = null;

const SERVER_PORT = 4444;
const HTTP_BASE = "http://localhost:" + SERVER_PORT;
const redirectPath = "/redirect";
const headerCheckPath = "/headerCheck";
const redirectURL = HTTP_BASE + redirectPath;
const headerCheckURL = HTTP_BASE + headerCheckPath;

function redirectHandler(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 302, "Found");
  response.setHeader("Location", headerCheckURL, false);
}

function headerCheckHandler(metadata, response) {
  try {
    let headerValue = metadata.getHeader("X-Custom-Header");
    do_check_eq(headerValue, "present");
  } catch(e) {
    do_throw("No header present after redirect");
  }
  try {
    metadata.getHeader("X-Unwanted-Header");
    do_throw("Unwanted header present after redirect");
  } catch (x) {
  }
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/plain");
  response.write("");
}

function run_test() {
  var server = new nsHttpServer();
  server.registerPathHandler(redirectPath, redirectHandler);
  server.registerPathHandler(headerCheckPath, headerCheckHandler);
  server.start(SERVER_PORT);

  do_test_pending();
  var request = Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"]
                .createInstance(Components.interfaces.nsIXMLHttpRequest);
  request.open("GET", redirectURL, true);
  request.setRequestHeader("X-Custom-Header", "present");
  request.addEventListener("readystatechange", function() {
    if (request.readyState == 4) {
      do_check_eq(request.status, 200);
      server.stop(do_test_finished);
    }
  }, false);
  request.send();
  try {
    request.setRequestHeader("X-Unwanted-Header", "present");
    do_throw("Shouldn't be able to set a header after send");
  } catch (x) {
  }    
}
