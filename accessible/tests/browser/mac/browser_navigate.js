/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test navigation of same/different type content
 */
addAccessibleTask(
  `<h1 id="hello">hello</h1>
  world<br>
  <a href="example.com" id="link">I am a link</a>
  <h1 id="goodbye">goodbye</h1>`,
  async (browser, accDoc) => {
    const searchPred = {
      AXSearchKey: "AXSameTypeSearchKey",
      AXImmediateDescendantsOnly: 0,
      AXResultsLimit: 1,
      AXDirection: "AXDirectionNext",
    };

    const hello = getNativeInterface(accDoc, "hello");
    const goodbye = getNativeInterface(accDoc, "goodbye");
    const webArea = accDoc.nativeInterface.QueryInterface(
      Ci.nsIAccessibleMacInterface
    );

    searchPred.AXStartElement = hello;

    let sameItem = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );

    is(sameItem.length, 1, "Found one item");
    is(
      "goodbye",
      sameItem[0].getAttributeValue("AXTitle"),
      "Found correct item of same type"
    );

    searchPred.AXDirection = "AXDirectionPrevious";
    searchPred.AXStartElement = goodbye;
    sameItem = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );

    is(sameItem.length, 1, "Found one item");
    is(
      "hello",
      sameItem[0].getAttributeValue("AXTitle"),
      "Found correct item of same type"
    );

    searchPred.AXSearchKey = "AXDifferentTypeSearchKey";
    let diffItem = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );
    is(diffItem.length, 1, "Found one item");
    is(
      "I am a link",
      diffItem[0].getAttributeValue("AXValue"),
      "Found correct item of different type"
    );
  }
);

/**
 * Test navigation of heading levels
 */
addAccessibleTask(
  `
  <h1 id="a">a</h1>
  <h2 id="b">b</h2>
  <h3 id="c">c</h3>
  <h4 id="d">d</h4>
  <h5 id="e">e</h5>
  <h6 id="f">f</h5>
  <h1 id="g">g</h1>
  <h2 id="h">h</h2>
  <h3 id="i">i</h3>
  <h4 id="j">j</h4>
  <h5 id="k">k</h5>
  <h6 id="l">l</h5>
  this is some regular text that should be ignored
  `,
  async (browser, accDoc) => {
    const searchPred = {
      AXSearchKey: "AXHeadingLevel1SearchKey",
      AXImmediateDescendantsOnly: 0,
      AXResultsLimit: -1,
      AXDirection: "AXDirectionNext",
    };

    const webArea = accDoc.nativeInterface.QueryInterface(
      Ci.nsIAccessibleMacInterface
    );

    let h1Count = webArea.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );

    is(2, h1Count, "Found two h1 items");

    let h1s = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );

    const a = getNativeInterface(accDoc, "a");
    const g = getNativeInterface(accDoc, "g");

    is(
      a.getAttributeValue("AXValue"),
      h1s[0].getAttributeValue("AXValue"),
      "Found correct h1 heading"
    );

    is(
      g.getAttributeValue("AXValue"),
      h1s[1].getAttributeValue("AXValue"),
      "Found correct h1 heading"
    );

    searchPred.AXSearchKey = "AXHeadingLevel2SearchKey";

    let h2Count = webArea.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );

    is(2, h2Count, "Found two h2 items");

    let h2s = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );

    const b = getNativeInterface(accDoc, "b");
    const h = getNativeInterface(accDoc, "h");

    is(
      b.getAttributeValue("AXValue"),
      h2s[0].getAttributeValue("AXValue"),
      "Found correct h2 heading"
    );

    is(
      h.getAttributeValue("AXValue"),
      h2s[1].getAttributeValue("AXValue"),
      "Found correct h2 heading"
    );

    searchPred.AXSearchKey = "AXHeadingLevel3SearchKey";

    let h3Count = webArea.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );

    is(2, h3Count, "Found two h3 items");

    let h3s = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );

    const c = getNativeInterface(accDoc, "c");
    const i = getNativeInterface(accDoc, "i");

    is(
      c.getAttributeValue("AXValue"),
      h3s[0].getAttributeValue("AXValue"),
      "Found correct h3 heading"
    );

    is(
      i.getAttributeValue("AXValue"),
      h3s[1].getAttributeValue("AXValue"),
      "Found correct h3 heading"
    );

    searchPred.AXSearchKey = "AXHeadingLevel4SearchKey";

    let h4Count = webArea.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );

    is(2, h4Count, "Found two h4 items");

    let h4s = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );

    const d = getNativeInterface(accDoc, "d");
    const j = getNativeInterface(accDoc, "j");

    is(
      d.getAttributeValue("AXValue"),
      h4s[0].getAttributeValue("AXValue"),
      "Found correct h4 heading"
    );

    is(
      j.getAttributeValue("AXValue"),
      h4s[1].getAttributeValue("AXValue"),
      "Found correct h4 heading"
    );

    searchPred.AXSearchKey = "AXHeadingLevel5SearchKey";

    let h5Count = webArea.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );

    is(2, h5Count, "Found two h5 items");

    let h5s = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );

    const e = getNativeInterface(accDoc, "e");
    const k = getNativeInterface(accDoc, "k");

    is(
      e.getAttributeValue("AXValue"),
      h5s[0].getAttributeValue("AXValue"),
      "Found correct h5 heading"
    );

    is(
      k.getAttributeValue("AXValue"),
      h5s[1].getAttributeValue("AXValue"),
      "Found correct h5 heading"
    );

    searchPred.AXSearchKey = "AXHeadingLevel6SearchKey";

    let h6Count = webArea.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );

    is(2, h6Count, "Found two h6 items");

    let h6s = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );

    const f = getNativeInterface(accDoc, "f");
    const l = getNativeInterface(accDoc, "l");

    is(
      f.getAttributeValue("AXValue"),
      h6s[0].getAttributeValue("AXValue"),
      "Found correct h6 heading"
    );

    is(
      l.getAttributeValue("AXValue"),
      h6s[1].getAttributeValue("AXValue"),
      "Found correct h6 heading"
    );
  }
);

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

/*
 * Test rotor with graphics
 */
addAccessibleTask(
  `
  <img id="img1" alt="image one" src="http://example.com/a11y/accessible/tests/mochitest/moz.png"><br>
  <a href="http://example.com">
    <img id="img2" alt="image two" src="http://example.com/a11y/accessible/tests/mochitest/moz.png">
  </a>
  <img src="" id="img3">
  `,
  (browser, accDoc) => {
    let searchPred = {
      AXSearchKey: "AXGraphicSearchKey",
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

    let images = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );

    is(images.length, 3, "Found three images");

    const img1 = getNativeInterface(accDoc, "img1");
    const img2 = getNativeInterface(accDoc, "img2");
    const img3 = getNativeInterface(accDoc, "img3");

    is(
      img1.getAttributeValue("AXDescription"),
      images[0].getAttributeValue("AXDescription"),
      "Found correct image"
    );

    is(
      img2.getAttributeValue("AXDescription"),
      images[1].getAttributeValue("AXDescription"),
      "Found correct image"
    );

    is(
      img3.getAttributeValue("AXDescription"),
      images[2].getAttributeValue("AXDescription"),
      "Found correct image"
    );
  }
);
