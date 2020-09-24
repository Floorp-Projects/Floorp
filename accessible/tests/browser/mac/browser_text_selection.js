/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test rotor with heading
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

  let evt = waitForMacEvent("AXSelectedTextChanged");
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

  evt = waitForMacEvent("AXSelectedTextChanged");
  macDoc.setAttributeValue("AXSelectedTextMarkerRange", firstWordRange);
  await evt;
  range = macDoc.getAttributeValue("AXSelectedTextMarkerRange");
  is(stringForRange(macDoc, range), "Hello");
});