/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that an icon appears next to web font URLs, and that clicking it copies the URL
// to the clipboard thanks to it.

const TEST_URI = URL_ROOT + "browser_fontinspector.html";

add_task(async function() {
  let { view } = await openFontInspectorForURL(TEST_URI);
  let viewDoc = view.document;

  let fontEl = getUsedFontsEls(viewDoc)[0];
  let linkEl = fontEl.querySelector(".font-origin");
  let iconEl = linkEl.querySelector(".copy-icon");

  ok(iconEl, "The icon is displayed");
  is(iconEl.getAttribute("title"), "Copy URL", "This is the right icon");

  info("Clicking the button and waiting for the clipboard to receive the URL");
  await waitForClipboardPromise(() => iconEl.click(), linkEl.textContent);
});
