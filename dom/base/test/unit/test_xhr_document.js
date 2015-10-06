/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

var server = new HttpServer();
server.start(-1);

var docbody = '<html xmlns="http://www.w3.org/1999/xhtml"><head></head><body></body></html>';

function handler(metadata, response) {
  let body = NetUtil.readInputStreamToString(metadata.bodyInputStream,
                                             metadata.bodyInputStream.available());
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.write(body, body.length);
}

function run_test() {
  do_test_pending();
  server.registerPathHandler("/foo", handler);

  var parser = Cc["@mozilla.org/xmlextras/domparser;1"].createInstance(Ci.nsIDOMParser);
  parser.init();
  let doc = parser.parseFromString(docbody, "text/html");
  let xhr = Cc['@mozilla.org/xmlextras/xmlhttprequest;1'].createInstance(Ci.nsIXMLHttpRequest);
  xhr.onload = function() {
    do_check_eq(xhr.responseText, docbody);
    server.stop(do_test_finished);
  };
  xhr.onerror = function() {
    do_check_false(false);
    server.stop(do_test_finished);
  };
  xhr.open("POST", "http://localhost:" + server.identity.primaryPort + "/foo", true);
  xhr.send(doc);
}
