/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

const snippet = `
  <style>
    #target {
      width: 150px;
      height: 150px;
      background-color: lightblue;
    }
    #child {
      width: 100px;
      height: 100px;
      background-color: lightgreen;
    }
    #content-child {
      width: 100px;
      height: 100px;
      background-color: green;
      display: contents;
    }
    .hidden {
      content-visibility: hidden;
    }
    .auto {
      content-visibility: auto;
    }
  </style>
  <div class="hidden" id="target">
    <div id="child"></div>
    <div id="content-child"></div>
  </div>
  `;

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["layout.css.content-visibility.enabled", true]],
  });
});

async function setContentVisibility(browser, id, value) {
  let onReorder = waitForEvent(EVENT_REORDER, id);

  // Change the value of `content-visibility` property for the target
  info(`Setting content-visibility: ${value} on ${id}`);
  await invokeSetAttribute(browser, id, "class", value);
  await onReorder;
}

addAccessibleTask(
  snippet,
  async function (browser, accDoc) {
    const targetId = "target";
    const target = findAccessibleChildByID(accDoc, targetId);

    info("Initial Accessibility Structure Test");
    testAccessibleTree(target, { SECTION: [] });

    await setContentVisibility(browser, targetId, "auto");
    testAccessibleTree(target, { SECTION: [{ SECTION: [] }, { SECTION: [] }] });

    await setContentVisibility(browser, targetId, "hidden");
    testAccessibleTree(target, { SECTION: [] });
  },
  { iframe: true, remoteIframe: true, chrome: true }
);
