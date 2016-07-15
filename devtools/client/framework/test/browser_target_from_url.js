/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "data:text/html;charset=utf-8," +
  "<p>browser_target-from-url.js</p>";

const { targetFromURL } = require("devtools/client/framework/target-from-url");

function assertIsTabTarget(target, chrome = false) {
  is(target.url, TEST_URI);
  is(target.isLocalTab, false);
  is(target.chrome, chrome);
  is(target.isTabActor, true);
  is(target.isRemote, true);
}

add_task(function* () {
  let tab = yield addTab(TEST_URI);
  let browser = tab.linkedBrowser;
  let target;

  info("Test invalid type");
  try {
    yield targetFromURL(new URL("http://foo?type=x"));
    ok(false, "Shouldn't pass");
  } catch (e) {
    is(e.message, "targetFromURL, unsupported type='x' parameter");
  }

  info("Test tab");
  let windowId = browser.outerWindowID;
  target = yield targetFromURL(new URL("http://foo?type=tab&id=" + windowId));
  assertIsTabTarget(target);

  info("Test tab with chrome privileges");
  target = yield targetFromURL(new URL("http://foo?type=tab&id=" + windowId + "&chrome"));
  assertIsTabTarget(target, true);

  info("Test invalid tab id");
  try {
    yield targetFromURL(new URL("http://foo?type=tab&id=10000"));
    ok(false, "Shouldn't pass");
  } catch (e) {
    is(e.message, "targetFromURL, tab with outerWindowID:'10000' doesn't exist");
  }

  info("Test parent process");
  target = yield targetFromURL(new URL("http://foo?type=process"));
  let topWindow = Services.wm.getMostRecentWindow("navigator:browser");
  is(target.url, topWindow.location.href);
  is(target.isLocalTab, false);
  is(target.chrome, true);
  is(target.isTabActor, true);
  is(target.isRemote, true);

  yield target.client.close();
  gBrowser.removeCurrentTab();
});
