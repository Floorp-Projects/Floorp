/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test rotor with heading
 */
addAccessibleTask(
  `<h1 id="hello">hello</h1><br><h2 id="world">world</h2><br>goodbye`,
  async (browser, accDoc) => {
    const searchPred = {
      AXSearchKey: "AXHeadingSearchKey",
      AXImmediateDescendants: 1,
      AXResultsLimit: -1,
      AXDirection: "AXDirectionNext",
    };

    const webArea = accDoc.nativeInterface.QueryInterface(
      Ci.nsIAccessibleMacInterface
    );
    is(
      webArea.getAttributeValue("AXRole"),
      "AXWebArea",
      "Got web area accessible"
    );

    const headingCount = webArea.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );
    is(2, headingCount, "Found two headings");

    const headings = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );
    const hello = getNativeInterface(accDoc, "hello");
    const world = getNativeInterface(accDoc, "world");
    is(
      hello.getAttributeValue("AXTitle"),
      headings[0].getAttributeValue("AXTitle"),
      "Found correct first heading"
    );
    is(
      world.getAttributeValue("AXTitle"),
      headings[1].getAttributeValue("AXTitle"),
      "Found correct second heading"
    );
  }
);
