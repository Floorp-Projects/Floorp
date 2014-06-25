// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test() {
  if (!isLandscapeMode()) {
    todo(false, "browser_snapped_tests need landscape mode to run.");
    return;
  }

  runTests();
}
let tabAdded = false;

function setUp() {
  if (!tabAdded) {
    yield addTab(chromeRoot + "res/textdivs01.html");
    tabAdded = true;
  }
  yield hideContextUI();
}

XPCOMUtils.defineLazyServiceGetter(this, "gDOMUtils",
  "@mozilla.org/inspector/dom-utils;1", "inIDOMUtils");

const kActiveState = 0x00000001;
const kHoverState = 0x00000004;

gTests.push({
  desc: "hover states of menus",
  setUp: setUp,
  run: function() {
    // Clicking on menu items should not leave the clicked menu item
    // in the :active or :hover state.

    let typesArray = [
      "copy",
      "paste"
    ];

    let promise = waitForEvent(document, "popupshown");
    ContextMenuUI.showContextMenu({
      target: null,
      json: {
        types: typesArray,
        string: '',
        xPos: 1,
        yPos: 1,
        leftAligned: true,
        bottomAligned: true
    }});
    yield promise;

    // should be visible
    ok(ContextMenuUI._menuPopup.visible, "is visible");

    let menuItem = document.getElementById("context-copy");
    promise = waitForEvent(document, "popuphidden");
    sendNativeTap(menuItem);
    yield promise;

    for (let idx = 0; idx < ContextMenuUI.commands.childNodes.length; idx++) {
      let item = ContextMenuUI.commands.childNodes[idx];
      let state = gDOMUtils.getContentState(item);
      if ((state & kHoverState) || (state & kActiveState)) {
        ok(false, "found invalid state on context menu item (" + state.toString(2) + ")");
      }
    }

    // Do it again, but this time check the visible menu too and
    // click a different menu item.
    promise = waitForEvent(document, "popupshown");
    ContextMenuUI.showContextMenu({
      target: null,
      json: {
        types: typesArray,
        string: '',
        xPos: 1,
        yPos: 1,
        leftAligned: true,
        bottomAligned: true
    }});
    yield promise;

    // should be visible
    ok(ContextMenuUI._menuPopup.visible, "is visible");

    for (let idx = 0; idx < ContextMenuUI.commands.childNodes.length; idx++) {
      let item = ContextMenuUI.commands.childNodes[idx];
      let state = gDOMUtils.getContentState(item);
      if ((state & kHoverState) || (state & kActiveState)) {
        ok(false, "found invalid state on context menu item (" + state.toString(2) + ")");
      }
    }

    menuItem = document.getElementById("context-paste");
    promise = waitForEvent(document, "popuphidden");
    sendNativeTap(menuItem);
    yield promise;

    for (let idx = 0; idx < ContextMenuUI.commands.childNodes.length; idx++) {
      let item = ContextMenuUI.commands.childNodes[idx];
      let state = gDOMUtils.getContentState(item);
      if ((state & kHoverState) || (state & kActiveState)) {
        ok(false, "found invalid state on context menu item (" + state.toString(2) + ")");
      }
    }
  },
  tearDown: function () {
    clearNativeTouchSequence();
  }
});

gTests.push({
  desc: "hover states of nav bar buttons",
  setUp: setUp,
  run: function() {
    // show nav bar
    yield showNavBar();

    // tap bookmark button
    sendNativeTap(Appbar.starButton);
    yield waitForMs(100);
    
    // check hover state
    let state = gDOMUtils.getContentState(Appbar.starButton);
    if ((state & kHoverState) || (state & kActiveState)) {
      ok(false, "found invalid state on star button (" + state.toString(2) + ")");
    }

    // tap bookmark button
    sendNativeTap(Appbar.starButton);
    yield waitForMs(100);
    
    // check hover state
    let state = gDOMUtils.getContentState(Appbar.starButton);
    if ((state & kHoverState) || (state & kActiveState)) {
      ok(false, "found invalid state on star button (" + state.toString(2) + ")");
    }
  },
  tearDown: function () {
    clearNativeTouchSequence();
  }
});

