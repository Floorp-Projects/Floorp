/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URI = `data:text/html;charset=utf8,
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
add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm, toolbox } = hud;
  const { autocompletePopup } = jsterm;

  await registerTestActor(toolbox.target.client);
  const testActor = await getTestActor(toolbox);
  const inspectorFront = await toolbox.target.getFront("inspector");

  ok(!autocompletePopup.isOpen, "popup is not open");
  const onPopupOpen = autocompletePopup.once("popup-opened");
  let onNodeHighlight = inspectorFront.highlighter.once("node-highlight");
  EventUtils.sendString("x.");
  await onPopupOpen;

  await waitForEagerEvaluationResult(hud, `<h1 class="title">`);
  await onNodeHighlight;
  let nodeFront = await onNodeHighlight;
  is(nodeFront.displayName, "h1", "The correct node was highlighted");
  isVisible = await testActor.isHighlighting();
  is(isVisible, true, "Highlighter is displayed");

  onNodeHighlight = inspectorFront.highlighter.once("node-highlight");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  await waitForEagerEvaluationResult(hud, `<div id="mydiv">`);
  await onNodeHighlight;
  nodeFront = await onNodeHighlight;
  is(nodeFront.displayName, "div", "The correct node was highlighted");
  isVisible = await testActor.isHighlighting();
  is(isVisible, true, "Highlighter is displayed");

  let onNodeUnhighlight = inspectorFront.highlighter.once("node-unhighlight");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  await waitForEagerEvaluationResult(hud, `<hr>`);
  await onNodeUnhighlight;
  ok(true, "The highlighter isn't displayed on a non-connected element");

  info("Test that text nodes are highlighted");
  onNodeHighlight = inspectorFront.highlighter.once("node-highlight");
  EventUtils.sendString("b.firstChild");
  await waitForEagerEvaluationResult(hud, `#text "mydivtext"`);
  await onNodeHighlight;
  nodeFront = await onNodeHighlight;
  is(nodeFront.displayName, "#text", "The correct text node was highlighted");
  isVisible = await testActor.isHighlighting();
  is(isVisible, true, "Highlighter is displayed");

  onNodeUnhighlight = inspectorFront.highlighter.once("node-unhighlight");
  EventUtils.synthesizeKey("KEY_Enter");
  await waitFor(() => findMessage(hud, `#text "mydivtext"`, ".result"));
  await waitForNoEagerEvaluationResult(hud);
  isVisible = await testActor.isHighlighting();
  is(isVisible, false, "Highlighter is closed after evaluating the expression");
});
