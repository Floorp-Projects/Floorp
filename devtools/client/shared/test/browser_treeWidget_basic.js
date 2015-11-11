/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the tree widget api works fine

const TEST_URI = "data:text/html;charset=utf-8,<head><link rel='stylesheet' " +
  "type='text/css' href='chrome://devtools/skin/common.css'><link " +
  "rel='stylesheet' type='text/css' href='chrome://devtools/skin/widg" +
  "ets.css'></head><body><div></div><span></span></body>";
const {TreeWidget} = require("devtools/client/shared/widgets/TreeWidget");

add_task(function*() {
  yield addTab("about:blank");
  let [host, win, doc] = yield createHost("bottom", TEST_URI);

  let tree = new TreeWidget(doc.querySelector("div"), {
    defaultType: "store"
  });

  populateTree(tree, doc);
  testTreeItemInsertedCorrectly(tree, doc);
  testAPI(tree, doc);
  populateUnsortedTree(tree, doc);
  testUnsortedTreeItemInsertedCorrectly(tree, doc);

  tree.destroy();
  host.destroy();
  gBrowser.removeCurrentTab();
});

function populateTree(tree, doc) {
  tree.add([{
    id: "level1",
    label: "Level 1"
  }, {
    id: "level2-1",
    label: "Level 2"
  }, {
    id: "level3-1",
    label: "Level 3 - Child 1",
    type: "dir"
  }]);
  tree.add(["level1", "level2-1", { id: "level3-2", label: "Level 3 - Child 2"}]);
  tree.add(["level1", "level2-1", { id: "level3-3", label: "Level 3 - Child 3"}]);
  tree.add(["level1", {
    id: "level2-2",
    label: "Level 2.1"
  }, {
    id: "level3-1",
    label: "Level 3.1"
  }]);
  tree.add([{
    id: "level1",
    label: "Level 1"
  }, {
    id: "level2",
    label: "Level 2"
  }, {
    id: "level3",
    label: "Level 3",
    type: "js"
  }]);
  tree.add(["level1.1", "level2", {id: "level3", type: "url"}]);
}

/**
 * Test if the nodes are inserted correctly in the tree.
 */
function testTreeItemInsertedCorrectly(tree, doc) {
  is(tree.root.children.children.length, 2, "Number of top level elements match");
  is(tree.root.children.firstChild.lastChild.children.length, 3,
     "Number of first second level elements match");
  is(tree.root.children.lastChild.lastChild.children.length, 1,
     "Number of second second level elements match");

  ok(tree.root.items.has("level1"), "Level1 top level element exists");
  is(tree.root.children.firstChild.dataset.id, JSON.stringify(["level1"]),
     "Data id of first top level element matches");
  is(tree.root.children.firstChild.firstChild.textContent, "Level 1",
     "Text content of first top level element matches");

  ok(tree.root.items.has("level1.1"), "Level1.1 top level element exists");
  is(tree.root.children.firstChild.nextSibling.dataset.id,
     JSON.stringify(["level1.1"]),
     "Data id of second top level element matches");
  is(tree.root.children.firstChild.nextSibling.firstChild.textContent, "level1.1",
     "Text content of second top level element matches");

  // Adding a new non text item in the tree.
  let node = doc.createElement("div");
  node.textContent = "Foo Bar";
  node.className = "foo bar";
  tree.add([{
    id: "level1.2",
    node: node,
    attachment: {
      foo: "bar"
    }
  }]);

  is(tree.root.children.children.length, 3,
     "Number of top level elements match after update");
  ok(tree.root.items.has("level1.2"), "New level node got added");
  ok(tree.attachments.has(JSON.stringify(["level1.2"])),
     "Attachment is present for newly added node");
  // The item should be added before level1 and level 1.1 as lexical sorting
  is(tree.root.children.firstChild.dataset.id, JSON.stringify(["level1.2"]),
     "Data id of last top level element matches");
  is(tree.root.children.firstChild.firstChild.firstChild, node,
     "Newly added node is inserted at the right location");
}

/**
 * Populate the unsorted tree.
 */
function populateUnsortedTree(tree, doc) {
  tree.sorted = false;

  tree.add([{ id: "g-1", label: "g-1"}])
  tree.add(["g-1", { id: "d-2", label: "d-2.1"}]);
  tree.add(["g-1", { id: "b-2", label: "b-2.2"}]);
  tree.add(["g-1", { id: "a-2", label: "a-2.3"}]);
}

/**
 * Test if the nodes are inserted correctly in the unsorted tree.
 */
