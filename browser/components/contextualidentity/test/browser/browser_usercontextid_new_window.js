/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the content of new browser windows have the expected
// userContextId when it passed as the window arguments.

const TEST_URI =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "http://mochi.test:8888"
  ) + "empty_file.html";

function openWindowWithUserContextId(userContextId, isPrivate) {
  let flags = "chrome,dialog=no,all";
  if (isPrivate) {
    flags += ",private";
  }

  let args = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);

  let urlSupports = Cc["@mozilla.org/supports-string;1"].createInstance(
    Ci.nsISupportsString
  );
  urlSupports.data = TEST_URI;
  args.appendElement(urlSupports);

  args.appendElement(null);
  args.appendElement(null);
  args.appendElement(null);
  args.appendElement(null);

  let userContextIdSupports = Cc[
    "@mozilla.org/supports-PRUint32;1"
  ].createInstance(Ci.nsISupportsPRUint32);
  userContextIdSupports.data = userContextId;
  args.appendElement(userContextIdSupports);

  args.appendElement(Services.scriptSecurityManager.getSystemPrincipal());
  args.appendElement(Services.scriptSecurityManager.getSystemPrincipal());
  args.appendElement(Services.scriptSecurityManager.getSystemPrincipal());

  let windowPromise = BrowserTestUtils.waitForNewWindow({ url: TEST_URI });
  Services.ww.openWindow(
    null,
    AppConstants.BROWSER_CHROME_URL,
    "_blank",
    flags,
    args
  );
  return windowPromise;
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.userContext.enabled", true]],
  });
});

add_task(async function test_new_window() {
  let win = await openWindowWithUserContextId(1, false);

  await ContentTask.spawn(win.gBrowser.selectedBrowser, TEST_URI, url => {
    Assert.equal(content.document.URL, url, "expected document URL");
    let { originAttributes } = content.document.nodePrincipal;
    Assert.equal(originAttributes.userContextId, 1, "expected userContextId");
    Assert.equal(
      originAttributes.privateBrowsingId,
      0,
      "expected non-private context"
    );
  });

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_new_private_window() {
  let win = await openWindowWithUserContextId(1, true);

  await ContentTask.spawn(win.gBrowser.selectedBrowser, TEST_URI, url => {
    Assert.equal(content.document.URL, url, "expected document URL");
    let { originAttributes } = content.document.nodePrincipal;
    Assert.equal(originAttributes.userContextId, 1, "expected userContextId");
    Assert.equal(
      originAttributes.privateBrowsingId,
      1,
      "expected private context"
    );
  });

  await BrowserTestUtils.closeWindow(win);
});
