/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test simple text selection
 */
addAccessibleTask(`<p id="p">Hello World</p>`, async (browser, accDoc) => {
  let macDoc = accDoc.nativeInterface.QueryInterface(
    Ci.nsIAccessibleMacInterface
  );

  let startMarker = macDoc.getAttributeValue("AXStartTextMarker");
  let endMarker = macDoc.getAttributeValue("AXEndTextMarker");
  let range = macDoc.getParameterizedAttributeValue(
    "AXTextMarkerRangeForUnorderedTextMarkers",
    [startMarker, endMarker]
  );
  is(stringForRange(macDoc, range), "Hello World");

  let evt = waitForMacEventWithInfo("AXSelectedTextChanged", (elem, info) => {
    return (
      !info.AXTextStateSync &&
      info.AXTextStateChangeType == AXTextStateChangeTypeSelectionExtend &&
      elem.getAttributeValue("AXRole") == "AXWebArea"
    );
  });
  await SpecialPowers.spawn(browser, [], () => {
    let p = content.document.getElementById("p");
    let r = new content.Range();
    r.setStart(p.firstChild, 1);
    r.setEnd(p.firstChild, 8);

    let s = content.getSelection();
    s.addRange(r);
  });
  await evt;

  range = macDoc.getAttributeValue("AXSelectedTextMarkerRange");
  is(stringForRange(macDoc, range), "ello Wo");

  let firstWordRange = macDoc.getParameterizedAttributeValue(
    "AXRightWordTextMarkerRangeForTextMarker",
    startMarker
  );
  is(stringForRange(macDoc, firstWordRange), "Hello");

  evt = waitForMacEventWithInfo("AXSelectedTextChanged", (elem, info) => {
    return (
      !info.AXTextStateSync &&
      info.AXTextStateChangeType == AXTextStateChangeTypeSelectionExtend &&
      elem.getAttributeValue("AXRole") == "AXWebArea"
    );
  });
  macDoc.setAttributeValue("AXSelectedTextMarkerRange", firstWordRange);
  await evt;
  range = macDoc.getAttributeValue("AXSelectedTextMarkerRange");
  is(stringForRange(macDoc, range), "Hello");

  // Collapse selection
  evt = waitForMacEventWithInfo("AXSelectedTextChanged", (elem, info) => {
    return (
      info.AXTextStateSync &&
      info.AXTextStateChangeType == AXTextStateChangeTypeSelectionMove &&
      elem.getAttributeValue("AXRole") == "AXWebArea"
    );
  });
  await SpecialPowers.spawn(browser, [], () => {
    let s = content.getSelection();
    s.collapseToEnd();
  });
  await evt;
});

/**
 * Test text selection events caused by focus change
 */
addAccessibleTask(
  `<p>
  Hello <a href="#" id="link">World</a>,
  I <a href="#" style="user-select: none;" id="unselectable_link">love</a>
  <button id="button">you</button></p>`,
  async browser => {
    // Set up an AXSelectedTextChanged listener here. It will get resolved
    // on the first non-root event it encounters, so if we test its data at the end
    // of this test it will show us the first text-selectable object that was focused,
    // which is "link".
    let selTextChanged = waitForMacEvent(
      "AXSelectedTextChanged",
      e => e.getAttributeValue("AXDOMIdentifier") != "body"
    );

    let focusChanged = waitForMacEvent("AXFocusedUIElementChanged");
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("unselectable_link").focus();
    });
    let focusChangedTarget = await focusChanged;
    is(
      focusChangedTarget.getAttributeValue("AXDOMIdentifier"),
      "unselectable_link",
      "Correct event target"
    );

    focusChanged = waitForMacEvent("AXFocusedUIElementChanged");
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("button").focus();
    });
    focusChangedTarget = await focusChanged;
    is(
      focusChangedTarget.getAttributeValue("AXDOMIdentifier"),
      "button",
      "Correct event target"
    );

    focusChanged = waitForMacEvent("AXFocusedUIElementChanged");
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("link").focus();
    });
    focusChangedTarget = await focusChanged;
    is(
      focusChangedTarget.getAttributeValue("AXDOMIdentifier"),
      "link",
      "Correct event target"
    );

    let selTextChangedTarget = await selTextChanged;
    is(
      selTextChangedTarget.getAttributeValue("AXDOMIdentifier"),
      "link",
      "Correct event target"
    );
  }
);

/**
 * Test text selection with focus change
 */
addAccessibleTask(
  `<p id="p">Hello <input id="input"></p>`,
  async (browser, accDoc) => {
    let macDoc = accDoc.nativeInterface.QueryInterface(
      Ci.nsIAccessibleMacInterface
    );

    let evt = waitForMacEventWithInfo("AXSelectedTextChanged", (elem, info) => {
      return (
        !info.AXTextStateSync &&
        info.AXTextStateChangeType == AXTextStateChangeTypeSelectionExtend &&
        elem.getAttributeValue("AXRole") == "AXWebArea"
      );
    });
    await SpecialPowers.spawn(browser, [], () => {
      let p = content.document.getElementById("p");
      let r = new content.Range();
      r.setStart(p.firstChild, 1);
      r.setEnd(p.firstChild, 3);

      let s = content.getSelection();
      s.addRange(r);
    });
    await evt;

    let range = macDoc.getAttributeValue("AXSelectedTextMarkerRange");
    is(stringForRange(macDoc, range), "el");

    let events = Promise.all([
      waitForMacEvent("AXFocusedUIElementChanged"),
      waitForMacEventWithInfo("AXSelectedTextChanged"),
    ]);
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("input").focus();
    });
    let [, { data }] = await events;
    ok(
      data.AXTextSelectionChangedFocus,
      "have AXTextSelectionChangedFocus in event info"
    );
    ok(!data.AXTextStateSync, "no AXTextStateSync in editables");
    is(
      data.AXTextSelectionDirection,
      AXTextSelectionDirectionDiscontiguous,
      "discontigous direction"
    );
  }
);
