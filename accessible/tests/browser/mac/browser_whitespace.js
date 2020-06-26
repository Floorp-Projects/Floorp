/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

/**
 * Test accessibles aren't created for linebreaks.
 */
addAccessibleTask(`hello<br>world`, async (browser, accDoc) => {
  let doc = accDoc.nativeInterface.QueryInterface(Ci.nsIAccessibleMacInterface);
  let docChildren = doc.getAttributeValue("AXChildren");
  is(docChildren.length, 2, "The document contains two children");

  // verify first child is correct
  is(
    docChildren[0].getAttributeValue("AXRole"),
    "AXStaticText",
    "First child is a text node"
  );
  is(
    docChildren[0].getAttributeValue("AXValue"),
    "hello",
    "First child is hello text"
  );

  // verify second child is correct
  is(
    docChildren[1].getAttributeValue("AXRole"),
    "AXStaticText",
    "Second child is a text node"
  );

  is(
    docChildren[1].getAttributeValue("AXValue"),
    "world ",
    "Second child is world text"
  );
  // we have a trailing space here due to bug 1577028
});
