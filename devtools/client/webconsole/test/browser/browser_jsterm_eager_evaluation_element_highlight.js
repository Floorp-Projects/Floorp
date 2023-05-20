/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URI = `data:text/html;charset=utf8,<!DOCTYPE html>
<h1 class="title">hello</h1>
<div id="mydiv">mydivtext</div>
<script>
  x = Object.create(null, Object.getOwnPropertyDescriptors({
    a: document.querySelector("h1"),
    b: document.querySelector("div"),
    c: document.createElement("hr")
  }));
</script>`;

// Test that when the eager evaluation result is an element, it gets highlighted.
add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm, toolbox } = hud;
  const { autocompletePopup } = jsterm;

  const highlighterTestFront = await getHighlighterTestFront(toolbox);
  const highlighter = toolbox.getHighlighter();
  let onHighlighterShown;
  let onHighlighterHidden;
  let data;

  ok(!autocompletePopup.isOpen, "popup is not open");
  const onPopupOpen = autocompletePopup.once("popup-opened");
  onHighlighterShown = highlighter.waitForHighlighterShown();
  EventUtils.sendString("x.");
  await onPopupOpen;

  await waitForEagerEvaluationResult(hud, `<h1 class="title">`);
  data = await onHighlighterShown;
  is(data.nodeFront.displayName, "h1", "The correct node was highlighted");
  isVisible = await highlighterTestFront.isHighlighting();
  is(isVisible, true, "Highlighter is displayed");

  onHighlighterShown = highlighter.waitForHighlighterShown();
  EventUtils.synthesizeKey("KEY_ArrowDown");
  await waitForEagerEvaluationResult(hud, `<div id="mydiv">`);
  data = await onHighlighterShown;
  is(data.nodeFront.displayName, "div", "The correct node was highlighted");
  isVisible = await highlighterTestFront.isHighlighting();
  is(isVisible, true, "Highlighter is displayed");

  onHighlighterHidden = highlighter.waitForHighlighterHidden();
  EventUtils.synthesizeKey("KEY_ArrowDown");
  await waitForEagerEvaluationResult(hud, `<hr>`);
  await onHighlighterHidden;
  ok(true, "The highlighter isn't displayed on a non-connected element");

  info("Test that text nodes are highlighted");
  onHighlighterShown = highlighter.waitForHighlighterShown();
  EventUtils.sendString("b.firstChild");
  await waitForEagerEvaluationResult(hud, `#text "mydivtext"`);
  data = await onHighlighterShown;
  is(
    data.nodeFront.displayName,
    "#text",
    "The correct text node was highlighted"
  );
  isVisible = await highlighterTestFront.isHighlighting();
  is(isVisible, true, "Highlighter is displayed");

  onHighlighterHidden = highlighter.waitForHighlighterHidden();
  EventUtils.synthesizeKey("KEY_Enter");
  await waitFor(() => findEvaluationResultMessage(hud, `#text "mydivtext"`));
  await waitForNoEagerEvaluationResult(hud);
  isVisible = await highlighterTestFront.isHighlighting();
  is(isVisible, false, "Highlighter is closed after evaluating the expression");
});
