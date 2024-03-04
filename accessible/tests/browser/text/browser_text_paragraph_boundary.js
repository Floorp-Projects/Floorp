/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that we don't crash the parent process when querying the paragraph
// boundary on an Accessible which has remote ProxyAccessible descendants.
addAccessibleTask(
  `test`,
  async function testParagraphBoundaryWithRemoteDescendants() {
    const root = getRootAccessible(document).QueryInterface(
      Ci.nsIAccessibleText
    );
    let start = {};
    let end = {};
    // The offsets will change as the Firefox UI changes. We don't really care
    // what they are, just that we don't crash.
    root.getTextAtOffset(0, nsIAccessibleText.BOUNDARY_PARAGRAPH, start, end);
    ok(true, "Getting paragraph boundary succeeded");
  }
);
