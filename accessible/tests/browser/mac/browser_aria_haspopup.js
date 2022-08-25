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

/**
 * Test aria-haspopup
 */
addAccessibleTask(
  `
  <button aria-haspopup="false" id="false">action</button>

  <button aria-haspopup="menu" id="menu">action</button>

  <button aria-haspopup="listbox" id="listbox">action</button>

  <button aria-haspopup="tree" id="tree">action</button>

  <button aria-haspopup="grid" id="grid">action</button>

  <button aria-haspopup="dialog" id="dialog">action</button>

  `,
  async (browser, accDoc) => {
    // FALSE
    let falseID = getNativeInterface(accDoc, "false");
    is(
      falseID.getAttributeValue("AXHasPopup"),
      0,
      "Correct AXHasPopup val for button with false"
    );
    is(
      falseID.getAttributeValue("AXPopupValue"),
      null,
      "Correct AXPopupValue val for button with false"
    );
    let attrChanged = waitForEvent(EVENT_STATE_CHANGE, "false");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("false")
        .setAttribute("aria-haspopup", "true");
    });
    await attrChanged;

    is(
      falseID.getAttributeValue("AXPopupValue"),
      "true",
      "Correct AXPopupValue after change for false"
    );
    is(
      falseID.getAttributeValue("AXHasPopup"),
      1,
      "Correct AXHasPopup val for button with true"
    );

    let stateChanged = waitForEvent(EVENT_STATE_CHANGE, "false");
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("false").removeAttribute("aria-haspopup");
    });
    await stateChanged;

    is(
      falseID.getAttributeValue("AXPopupValue"),
      null,
      "Correct AXPopupValue after remove for false"
    );
    is(
      falseID.getAttributeValue("AXHasPopup"),
      0,
      "Correct AXHasPopup val for button after remove"
    );

    // MENU
    let menuID = getNativeInterface(accDoc, "menu");
    is(
      menuID.getAttributeValue("AXPopupValue"),
      "menu",
      "Correct AXPopupValue val for button with menu"
    );
    is(
      menuID.getAttributeValue("AXHasPopup"),
      1,
      "Correct AXHasPopup val for button with menu"
    );

    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("menu")
        .setAttribute("aria-haspopup", "true");
    });

    await untilCacheIs(
      () => menuID.getAttributeValue("AXPopupValue"),
      "true",
      "Correct AXPopupValue after change for menu"
    );
    is(
      menuID.getAttributeValue("AXHasPopup"),
      1,
      "Correct AXHasPopup val for button with menu"
    );

    stateChanged = waitForEvent(EVENT_STATE_CHANGE, "menu");
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("menu").removeAttribute("aria-haspopup");
    });
    await stateChanged;

    await untilCacheIs(
      () => menuID.getAttributeValue("AXPopupValue"),
      null,
      "Correct AXPopupValue after remove for menu"
    );
    is(
      menuID.getAttributeValue("AXHasPopup"),
      0,
      "Correct AXHasPopup val for button after remove"
    );

    // LISTBOX
    let listboxID = getNativeInterface(accDoc, "listbox");
    is(
      listboxID.getAttributeValue("AXPopupValue"),
      "listbox",
      "Correct AXPopupValue for button with listbox"
    );
    is(
      listboxID.getAttributeValue("AXHasPopup"),
      1,
      "Correct AXHasPopup for button with listbox"
    );

    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("listbox")
        .setAttribute("aria-haspopup", "true");
    });

    await untilCacheIs(
      () => listboxID.getAttributeValue("AXPopupValue"),
      "true",
      "Correct AXPopupValue after change for listbox"
    );
    is(
      listboxID.getAttributeValue("AXHasPopup"),
      1,
      "Correct AXHasPopup for button with listbox"
    );

    stateChanged = waitForEvent(EVENT_STATE_CHANGE, "listbox");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("listbox")
        .removeAttribute("aria-haspopup");
    });
    await stateChanged;

    is(
      listboxID.getAttributeValue("AXPopupValue"),
      null,
      "Correct AXPopupValue after remove for listbox"
    );
    is(
      listboxID.getAttributeValue("AXHasPopup"),
      0,
      "Correct AXHasPopup for button with listbox"
    );

    // TREE
    let treeID = getNativeInterface(accDoc, "tree");
    is(
      treeID.getAttributeValue("AXPopupValue"),
      "tree",
      "Correct AXPopupValue for button with tree"
    );
    is(
      treeID.getAttributeValue("AXHasPopup"),
      1,
      "Correct AXHasPopup for button with tree"
    );

    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("tree")
        .setAttribute("aria-haspopup", "true");
    });

    await untilCacheIs(
      () => treeID.getAttributeValue("AXPopupValue"),
      "true",
      "Correct AXPopupValue after change for tree"
    );
    is(
      treeID.getAttributeValue("AXHasPopup"),
      1,
      "Correct AXHasPopup for button with tree"
    );

    stateChanged = waitForEvent(EVENT_STATE_CHANGE, "tree");
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("tree").removeAttribute("aria-haspopup");
    });
    await stateChanged;

    is(
      treeID.getAttributeValue("AXPopupValue"),
      null,
      "Correct AXPopupValue after remove for tree"
    );
    is(
      treeID.getAttributeValue("AXHasPopup"),
      0,
      "Correct AXHasPopup for button with tree after remove"
    );

    // GRID
    let gridID = getNativeInterface(accDoc, "grid");
    is(
      gridID.getAttributeValue("AXPopupValue"),
      "grid",
      "Correct AXPopupValue for button with grid"
    );
    is(
      gridID.getAttributeValue("AXHasPopup"),
      1,
      "Correct AXHasPopup for button with grid"
    );

    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("grid")
        .setAttribute("aria-haspopup", "true");
    });

    await untilCacheIs(
      () => gridID.getAttributeValue("AXPopupValue"),
      "true",
      "Correct AXPopupValue after change for grid"
    );
    is(
      gridID.getAttributeValue("AXHasPopup"),
      1,
      "Correct AXHasPopup for button with grid"
    );

    stateChanged = waitForEvent(EVENT_STATE_CHANGE, "grid");
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("grid").removeAttribute("aria-haspopup");
    });
    await stateChanged;

    is(
      gridID.getAttributeValue("AXPopupValue"),
      null,
      "Correct AXPopupValue after remove for grid"
    );
    is(
      gridID.getAttributeValue("AXHasPopup"),
      0,
      "Correct AXHasPopup for button with grid after remove"
    );

    // DIALOG
    let dialogID = getNativeInterface(accDoc, "dialog");
    is(
      dialogID.getAttributeValue("AXPopupValue"),
      "dialog",
      "Correct AXPopupValue for button with dialog"
    );
    is(
      dialogID.getAttributeValue("AXHasPopup"),
      1,
      "Correct AXHasPopup for button with dialog"
    );

    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("dialog")
        .setAttribute("aria-haspopup", "true");
    });

    await untilCacheIs(
      () => dialogID.getAttributeValue("AXPopupValue"),
      "true",
      "Correct AXPopupValue after change for dialog"
    );
    is(
      dialogID.getAttributeValue("AXHasPopup"),
      1,
      "Correct AXHasPopup for button with dialog"
    );

    stateChanged = waitForEvent(EVENT_STATE_CHANGE, "dialog");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("dialog")
        .removeAttribute("aria-haspopup");
    });
    await stateChanged;

    is(
      dialogID.getAttributeValue("AXPopupValue"),
      null,
      "Correct AXPopupValue after remove for dialog"
    );
    is(
      dialogID.getAttributeValue("AXHasPopup"),
      0,
      "Correct AXHasPopup for button with dialog after remove"
    );
  }
);
