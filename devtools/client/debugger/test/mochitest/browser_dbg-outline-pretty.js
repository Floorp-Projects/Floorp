/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that the length of outline functions for original and pretty printed source matches

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-scripts.html", "simple1.js");

  await selectSource(dbg, "simple1.js");
  await openOutlinePanel(dbg);
  const originalSourceOutlineItems = getItems(dbg);

  clickElement(dbg, "prettyPrintButton");
  await waitForLoadedSource(dbg, "simple1.js:formatted");
  await waitForElementWithSelector(dbg, ".outline-list");
  const prettySourceOutlineItems = getItems(dbg);

  is(
    originalSourceOutlineItems.length,
    prettySourceOutlineItems.length,
    "Length of outline functions for both prettyPrint and originalSource same"
  );
});

function getItems(dbg) {
  return findAllElements(dbg, "outlineItems");
}
