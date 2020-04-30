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
      null,
      "Correct haspopup val for button with false"
    );
    let attrChanged = waitForEvent(EVENT_STATE_CHANGE, "false");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("false")
        .setAttribute("aria-haspopup", "true");
    });
    await attrChanged;

    is(
      falseID.getAttributeValue("AXHasPopup"),
      "true",
      "Correct aria-haspopup after change for false"
    );

    let stateChanged = waitForEvent(EVENT_STATE_CHANGE, "false");
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("false").removeAttribute("aria-haspopup");
    });
    await stateChanged;

    is(
      falseID.getAttributeValue("AXHasPopup"),
      null,
      "Correct aria-haspopup after remove for false"
    );

    // MENU
    let menuID = getNativeInterface(accDoc, "menu");
    is(
      menuID.getAttributeValue("AXHasPopup"),
      "menu",
      "Correct haspopup val for button with menu"
    );

    attrChanged = waitForEvent(EVENT_STATE_CHANGE, "menu");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("menu")
        .setAttribute("aria-haspopup", "true");
    });
    await attrChanged;

    is(
      menuID.getAttributeValue("AXHasPopup"),
      "true",
      "Correct aria-haspopup after change for menu"
    );

    stateChanged = waitForEvent(EVENT_STATE_CHANGE, "menu");
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("menu").removeAttribute("aria-haspopup");
    });
    await stateChanged;

    is(
      menuID.getAttributeValue("AXHasPopup"),
      null,
      "Correct aria-haspopup after remove for menu"
    );

    // LISTBOX
    let listboxID = getNativeInterface(accDoc, "listbox");
    is(
      listboxID.getAttributeValue("AXHasPopup"),
      "listbox",
      "Correct haspopup val for button with listbox"
    );

    attrChanged = waitForEvent(EVENT_STATE_CHANGE, "listbox");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("listbox")
        .setAttribute("aria-haspopup", "true");
    });
    await attrChanged;

    is(
      listboxID.getAttributeValue("AXHasPopup"),
      "true",
      "Correct aria-haspopup after change for listbox"
    );

    stateChanged = waitForEvent(EVENT_STATE_CHANGE, "listbox");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("listbox")
        .removeAttribute("aria-haspopup");
    });
    await stateChanged;

    is(
      listboxID.getAttributeValue("AXHasPopup"),
      null,
      "Correct aria-haspopup after remove for listbox"
    );

    // TREE
    let treeID = getNativeInterface(accDoc, "tree");
    is(
      treeID.getAttributeValue("AXHasPopup"),
      "tree",
      "Correct haspopup val for button with tree"
    );

    attrChanged = waitForEvent(EVENT_STATE_CHANGE, "tree");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("tree")
        .setAttribute("aria-haspopup", "true");
    });
    await attrChanged;

    is(
      treeID.getAttributeValue("AXHasPopup"),
      "true",
      "Correct aria-haspopup after change for tree"
    );

    stateChanged = waitForEvent(EVENT_STATE_CHANGE, "tree");
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("tree").removeAttribute("aria-haspopup");
    });
    await stateChanged;

    is(
      treeID.getAttributeValue("AXHasPopup"),
      null,
      "Correct aria-haspopup after remove for tree"
    );

    // GRID
    let gridID = getNativeInterface(accDoc, "grid");
    is(
      gridID.getAttributeValue("AXHasPopup"),
      "grid",
      "Correct haspopup val for button with grid"
    );

    attrChanged = waitForEvent(EVENT_STATE_CHANGE, "grid");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("grid")
        .setAttribute("aria-haspopup", "true");
    });
    await attrChanged;

    is(
      gridID.getAttributeValue("AXHasPopup"),
      "true",
      "Correct aria-haspopup after change for grid"
    );

    stateChanged = waitForEvent(EVENT_STATE_CHANGE, "grid");
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("grid").removeAttribute("aria-haspopup");
    });
    await stateChanged;

    is(
      gridID.getAttributeValue("AXHasPopup"),
      null,
      "Correct aria-haspopup after remove for grid"
    );

    // DIALOG
    let dialogID = getNativeInterface(accDoc, "dialog");
    is(
      dialogID.getAttributeValue("AXHasPopup"),
      "dialog",
      "Correct haspopup val for button with dialog"
    );

    attrChanged = waitForEvent(EVENT_STATE_CHANGE, "dialog");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("dialog")
        .setAttribute("aria-haspopup", "true");
    });
    await attrChanged;

    is(
      dialogID.getAttributeValue("AXHasPopup"),
      "true",
      "Correct aria-haspopup after change for dialog"
    );

    stateChanged = waitForEvent(EVENT_STATE_CHANGE, "dialog");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("dialog")
        .removeAttribute("aria-haspopup");
    });
    await stateChanged;

    is(
      dialogID.getAttributeValue("AXHasPopup"),
      null,
      "Correct aria-haspopup after remove for dialog"
    );
  }
);
