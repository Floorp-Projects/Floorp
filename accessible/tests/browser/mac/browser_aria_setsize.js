/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test aria-setsize and aria-posinset
 */
addAccessibleTask(
  `<div role="listbox" id="select" aria-label="Choose a number">
     <div role="option" id="two" aria-setsize="5" aria-posinset="2">Two</div>
     <div role="option" id="three" aria-setsize="5" aria-posinset="3">Three</div>
     <div role="option" id="four" aria-setsize="5" aria-posinset="4">Four</div>
   </div>
   <p id="p">Hello</p>`,
  async (browser, accDoc) => {
    function testPosInSet(id, setsize, posinset) {
      let elem = getNativeInterface(accDoc, id);
      is(
        elem.getAttributeValue("AXARIASetSize"),
        setsize,
        `${id}: Correct set size`
      );

      is(
        elem.getAttributeValue("AXARIAPosInSet"),
        posinset,
        `${id}: Correct position in set`
      );
    }

    testPosInSet("two", 5, 2);
    testPosInSet("three", 5, 3);
    testPosInSet("four", 5, 4);

    let p = getNativeInterface(accDoc, "p");
    ok(
      !p.attributeNames.includes("AXARIASetSize"),
      "Paragraph should not have AXARIASetSize"
    );
    ok(
      !p.attributeNames.includes("AXARIAPosInSet"),
      "Paragraph should not have AXARIAPosInSet"
    );
  }
);
