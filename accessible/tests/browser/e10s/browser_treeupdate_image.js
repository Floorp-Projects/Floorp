/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

const IMG_ID = "img";
const ALT_TEXT = "some-text";
const ARIA_LABEL = "some-label";

// Verify that granting alt text adds the graphic accessible.
addAccessibleTask(
  `<img id="${IMG_ID}" src="${MOCHITESTS_DIR}/moz.png" alt=""/>`,
  async function (browser, accDoc) {
    // Test initial state; the img has empty alt text so it should not be in the tree.
    const acc = findAccessibleChildByID(accDoc, IMG_ID);
    ok(!acc, "Image has no Accessible");

    // Add the alt text. The graphic should have been inserted into the tree.
    info(`Adding alt text "${ALT_TEXT}" to img id '${IMG_ID}'`);
    const shown = waitForEvent(EVENT_SHOW, IMG_ID);
    await invokeSetAttribute(browser, IMG_ID, "alt", ALT_TEXT);
    await shown;
    let tree = {
      role: ROLE_GRAPHIC,
      name: ALT_TEXT,
      children: [],
    };
    testAccessibleTree(acc, tree);
  },
  { chrome: true, iframe: true, remoteIframe: true }
);

// Verify that the graphic accessible exists even with a missing alt attribute.
addAccessibleTask(
  `<img id="${IMG_ID}" src="${MOCHITESTS_DIR}/moz.png"/>`,
  async function (browser, accDoc) {
    // Test initial state; the img has no alt attribute so the name is empty.
    const acc = findAccessibleChildByID(accDoc, IMG_ID);
    let tree = {
      role: ROLE_GRAPHIC,
      name: null,
      children: [],
    };
    testAccessibleTree(acc, tree);

    // Add the alt text. The graphic should still be present in the tree.
    info(`Adding alt attribute with text "${ALT_TEXT}" to id ${IMG_ID}`);
    const shown = waitForEvent(EVENT_NAME_CHANGE, IMG_ID);
    await invokeSetAttribute(browser, IMG_ID, "alt", ALT_TEXT);
    await shown;
    tree = {
      role: ROLE_GRAPHIC,
      name: ALT_TEXT,
      children: [],
    };
    testAccessibleTree(acc, tree);
  },
  { chrome: true, iframe: true, remoteIframe: true }
);

// Verify that removing alt text removes the graphic accessible.
addAccessibleTask(
  `<img id="${IMG_ID}" src="${MOCHITESTS_DIR}/moz.png" alt="${ALT_TEXT}"/>`,
  async function (browser, accDoc) {
    // Test initial state; the img has alt text so it should be in the tree.
    let acc = findAccessibleChildByID(accDoc, IMG_ID);
    let tree = {
      role: ROLE_GRAPHIC,
      name: ALT_TEXT,
      children: [],
    };
    testAccessibleTree(acc, tree);

    // Set the alt text empty. The graphic should have been removed from the tree.
    info(`Setting empty alt text for img id ${IMG_ID}`);
    const hidden = waitForEvent(EVENT_HIDE, acc);
    await invokeContentTask(browser, [IMG_ID, "alt", ""], (id, attr, value) => {
      let elm = content.document.getElementById(id);
      elm.setAttribute(attr, value);
    });
    await hidden;
    acc = findAccessibleChildByID(accDoc, IMG_ID);
    ok(!acc, "Image has no Accessible");
  },
  { chrome: true, iframe: true, remoteIframe: true }
);

// Verify that the presence of an aria-label creates an accessible, even if
// there is no alt text.
addAccessibleTask(
  `<img id="${IMG_ID}" src="${MOCHITESTS_DIR}/moz.png" aria-label="${ARIA_LABEL}" alt=""/>`,
  async function (browser, accDoc) {
    // Test initial state; the img has empty alt text, but it does have an
    // aria-label, so it should be in the tree.
    const acc = findAccessibleChildByID(accDoc, IMG_ID);
    let tree = {
      role: ROLE_GRAPHIC,
      name: ARIA_LABEL,
      children: [],
    };
    testAccessibleTree(acc, tree);

    // Add the alt text. The graphic should still be in the tree.
    info(`Adding alt text "${ALT_TEXT}" to img id '${IMG_ID}'`);
    await invokeSetAttribute(browser, IMG_ID, "alt", ALT_TEXT);
    tree = {
      role: ROLE_GRAPHIC,
      name: ARIA_LABEL,
      children: [],
    };
    testAccessibleTree(acc, tree);
  },
  { chrome: true, iframe: true, remoteIframe: true }
);

// Verify that the presence of a click listener results in the graphic
// accessible's presence in the tree.
addAccessibleTask(
  `<img id="${IMG_ID}" src="${MOCHITESTS_DIR}/moz.png" alt=""/>`,
  async function (browser, accDoc) {
    // Add a click listener to the img element.
    info(`Adding click listener to img id '${IMG_ID}'`);
    const shown = waitForEvent(EVENT_SHOW, IMG_ID);
    await invokeContentTask(browser, [IMG_ID], id => {
      content.document.getElementById(id).addEventListener("click", () => {});
    });
    await shown;

    // Test initial state; the img has empty alt text, but it does have a click
    // listener, so it should be in the tree.
    let acc = findAccessibleChildByID(accDoc, IMG_ID);
    let tree = {
      role: ROLE_GRAPHIC,
      name: null,
      children: [],
    };
    testAccessibleTree(acc, tree);
  },
  { chrome: true, iframe: true, remoteIframe: true }
);

// Verify that the presentation role prevents creation of the graphic accessible.
addAccessibleTask(
  `<img id="${IMG_ID}" src="${MOCHITESTS_DIR}/moz.png" role="presentation"/>`,
  async function (browser, accDoc) {
    // Test initial state; the img is presentational and should not be in the tree.
    const acc = findAccessibleChildByID(accDoc, IMG_ID);
    ok(!acc, "Image has no Accessible");

    // Add some alt text. There should still be no accessible for the img in the tree.
    info(`Adding alt attribute with text "${ALT_TEXT}" to id ${IMG_ID}`);
    await invokeSetAttribute(browser, IMG_ID, "alt", ALT_TEXT);
    ok(!acc, "Image has no Accessible");

    // Remove the presentation role. The accessible should be created.
    info(`Removing presentation role from img id ${IMG_ID}`);
    const shown = waitForEvent(EVENT_SHOW, IMG_ID);
    await invokeSetAttribute(browser, IMG_ID, "role", "");
    await shown;
    let tree = {
      role: ROLE_GRAPHIC,
      name: ALT_TEXT,
      children: [],
    };
    testAccessibleTree(acc, tree);
  },
  { chrome: true, iframe: true, remoteIframe: true }
);

// Verify that setting empty alt text on a hidden image does not crash.
// See Bug 1799208 for more info.
addAccessibleTask(
  `<img id="${IMG_ID}" src="${MOCHITESTS_DIR}/moz.png" hidden/>`,
  async function (browser, accDoc) {
    // Test initial state; should be no accessible since img is hidden.
    const acc = findAccessibleChildByID(accDoc, IMG_ID);
    ok(!acc, "Image has no Accessible");

    // Add empty alt text. We shouldn't crash.
    info(`Adding empty alt text "" to img id '${IMG_ID}'`);
    await invokeContentTask(browser, [IMG_ID, "alt", ""], (id, attr, value) => {
      let elm = content.document.getElementById(id);
      elm.setAttribute(attr, value);
    });
    ok(true, "Setting empty alt text on a hidden image did not crash");
  },
  { chrome: true, iframe: true, remoteIframe: true }
);
