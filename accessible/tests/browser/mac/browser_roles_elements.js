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
