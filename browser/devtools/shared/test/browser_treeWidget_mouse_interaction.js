/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that mouse interaction works fine with tree widget

const TEST_URI = "data:text/html;charset=utf-8,<head><link rel='stylesheet' " +
  "type='text/css' href='chrome://browser/skin/devtools/common.css'><link " +
  "rel='stylesheet' type='text/css' href='chrome://browser/skin/devtools/widg" +
  "ets.css'></head><body><div></div><span></span></body>";
const {TreeWidget} = devtools.require("devtools/shared/widgets/TreeWidget");
let {Task} = devtools.require("resource://gre/modules/Task.jsm");
let {Promise} = devtools.require("resource://gre/modules/Promise.jsm");

let doc, tree;

function test() {
  waitForExplicitFinish();
  addTab(TEST_URI, () => {
    doc = content.document;
    tree = new TreeWidget(doc.querySelector("div"), {
      defaultType: "store"
    });
    startTests();
  });
}

function endTests() {
  tree.destroy();
  doc = tree = null;
  gBrowser.removeCurrentTab();
  finish();
}

let startTests = Task.async(function*() {
  populateTree();
  yield testMouseInteraction();
  endTests();
});

function populateTree() {
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
}

// Sends a click event on the passed DOM node in an async manner
function click(node) {
  executeSoon(() => EventUtils.synthesizeMouseAtCenter(node, {}, content));
}

/**
 * Tests if clicking the tree items does the expected behavior
 */
let testMouseInteraction = Task.async(function*() {
  info("Testing mouse interaction with the tree");
  let event;
  let pass = (e, d, a) => event.resolve([e, d, a]);

  ok(!tree.selectedItem, "Nothing should be selected beforehand");

  tree.once("select", pass);
  let node = tree.root.children.firstChild.firstChild;
  info("clicking on first top level item");
  event = Promise.defer();
  ok(!node.classList.contains("theme-selected"),
     "Node should not have selected class before clicking");
  click(node);
  let [name, data, attachment] = yield event.promise;
  ok(node.classList.contains("theme-selected"),
     "Node has selected class after click");
  is(data[0], "level1.2", "Correct tree path is emitted")
  ok(attachment && attachment.foo, "Correct attachment is emitted")
  is(attachment.foo, "bar", "Correct attachment value is emitted");

  info("clicking second top level item with children to check if it expands");
  let node2 = tree.root.children.firstChild.nextSibling.firstChild;
  event = Promise.defer();
  // node should not have selected class
  ok(!node2.classList.contains("theme-selected"),
     "New node should not have selected class before clicking");
  ok(!node2.hasAttribute("expanded"), "New node is not expanded before clicking");
  tree.once("select", pass);
  click(node2);
  let [name, data, attachment] = yield event.promise;
  ok(node2.classList.contains("theme-selected"),
     "New node has selected class after clicking");
  is(data[0], "level1", "Correct tree path is emitted for new node")
  ok(!attachment, "null attachment should be emitted for new node")
  ok(node2.hasAttribute("expanded"), "New node expanded after click");

  ok(!node.classList.contains("theme-selected"),
     "Old node should not have selected class after the click on new node");


  // clicking again should just collapse
  // this will not emit "select" event
  event = Promise.defer();
  node2.addEventListener("click", function onClick() {
    node2.removeEventListener("click", onClick);
    executeSoon(() => event.resolve(null));
  });
  click(node2);
  yield event.promise;
  ok(!node2.hasAttribute("expanded"), "New node collapsed after click again");
});
