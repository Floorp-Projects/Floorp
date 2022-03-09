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
richer
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
      "We’re building a richer search experience"
    );
  }
);
