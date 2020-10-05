/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Test rotor with blockquotes
 */
addAccessibleTask(
  `
  <blockquote id="first">hello I am a blockquote</blockquote>
  <blockquote id="second">
    I am also a blockquote of the same level
    <br>
    <blockquote id="third">but I have a different level</blockquote>
  </blockquote>
  `,
  (browser, accDoc) => {
    let searchPred = {
      AXSearchKey: "AXBlockquoteSearchKey",
      AXImmediateDescendantsOnly: 0,
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

    let bquotes = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );

    is(bquotes.length, 3, "Found three blockquotes");

    const first = getNativeInterface(accDoc, "first");
    const second = getNativeInterface(accDoc, "second");
    const third = getNativeInterface(accDoc, "third");
    console.log("values :");
    console.log(first.getAttributeValue("AXValue"));
    is(
      first.getAttributeValue("AXValue"),
      bquotes[0].getAttributeValue("AXValue"),
      "Found correct first blockquote"
    );

    is(
      second.getAttributeValue("AXValue"),
      bquotes[1].getAttributeValue("AXValue"),
      "Found correct second blockquote"
    );

    is(
      third.getAttributeValue("AXValue"),
      bquotes[2].getAttributeValue("AXValue"),
      "Found correct third blockquote"
    );
  }
);
