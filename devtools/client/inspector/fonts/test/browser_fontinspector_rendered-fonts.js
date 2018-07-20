/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Check that the font inspector has a section for "rendered fonts" and that section
// contains the fonts used to render the content of the selected element.
// Check that it's collapsed by default, but can be expanded.

const TEST_URI = URL_ROOT + "doc_browser_fontinspector.html";

add_task(async function() {
  await pushPref("devtools.inspector.fonteditor.enabled", true);
  const { view } = await openFontInspectorForURL(TEST_URI);
  const viewDoc = view.document;

  const renderedFontsAccordion = getRenderedFontsAccordion(viewDoc);
  ok(renderedFontsAccordion, "There's an accordion for rendered fonts in the panel");
  is(renderedFontsAccordion.textContent, "Rendered fonts", "It has the right title");

  await expandAccordion(renderedFontsAccordion);
  const fontsEls = getRenderedFontsEls(viewDoc);

  ok(fontsEls.length, "There are fonts listed in the rendered fonts section");
});
