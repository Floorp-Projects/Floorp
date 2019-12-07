/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* This test ensures that the click-to-play "Activate Plugin" overlay
 * is shown when expected.
 * All testcases are in the plugin_shouldShowOverlay.html file.
 *
 * Note: Technically, the overlay is *always* shown. When this test was
 * originally written, the meaning of "shown" was "shown with the contents",
 * as opposed to "shown as blank". The behavior hasn't changed, but the naming
 * has: now, a "shown as blank" overlay no longer receives a ".hidden" class.
 * It receives a sizing="blank" attribute.
 */

var rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir.replace(
  "chrome://mochitests/content/",
  "http://127.0.0.1:8888/"
);

var gTestBrowser = null;

add_task(async function() {
  registerCleanupFunction(function() {
    clearAllPluginPermissions();
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
    gBrowser.removeCurrentTab();
    gTestBrowser = null;
  });
});

add_task(async function() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  gTestBrowser = gBrowser.selectedBrowser;

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");

  let popupNotification = PopupNotifications.getNotification(
    "click-to-play-plugins",
    gTestBrowser
  );
  ok(
    !popupNotification,
    "Sanity check, should not have a click-to-play notification"
  );

  await promiseTabLoadEvent(
    gBrowser.selectedTab,
    gTestRoot + "plugin_shouldShowOverlay.html"
  );

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let doc = content.document;
    let testcases = doc.querySelectorAll(".testcase");

    for (let testcase of testcases) {
      let plugin = testcase.querySelector("object");
      Assert.ok(plugin, `plugin exists in ${testcase.id}`);

      let overlay = plugin.openOrClosedShadowRoot.getElementById("main");
      Assert.ok(overlay, `overlay exists in ${testcase.id}`);

      let expectedVisibility = testcase.getAttribute("shouldshow") == "true";
      Assert.ok(
        (overlay.getAttribute("sizing") != "blank") == expectedVisibility,
        `The expected visibility is correct in ${testcase.id}`
      );
    }
  });
});
