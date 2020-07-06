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

// Read-only tests
addAccessibleTask(`<p id="p">Hello World</p>`, async (browser, accDoc) => {
  let macDoc = accDoc.nativeInterface.QueryInterface(
    Ci.nsIAccessibleMacInterface
  );

  function stringForRange(r) {
    return macDoc.getParameterizedAttributeValue(
      "AXStringForTextMarkerRange",
      r
    );
  }

  let startMarker = macDoc.getAttributeValue("AXStartTextMarker");
  let endMarker = macDoc.getAttributeValue("AXEndTextMarker");
  let range = macDoc.getParameterizedAttributeValue(
    "AXTextMarkerRangeForUnorderedTextMarkers",
    [startMarker, endMarker]
  );
  is(stringForRange(range), "Hello World");

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
  is(stringForRange(range), "ello Wo");
});
