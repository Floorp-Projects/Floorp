/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test: pseudo-elements and anonymous nodes should not be draggable

const TEST_URL = TEST_URL_ROOT + "doc_markup_dragdrop.html";
const GRAB_DELAY = 400;

add_task(function*() {
  Services.prefs.setBoolPref("devtools.inspector.showAllAnonymousContent", true);

  let {inspector} = yield addTab(TEST_URL).then(openInspector);

  info("Expanding #test");
  let parentFront = yield getNodeFront("#test", inspector);
  yield inspector.markup.expandNode(parentFront);
  yield waitForMultipleChildrenUpdates(inspector);

  let parentContainer = yield getContainerForNodeFront(parentFront, inspector);
  let beforePseudo = parentContainer.elt.children[1].firstChild.container;

  parentContainer.elt.scrollIntoView(true);

  info("Simulating mouseDown on #test::before");
  beforePseudo._onMouseDown({
    target: beforePseudo.tagLine,
    stopPropagation: function() {},
    preventDefault: function() {}
  });

  info("Waiting " + (GRAB_DELAY + 1) + "ms")
  yield wait(GRAB_DELAY + 1);
  is(beforePseudo.isDragging, false, "[pseudo-element] isDragging is false after GRAB_DELAY has passed");

  let inputFront = yield getNodeFront("#anonymousParent", inspector);

  yield inspector.markup.expandNode(inputFront);
  yield waitForMultipleChildrenUpdates(inspector);

  let inputContainer = yield getContainerForNodeFront(inputFront, inspector);
  let anonymousDiv = inputContainer.elt.children[1].firstChild.container;

  inputContainer.elt.scrollIntoView(true);

  info("Simulating mouseDown on input#anonymousParent div");
  anonymousDiv._onMouseDown({
    target: anonymousDiv.tagLine,
    stopPropagation: function() {},
    preventDefault: function() {}
  });

  info("Waiting " + (GRAB_DELAY + 1) + "ms")
  yield wait(GRAB_DELAY + 1);
  is(anonymousDiv.isDragging, false, "[anonymous element] isDragging is false after GRAB_DELAY has passed");
});
