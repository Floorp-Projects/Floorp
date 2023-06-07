/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { NetUtil } = ChromeUtils.importESModule(
  "resource://gre/modules/NetUtil.sys.mjs"
);

function testURL(url) {
  Services.io.newChannelFromURI(
    NetUtil.newURI(url),
    null, // aLoadingNode
    Services.scriptSecurityManager.getSystemPrincipal(),
    null, // aTriggeringPrincipal
    Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
    Ci.nsIContentPolicy.TYPE_OTHER
  );
}

add_task(async function test_create_channel_with_chrome_url() {
  try {
    testURL("chrome://path");
    Assert.ok(false);
  } catch (e) {
    // Chrome url fails canonicalization
    Assert.equal(e.result, Cr.NS_ERROR_FAILURE);
  }

  try {
    testURL("chrome://path/path/path");
    Assert.ok(false);
  } catch (e) {
    // Chrome url passes canonicalization
    Assert.equal(e.result, Cr.NS_ERROR_FILE_NOT_FOUND);
  }
});