function testUnsortedTreeItemInsertedCorrectly(tree, doc) {
  ok(tree.root.items.has("g-1"), "g-1 top level element exists");

  is(tree.root.children.firstChild.lastChild.children.length, 3,
    "Number of children for g-1 matches");
  is(tree.root.children.firstChild.dataset.id, JSON.stringify(["g-1"]),
    "Data id of g-1 matches");
  is(tree.root.children.firstChild.firstChild.textContent, "g-1",
    "Text content of g-1 matches");
  is(tree.root.children.firstChild.lastChild.firstChild.dataset.id,
    JSON.stringify(["g-1", "d-2"]),
    "Data id of d-2 matches");
  is(tree.root.children.firstChild.lastChild.firstChild.textContent, "d-2.1",
    "Text content of d-2 matches");
  is(tree.root.children.firstChild.lastChild.firstChild.nextSibling.textContent,
    "b-2.2", "Text content of b-2 matches");
  is(tree.root.children.firstChild.lastChild.lastChild.textContent, "a-2.3",
    "Text content of a-2 matches");
}

/**
 * Tests if the API exposed by TreeWidget works properly
 */
function testAPI(tree, doc) {
  info("Testing TreeWidget API");
  // Check if selectItem and selectedItem setter works as expected
  // Nothing should be selected beforehand
  ok(!doc.querySelector(".theme-selected"), "Nothing is selected");
  tree.selectItem(["level1"]);
  let node = doc.querySelector(".theme-selected");
  ok(!!node, "Something got selected");
  is(node.parentNode.dataset.id, JSON.stringify(["level1"]),
     "Correct node selected");

  tree.selectItem(["level1", "level2"]);
  let node2 = doc.querySelector(".theme-selected");
  ok(!!node2, "Something is still selected");
  isnot(node, node2, "Newly selected node is different from previous");
  is(node2.parentNode.dataset.id, JSON.stringify(["level1", "level2"]),
     "Correct node selected");

  // test if selectedItem getter works
  is(tree.selectedItem.length, 2, "Correct length of selected item");
  is(tree.selectedItem[0], "level1", "Correct selected item");
  is(tree.selectedItem[1], "level2", "Correct selected item");

  // test if isSelected works
  ok(tree.isSelected(["level1", "level2"]), "isSelected works");

  tree.selectedItem = ["level1"];
  let node3 = doc.querySelector(".theme-selected");
  ok(!!node3, "Something is still selected");
  isnot(node2, node3, "Newly selected node is different from previous");
  is(node3, node, "First and third selected nodes should be same");
  is(node3.parentNode.dataset.id, JSON.stringify(["level1"]),
     "Correct node selected");

  // test if selectedItem getter works
  is(tree.selectedItem.length, 1, "Correct length of selected item");
  is(tree.selectedItem[0], "level1", "Correct selected item");

  // test if clear selection works
  tree.clearSelection();
  ok(!doc.querySelector(".theme-selected"),
     "Nothing selected after clear selection call")

  // test if collapseAll/expandAll work
  ok(doc.querySelectorAll("[expanded]").length > 0,
     "Some nodes are expanded");
  tree.collapseAll();
  is(doc.querySelectorAll("[expanded]").length, 0,
     "Nothing is expanded after collapseAll call");
  tree.expandAll();
  is(doc.querySelectorAll("[expanded]").length, 13,
     "All tree items expanded after expandAll call");

  // test if selectNextItem and selectPreviousItem work
  tree.selectedItem = ["level1", "level2"];
  ok(tree.isSelected(["level1", "level2"]), "Correct item selected");
  tree.selectNextItem();
  ok(tree.isSelected(["level1", "level2", "level3"]),
     "Correct item selected after selectNextItem call");

  tree.selectNextItem();
  ok(tree.isSelected(["level1", "level2-1"]),
     "Correct item selected after second selectNextItem call");

  tree.selectNextItem();
  ok(tree.isSelected(["level1", "level2-1", "level3-1"]),
     "Correct item selected after third selectNextItem call");

  tree.selectPreviousItem();
  ok(tree.isSelected(["level1", "level2-1"]),
     "Correct item selected after selectPreviousItem call");

  tree.selectPreviousItem();
  ok(tree.isSelected(["level1", "level2", "level3"]),
     "Correct item selected after second selectPreviousItem call");

  // test if remove works
  ok(doc.querySelector("[data-id='" +
       JSON.stringify(["level1", "level2", "level3"]) + "']"),
     "level1-level2-level3 item exists before removing");
  tree.remove(["level1", "level2", "level3"]);
  ok(!doc.querySelector("[data-id='" +
       JSON.stringify(["level1", "level2", "level3"]) + "']"),
     "level1-level2-level3 item does not exist after removing");
  tree.add([{
    id: "level1",
    label: "Level 1"
  }, {
    id: "level2",
    label: "Level 2"
  }, {
    id: "level3",
    label: "Level 3",
    type: "js"
  }]);

  // test if clearing the tree works
  is(doc.querySelectorAll("[level='1']").length, 3,
     "Correct number of top level items before clearing");
  tree.clear();
  is(doc.querySelectorAll("[level='1']").length, 0,
     "No top level item after clearing the tree");
}
