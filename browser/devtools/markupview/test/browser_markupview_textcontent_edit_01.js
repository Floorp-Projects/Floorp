/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test editing a node's text content

const TEST_URL = TEST_URL_ROOT + "doc_markup_edit.html";

add_task(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);

  info("Expanding all nodes");
  yield inspector.markup.expandAll();
  yield waitForMultipleChildrenUpdates(inspector);

  yield editContainer(inspector, {
    selector: ".node6",
    newValue: "New text",
    oldValue: "line6"
  });

  yield editContainer(inspector, {
    selector: "#node17",
    newValue: "LOREM IPSUM DOLOR SIT AMET, CONSECTETUR ADIPISCING ELIT. DONEC POSUERE PLACERAT MAGNA ET IMPERDIET.",
    oldValue: "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec posuere placerat magna et imperdiet.",
    shortValue: true
  });

  yield editContainer(inspector, {
    selector: "#node17",
    newValue: "New value",
    oldValue: "LOREM IPSUM DOLOR SIT AMET, CONSECTETUR ADIPISCING ELIT. DONEC POSUERE PLACERAT MAGNA ET IMPERDIET.",
    shortValue: true
  });
});

function* editContainer(inspector, {selector, newValue, oldValue, shortValue}) {
  let node = getNode(selector).firstChild;
  is(node.nodeValue, oldValue, "The test node's text content is correct");

  info("Changing the text content");
  let onMutated = inspector.once("markupmutation");
  let container = yield getContainerForSelector(selector, inspector);
  let field = container.elt.querySelector("pre");

  if (shortValue) {
    is (oldValue.indexOf(field.textContent.substring(0, field.textContent.length - 1)), 0,
        "The shortened value starts with the full value " + field.textContent);
    ok (oldValue.length > field.textContent.length, "The shortened value is short");
  } else {
    is (field.textContent, oldValue, "The text node has the correct original value");
  }

  inspector.markup.markNodeAsSelected(container.node);

  if (shortValue) {
    info("Waiting for the text to be updated");
    yield inspector.markup.once("text-expand");
  }

  is (field.textContent, oldValue, "The text node has the correct original value after selecting");
  setEditableFieldValue(field, newValue, inspector);

  info("Listening to the markupmutation event");
  yield onMutated;

  is(node.nodeValue, newValue, "The test node's text content has changed");

  info("Selecting the <body> to reset the selection");
  let bodyContainer = yield getContainerForSelector("body", inspector);
  inspector.markup.markNodeAsSelected(bodyContainer.node);
}
