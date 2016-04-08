/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that pseudo-elements and anonymous nodes are not draggable.

const TEST_URL = URL_ROOT + "doc_markup_dragdrop.html";
const PREF = "devtools.inspector.showAllAnonymousContent";

add_task(function* () {
  Services.prefs.setBoolPref(PREF, true);

  let {inspector} = yield openInspectorForURL(TEST_URL);

  info("Expanding nodes below #test");
  let parentFront = yield getNodeFront("#test", inspector);
  yield inspector.markup.expandNode(parentFront);
  yield waitForMultipleChildrenUpdates(inspector);

  info("Getting the ::before pseudo element and selecting it");
  let parentContainer = yield getContainerForNodeFront(parentFront, inspector);
  let beforePseudo = parentContainer.elt.children[1].firstChild.container;
  parentContainer.elt.scrollIntoView(true);
  yield selectNode(beforePseudo.node, inspector);

  info("Simulate dragging the ::before pseudo element");
  yield simulateNodeDrag(inspector, beforePseudo);

  ok(!beforePseudo.isDragging, "::before pseudo element isn't dragging");

  info("Expanding nodes below #anonymousParent");
  let inputFront = yield getNodeFront("#anonymousParent", inspector);
  yield inspector.markup.expandNode(inputFront);
  yield waitForMultipleChildrenUpdates(inspector);

  info("Getting the anonymous node and selecting it");
  let inputContainer = yield getContainerForNodeFront(inputFront, inspector);
  let anonymousDiv = inputContainer.elt.children[1].firstChild.container;
  inputContainer.elt.scrollIntoView(true);
  yield selectNode(anonymousDiv.node, inspector);

  info("Simulate dragging the anonymous node");
  yield simulateNodeDrag(inspector, anonymousDiv);

  ok(!anonymousDiv.isDragging, "anonymous node isn't dragging");
});
