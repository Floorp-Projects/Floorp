/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test whether line break code in text content will be removed
 * and extra whitespaces will be trimmed.
 */
addAccessibleTask(
  `
  <h1 id="single-line-content">We’re building a richer search experience</h1>
  <h1 id="multi-lines-content">
We’re building a
richest
search experience
  </h1>
  `,
  async (browser, accDoc) => {
    const singleLineContentHeading = getNativeInterface(
      accDoc,
      "single-line-content"
    );
    is(
      singleLineContentHeading.getAttributeValue("AXTitle"),
      "We’re building a richer search experience"
    );

    const multiLinesContentHeading = getNativeInterface(
      accDoc,
      "multi-lines-content"
    );
    is(
      multiLinesContentHeading.getAttributeValue("AXTitle"),
      "We’re building a richest search experience"
    );
  }
);

/**
 * Test AXTitle/AXDescription attributes of heading elements
 */
addAccessibleTask(
  `
  <h1 id="a">Hello <a href="#">world</a></h1>
  <h1 id="b">Hello</h1>
  <h1 id="c" aria-label="Goodbye">Hello</h1>
  `,
  async (browser, accDoc) => {
    const a = getNativeInterface(accDoc, "a");
    is(
      a.getAttributeValue("AXTitle"),
      "Hello world",
      "Correct AXTitle for 'a'"
    );
    ok(
      !a.getAttributeValue("AXDescription"),
      "'a' Should not have AXDescription"
    );

    const b = getNativeInterface(accDoc, "b");
    is(b.getAttributeValue("AXTitle"), "Hello", "Correct AXTitle for 'b'");
    ok(
      !b.getAttributeValue("AXDescription"),
      "'b' Should not have AXDescription"
    );

    const c = getNativeInterface(accDoc, "c");
    is(
      c.getAttributeValue("AXDescription"),
      "Goodbye",
      "Correct AXDescription for 'c'"
    );
    ok(!c.getAttributeValue("AXTitle"), "'c' Should not have AXTitle");
  }
);
