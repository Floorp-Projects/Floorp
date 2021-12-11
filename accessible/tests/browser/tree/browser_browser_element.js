/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

// Test that the tree is correct for browser elements containing remote
// documents.
addAccessibleTask(`test`, async function(browser, docAcc) {
  // testAccessibleTree also verifies childCount, indexInParent and parent.
  testAccessibleTree(browser, {
    INTERNAL_FRAME: [{ DOCUMENT: [{ TEXT_LEAF: [] }] }],
  });
});
