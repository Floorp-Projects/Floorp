/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// This test checks if 'executing getter' in a preview correctly displays values.
// Bug 1456441 - 'execute getter' not working in Preview

"use strict";

add_task(async function () {
  // Make sure the toolbox is tall enough to be able to display the whole popup.
  await pushPref("devtools.toolbox.footer.height", 500);

  const dbg = await initDebugger(
    "doc-preview-getter.html",
    "preview-getter.js"
  );

  await loadAndAddBreakpoint(dbg, "preview-getter.js", 5, 5);
  invokeInTab("funcA");
  await waitForPaused(dbg);

  info("Hovers over 'this' token to display the preview.");
  const { tokenEl } = await tryHovering(dbg, 5, 5, "previewPopup");

  info("Wait for properties to be loaded");
  await waitUntil(
    () => dbg.win.document.querySelectorAll(".preview-popup .node").length > 1
  );

  info("Invokes getter and waits for the element to be rendered");
  await clickElement(dbg, "previewPopupInvokeGetterButton");
  await waitForAllElements(dbg, "previewPopupObjectNumber", 2);

  await clickElement(dbg, "previewPopupInvokeGetterButton");
  await waitForAllElements(dbg, "previewPopupObjectObject", 4);

  const getterButtonEls = findAllElements(
    dbg,
    "previewPopupInvokeGetterButton"
  );
  const previewObjectNumberEls = findAllElements(
    dbg,
    "previewPopupObjectNumber"
  );
  const previewObjectObjectEls = findAllElements(
    dbg,
    "previewPopupObjectObject"
  );

  is(getterButtonEls.length, 0, "There are no getter buttons in the preview.");
  is(
    previewObjectNumberEls.length,
    2,
    "Getters were invoked and two numerical values are displayed as expected."
  );
  is(
    previewObjectObjectEls.length,
    4,
    "Getters were invoked and three object values are displayed as expected."
  );

  await closePreviewForToken(dbg, tokenEl);
});
