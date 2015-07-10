/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://gre/modules/BrowserUtils.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

function makeInputStream(aString) {
  let stream = Cc["@mozilla.org/io/string-input-stream;1"]
                 .createInstance(Ci.nsIStringInputStream);
  stream.data = aString;
  return stream; // XPConnect will QI this to nsIInputStream for us.
}

add_task(function* test_remoteWebNavigation_postdata() {
  let obj = {};
  Cu.import("resource://testing-common/httpd.js", obj);
  Cu.import("resource://services-common/utils.js", obj);

  let server = new obj.HttpServer();
  server.start(-1);

  let loadDeferred = Promise.defer();

  server.registerPathHandler("/test", (request, response) => {
    let body = obj.CommonUtils.readBytesFromInputStream(request.bodyInputStream);
    is(body, "success", "request body is correct");
    is(request.method, "POST", "request was a post");
    response.write("Received from POST: " + body);
    loadDeferred.resolve();
  });

  let i = server.identity;
  let path = i.primaryScheme + "://" + i.primaryHost + ":" + i.primaryPort + "/test";

  let postdata =
    "Content-Length: 7\r\n" +
    "Content-Type: application/x-www-form-urlencoded\r\n" +
    "\r\n" +
    "success";

  openUILinkIn(path, "tab", null, makeInputStream(postdata));

  yield loadDeferred.promise;
  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);

  let serverStoppedDeferred = Promise.defer();
  server.stop(function() { serverStoppedDeferred.resolve(); });
  yield serverStoppedDeferred.promise;
});
