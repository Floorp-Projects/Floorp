/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the font-face css rule code is collapsed by default, and can be expanded.

const TEST_URI = URL_ROOT + "doc_browser_fontinspector.html";

add_task(async function () {
  const { view, inspector } = await openFontInspectorForURL(TEST_URI);
  const viewDoc = view.document;
  await selectNode("div", inspector);

  await expandFontsAccordion(viewDoc);
  info("Checking that the css font-face rule is collapsed by default");
  const fontEl = getAllFontsEls(viewDoc)[0];
  const codeEl = fontEl.querySelector(".font-css-code");
  is(codeEl.textContent, "@font-face {}", "The font-face rule is collapsed");

  info("Expanding the rule by clicking on the expander icon");
  const onExpanded = BrowserTestUtils.waitForCondition(() => {
    return (
      codeEl.textContent ===
      `@font-face { font-family: bar; src: url("bad/font/name.ttf"), url("ostrich-regular.ttf") format("truetype"); }`
    );
  }, "Waiting for the font-face rule 1");

  const expander = fontEl.querySelector(".font-css-code .theme-twisty");
  expander.click();
  await onExpanded;

  ok(true, "Font-face rule is now expanded");
});
