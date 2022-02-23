/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// This test checks if 'executing getter' in a preview correctly displays values.
// Bug 1456441 - 'execute getter' not working in Preview

add_task(async function() {
  const dbg = await initDebugger("doc-preview-getter.html", "preview-getter.js");
  const source = findSource(dbg, "preview-getter.js");

  await loadAndAddBreakpoint(dbg, source.url, 5, 4);
  invokeInTab("funcA");
  await waitForPaused(dbg);

  info("Hovers over 'this' token to display the preview.");
  await tryHovering(dbg, 5, 8, "previewPopup");

  info("Invokes getter and waits for the element to be rendered");
  await clickElement(dbg, "previewPopupInvokeGetterButton");
  await waitForAllElements(dbg, "previewPopupObjectNumber", 2);

  await clickElement(dbg, "previewPopupInvokeGetterButton");
  await waitForAllElements(dbg, "previewPopupObjectObject", 3);

  const getterButtonEls = findAllElements(dbg, "previewPopupInvokeGetterButton");
  const previewObjectNumberEls = findAllElements(dbg, "previewPopupObjectNumber");
  const previewObjectObjectEls = findAllElements(dbg, "previewPopupObjectObject");

  is(getterButtonEls.length, 0, "There are no getter buttons in the preview.");
  is(previewObjectNumberEls.length, 2, "Getters were invoked and two numerical values are displayed as expected.");
  is(previewObjectObjectEls.length, 3, "Getters were invoked and three object values are displayed as expected.");

  await closePreviewAtPos(dbg, 5, 8);
});

