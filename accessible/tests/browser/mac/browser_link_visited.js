/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
/* import-globals-from ../../mochitest/states.js */
loadScripts(
  { name: "role.js", dir: MOCHITESTS_DIR },
  { name: "states.js", dir: MOCHITESTS_DIR }
);

ChromeUtils.defineModuleGetter(
  this,
  "BrowserTestUtils",
  "resource://testing-common/BrowserTestUtils.jsm"
);

/**
 * Test visited link properties.
 */
addAccessibleTask(
  `
  <a id="link" href="http://www.example.com/">I am a non-visited link</a><br>
  `,
  async (browser, accDoc) => {
    let link = getNativeInterface(accDoc, "link");
    let stateChanged = waitForEvent(EVENT_STATE_CHANGE, "link");

    is(link.getAttributeValue("AXVisited"), 0, "Link has not been visited");

    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      "http://www.example.com/"
    );
    await BrowserTestUtils.removeTab(tab);

    await stateChanged;
    is(link.getAttributeValue("AXVisited"), 1, "Link has been visited");
  }
);
