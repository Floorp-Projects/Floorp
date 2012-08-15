/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://testing-common/httpd.js");

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
  var server = new HttpServer();
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
