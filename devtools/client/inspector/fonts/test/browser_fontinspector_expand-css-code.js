/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the font-face css rule code is collapsed by default, and can be expanded.

const TEST_URI = URL_ROOT + "browser_fontinspector.html";

add_task(async function() {
  await pushPref("devtools.inspector.fonteditor.enabled", true);
  const { view, inspector } = await openFontInspectorForURL(TEST_URI);
  const viewDoc = view.document;
  await selectNode("div", inspector);

  await expandOtherFontsAccordion(viewDoc);
  info("Checking that the css font-face rule is collapsed by default");
  let fontEl = getOtherFontsEls(viewDoc)[0];
  let codeEl = fontEl.querySelector(".font-css-code");
  is(codeEl.textContent, "@font-face {}", "The font-face rule is collapsed");

  info("Expanding the rule by clicking on the expander icon");
  let onExpanded = BrowserTestUtils.waitForCondition(() => {
    return codeEl.textContent === `@font-face {
  font-family: bar;
  src: url("ostrich-black.ttf");
  font-weight: bold;
}`;
  }, "Waiting for the font-face rule 1");

  let expander = fontEl.querySelector(".font-css-code .theme-twisty");
  expander.click();
  await onExpanded;

  ok(true, "Font-face rule is now expanded");

  info("Expanding another rule by clicking on the [...] icon instead");
  fontEl = getOtherFontsEls(viewDoc)[1];
  codeEl = fontEl.querySelector(".font-css-code");

  onExpanded = BrowserTestUtils.waitForCondition(() => {
    return codeEl.textContent === `@font-face {
  font-family: bar;
  src: url("ostrich-black.ttf");
  font-weight: 800;
}`;
  }, "Waiting for the font-face rule 2");

  expander = fontEl.querySelector(".font-css-code .font-css-code-expander");
  expander.click();
  await onExpanded;

  ok(true, "Font-face rule is now expanded too");
});
