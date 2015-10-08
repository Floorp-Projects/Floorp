/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test different kinds of drag and drop node re-ordering

const TEST_URL = TEST_URL_ROOT + "doc_markup_dragdrop.html";
const GRAB_DELAY = 5;

add_task(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);
  inspector.markup.GRAB_DELAY = GRAB_DELAY;

  info("Expanding #test");
  let parentFront = yield getNodeFront("#test", inspector);
  let parent = yield getNode("#test");
  let parentContainer = yield getContainerForNodeFront(parentFront, inspector);

  yield inspector.markup.expandNode(parentFront);
  yield waitForMultipleChildrenUpdates(inspector);

  parentContainer.elt.scrollIntoView(true);

  info("Testing putting an element back in it's original place");
  yield dragElementToOriginalLocation("#firstChild", inspector);
  is(parent.children[0].id, "firstChild", "#firstChild is still the first child of #test");
  is(parent.children[1].id, "middleChild", "#middleChild is still the second child of #test");

  info("Testing switching elements inside their parent");
  yield moveElementDown("#firstChild", "#middleChild", inspector);

  is(parent.children[0].id, "middleChild", "#firstChild is now the second child of #test");
  is(parent.children[1].id, "firstChild", "#middleChild is now the first child of #test");

  info("Testing switching elements with a last child");
  yield moveElementDown("#firstChild", "#lastChild", inspector);

  is(parent.children[1].id, "lastChild", "#lastChild is now the second child of #test");
  is(parent.children[2].id, "firstChild", "#firstChild is now the last child of #test");

  info("Testing appending element to a parent");
  yield moveElementDown("#before", "#test", inspector);

  is(parent.children.length, 4, "New element appended to #test");
  is(parent.children[0].id, "before", "New element is appended at the right place (currently first child)");

  info("Testing moving element to after it's parent");
  yield moveElementDown("#firstChild", "#test", inspector);

  is(parent.children.length, 3, "#firstChild is no longer #test's child");
  is(parent.nextElementSibling.id, "firstChild", "#firstChild is now #test's nextElementSibling");
});

function* dragContainer(selector, targetOffset, inspector) {
  info("Dragging the markup-container for node " + selector);

  let container = yield getContainerForSelector(selector, inspector);

  let updated = inspector.once("inspector-updated");

  let rect = {
    x: container.tagLine.offsetLeft,
    y: container.tagLine.offsetTop
  };

  info("Simulating mouseDown on " + selector);
  container._onMouseDown({
    target: container.tagLine,
    pageX: rect.x,
    pageY: rect.y,
    stopPropagation: function() {},
    preventDefault: function() {}
  });

  let targetX = rect.x + targetOffset.x,
      targetY = rect.y + targetOffset.y;

  setTimeout(() => {
    info("Simulating mouseMove on " + selector +
         " with pageX: " + targetX + " pageY: " + targetY);
    container._onMouseMove({
      target: container.tagLine,
      pageX: targetX,
      pageY: targetY
    });

    info("Simulating mouseUp on " + selector +
         " with pageX: " + targetX + " pageY: " + targetY);
    container._onMouseUp({
      target: container.tagLine,
      pageX: targetX,
      pageY: targetY
    });

    container.markup._onMouseUp();
  }, GRAB_DELAY+1);

  return updated;
};

function* dragElementToOriginalLocation(selector, inspector) {
  let el = yield getContainerForSelector(selector, inspector);
  let height = el.tagLine.getBoundingClientRect().height;

  info("Picking up and putting back down " + selector);

  function onMutation() {
    ok(false, "Mutation received from dragging a node back to its location");
  }
  inspector.on("markupmutation", onMutation);
  yield dragContainer(selector, {x: 0, y: 0}, inspector);

  // Wait a bit to make sure the event never fires.
  // This doesn't need to catch *all* cases, since the mutation
  // will cause failure later in the test when it checks element ordering.
  yield new Promise(resolve => {
    setTimeout(resolve, 500);
  });
  inspector.off("markupmutation", onMutation);
}

function* moveElementDown(selector, next, inspector) {
  let onMutated = inspector.once("markupmutation");
  let uiUpdate = inspector.once("inspector-updated");

  let el = yield getContainerForSelector(next, inspector);
  let height = el.tagLine.getBoundingClientRect().height;

  info("Switching " + selector + ' with ' + next);

  yield dragContainer(selector, {x: 0, y: Math.round(height) + 2}, inspector);

  let mutations = yield onMutated;
  is(mutations.length, 2, "2 mutations");
  yield uiUpdate;
};