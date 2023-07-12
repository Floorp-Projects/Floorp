/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function makeInputStream(aString) {
  let stream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(
    Ci.nsIStringInputStream
  );
  stream.data = aString;
  return stream; // XPConnect will QI this to nsIInputStream for us.
}

add_task(async function test_remoteWebNavigation_postdata() {
  let { HttpServer } = ChromeUtils.importESModule(
    "resource://testing-common/httpd.sys.mjs"
  );
  let { CommonUtils } = ChromeUtils.importESModule(
    "resource://services-common/utils.sys.mjs"
  );

  let server = new HttpServer();
  server.start(-1);

  await new Promise(resolve => {
    server.registerPathHandler("/test", (request, response) => {
      let body = CommonUtils.readBytesFromInputStream(request.bodyInputStream);
      is(body, "success", "request body is correct");
      is(request.method, "POST", "request was a post");
      response.write("Received from POST: " + body);
      resolve();
    });

    let i = server.identity;
    let path =
      i.primaryScheme + "://" + i.primaryHost + ":" + i.primaryPort + "/test";

    let postdata =
      "Content-Length: 7\r\n" +
      "Content-Type: application/x-www-form-urlencoded\r\n" +
      "\r\n" +
      "success";

    openTrustedLinkIn(path, "tab", {
      allowThirdPartyFixup: null,
      postData: makeInputStream(postdata),
    });
  });
  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  await new Promise(resolve => {
    server.stop(function () {
      resolve();
    });
  });
});
