/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that an icon appears next to web font URLs, and that clicking it copies the URL
// to the clipboard thanks to it.

const TEST_URI = URL_ROOT + "doc_browser_fontinspector.html";

add_task(async function () {
  const { view, inspector } = await openFontInspectorForURL(TEST_URI);
  const viewDoc = view.document;
  await selectNode("div", inspector);
  await expandFontsAccordion(viewDoc);
  const allFontsEls = getAllFontsEls(viewDoc);
  const fontEl = allFontsEls[0];

  const linkEl = fontEl.querySelector(".font-origin");
  const iconEl = linkEl.querySelector(".copy-icon");

  ok(iconEl, "The icon is displayed");
  is(iconEl.getAttribute("title"), "Copy URL", "This is the right icon");

  info("Clicking the button and waiting for the clipboard to receive the URL");
  await waitForClipboardPromise(() => iconEl.click(), linkEl.textContent);
});
