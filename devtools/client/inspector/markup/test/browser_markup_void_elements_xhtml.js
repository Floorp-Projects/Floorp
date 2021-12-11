/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test void element display in the markupview.
const TEST_URL = URL_ROOT + "doc_markup_void_elements.xhtml";

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URL);
  const { win } = inspector.markup;

  info("check non-void element closing tag is displayed");
  const { editor } = await getContainerForSelector("h1", inspector);
  ok(
    !editor.elt.classList.contains("void-element"),
    "h1 element does not have void-element class"
  );
  ok(
    !editor.elt.querySelector(".close").style.display !== "none",
    "h1 element tag is not hidden"
  );

  info("check void element closing tag is not hidden in XHTML document");
  const container = await getContainerForSelector("br", inspector);
  ok(
    !container.editor.elt.classList.contains("void-element"),
    "br element does not have void-element class"
  );
  const closeElement = container.editor.elt.querySelector(".close");
  const computedStyle = win.getComputedStyle(closeElement);
  ok(computedStyle.display !== "none", "br closing tag is not hidden");
});
