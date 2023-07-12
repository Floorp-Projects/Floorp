/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

var server = new HttpServer();
server.start(-1);

var docbody =
  '<html xmlns="http://www.w3.org/1999/xhtml"><head></head><body></body></html>';

function handler(metadata, response) {
  var { NetUtil } = ChromeUtils.importESModule(
    "resource://gre/modules/NetUtil.sys.mjs"
  );

  let body = NetUtil.readInputStreamToString(
    metadata.bodyInputStream,
    metadata.bodyInputStream.available()
  );
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.write(body, body.length);
}

function run_test() {
  do_test_pending();
  server.registerPathHandler("/foo", handler);

  var parser = new DOMParser();
  let doc = parser.parseFromString(docbody, "text/html");
  let xhr = new XMLHttpRequest();
  xhr.onload = function () {
    Assert.equal(xhr.responseText, docbody);
    server.stop(do_test_finished);
  };
  xhr.onerror = function () {
    Assert.equal(false, false);
    server.stop(do_test_finished);
  };
  xhr.open(
    "POST",
    "http://localhost:" + server.identity.primaryPort + "/foo",
    true
  );
  xhr.send(doc);
}
