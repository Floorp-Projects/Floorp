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

/**
 * Test different HTML elements for their roles and subroles
 */
addAccessibleTask(`<hr id="hr" />`, (browser, accDoc) => {
  let hr = getNativeInterface(accDoc, "hr");
  is(
    hr.getAttributeValue("AXRole"),
    "AXSplitter",
    "AXRole for hr is AXSplitter"
  );
  is(
    hr.getAttributeValue("AXSubrole"),
    "AXContentSeparator",
    "Subrole for hr is AXContentSeparator"
  );
});

addAccessibleTask(
  `
  <figure id="figure">
    <img id="img" src="http://example.com/a11y/accessible/tests/mochitest/moz.png" alt="Logo">
    <p>Non-image figure content</p>
    <figcaption id="figcaption">Old Mozilla logo</figcaption>
  </figure>`,
  (browser, accDoc) => {
    let figure = getNativeInterface(accDoc, "figure");
    ok(!figure.getAttributeValue("AXTitle"), "Figure should not have a title");
    is(
      figure.getAttributeValue("AXDescription"),
      "Old Mozilla logo",
      "Correct figure label"
    );
    is(figure.getAttributeValue("AXRole"), "AXGroup", "Correct figure role");
    is(
      figure.getAttributeValue("AXRoleDescription"),
      "figure",
      "Correct figure role description"
    );

    let img = getNativeInterface(accDoc, "img");
    ok(!img.getAttributeValue("AXTitle"), "img should not have a title");
    is(img.getAttributeValue("AXDescription"), "Logo", "Correct img label");
    is(img.getAttributeValue("AXRole"), "AXImage", "Correct img role");
    is(
      img.getAttributeValue("AXRoleDescription"),
      "image",
      "Correct img role description"
    );

    let figcaption = getNativeInterface(accDoc, "figcaption");
    ok(
      !figcaption.getAttributeValue("AXTitle"),
      "figcaption should not have a title"
    );
    ok(
      !figcaption.getAttributeValue("AXDescription"),
      "figcaption should not have a label"
    );
    is(
      figcaption.getAttributeValue("AXRole"),
      "AXGroup",
      "Correct figcaption role"
    );
    is(
      figcaption.getAttributeValue("AXRoleDescription"),
      "group",
      "Correct figcaption role description"
    );
  }
);
