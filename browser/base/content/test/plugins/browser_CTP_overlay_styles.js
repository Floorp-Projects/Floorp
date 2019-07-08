/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* This test ensures that the click-to-play "Activate Plugin" overlay
 * is shown in the right style (which is dependent on its size).
 */

const rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir.replace(
  "chrome://mochitests/content/",
  "http://127.0.0.1:8888/"
);

var gTestBrowser = null;

const gTestcases = {
  // 10x10
  testcase1: {
    sizing: "blank",
    notext: null,
  },

  // 40x40
  testcase2: {
    sizing: "tiny",
    notext: "notext",
  },

  // 100x70
  testcase3: {
    sizing: "reduced",
    notext: "notext",
  },

  // 200x200
  testcase4: {
    sizing: null,
    notext: "notext",
  },

  // 300x300
  testcase5: {
    sizing: null,
    notext: null,
  },
};

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
    gTestRoot + "plugin_overlay_styles.html"
  );

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  await ContentTask.spawn(gTestBrowser, gTestcases, async function(testcases) {
    let doc = content.document;

    for (let testcaseId of Object.keys(testcases)) {
      let plugin = doc.querySelector(`#${testcaseId} > object`);
      let overlay = plugin.openOrClosedShadowRoot.getElementById("main");
      Assert.ok(overlay, `overlay exists in ${testcaseId}`);

      let expectations = testcases[testcaseId];

      Assert.ok(
        overlay.classList.contains("visible"),
        `The expected visibility is correct in ${testcaseId}`
      );

      Assert.ok(
        overlay.getAttribute("sizing") == expectations.sizing,
        `The expected sizing is correct in ${testcaseId}`
      );

      Assert.ok(
        overlay.getAttribute("notext") == expectations.notext,
        `The expected notext is correct in ${testcaseId}`
      );
    }
  });
});
