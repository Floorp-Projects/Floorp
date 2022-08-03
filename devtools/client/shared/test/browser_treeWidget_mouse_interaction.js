/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that mouse interaction works fine with tree widget

const TEST_URI =
  "data:text/html;charset=utf-8,<head>" +
  "<link rel='stylesheet' type='text/css' href='chrome://devtools/skin/widg" +
  "ets.css'></head><body><div></div><span></span></body>";
const { TreeWidget } = require("devtools/client/shared/widgets/TreeWidget");

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["security.allow_unsafe_parent_loads", true]],
  });

  await addTab("about:blank");
  const { host, doc } = await createHost("bottom", TEST_URI);

  // Creating a host is not correctly waiting when DevTools run in content frame
  // See Bug 1571421.
  await wait(1000);

  const tree = new TreeWidget(doc.querySelector("div"), {
    defaultType: "store",
  });

  populateTree(tree, doc);
  await testMouseInteraction(tree);

  tree.destroy();
  host.destroy();
  gBrowser.removeCurrentTab();
});

function populateTree(tree, doc) {
  tree.add([
    {
      id: "level1",
      label: "Level 1",
    },
    {
      id: "level2-1",
      label: "Level 2",
    },
    {
      id: "level3-1",
      label: "Level 3 - Child 1",
      type: "dir",
    },
  ]);
  tree.add([
    "level1",
    "level2-1",
    { id: "level3-2", label: "Level 3 - Child 2" },
  ]);
  tree.add([
    "level1",
    "level2-1",
    { id: "level3-3", label: "Level 3 - Child 3" },
  ]);
  tree.add([
    "level1",
    {
      id: "level2-2",
      label: "Level 2.1",
    },
    {
      id: "level3-1",
      label: "Level 3.1",
    },
  ]);
  tree.add([
    {
      id: "level1",
      label: "Level 1",
    },
    {
      id: "level2",
      label: "Level 2",
    },
    {
      id: "level3",
      label: "Level 3",
      type: "js",
    },
  ]);
  tree.add(["level1.1", "level2", { id: "level3", type: "url" }]);

  // Adding a new non text item in the tree.
  const node = doc.createElement("div");
  node.textContent = "Foo Bar";
  node.className = "foo bar";
  tree.add([
    {
      id: "level1.2",
      node,
      attachment: {
        foo: "bar",
      },
    },
  ]);
}

// Sends a click event on the passed DOM node in an async manner
function click(node) {
  const win = node.ownerDocument.defaultView;
  executeSoon(() => EventUtils.synthesizeMouseAtCenter(node, {}, win));
}

/**
 * Tests if clicking the tree items does the expected behavior
 */
async function testMouseInteraction(tree) {
  info("Testing mouse interaction with the tree");
  const waitForSelect = () =>
    new Promise(resolve => {
      tree.once("select", (d, a) => resolve({ data: d, attachment: a }));
    });

  ok(!tree.selectedItem, "Nothing should be selected beforehand");

  let onTreeSelect = waitForSelect();
  const node = tree.root.children.firstChild.firstChild;
  info("clicking on first top level item");
  ok(
    !node.classList.contains("theme-selected"),
    "Node should not have selected class before clicking"
  );
  click(node);
  let { data, attachment } = await onTreeSelect;
  ok(
    node.classList.contains("theme-selected"),
    "Node has selected class after click"
  );
  is(data[0], "level1.2", "Correct tree path is emitted");
  ok(attachment && attachment.foo, "Correct attachment is emitted");
  is(attachment.foo, "bar", "Correct attachment value is emitted");

  info("clicking second top level item with children to check if it expands");
  const node2 = tree.root.children.firstChild.nextSibling.firstChild;
  // node should not have selected class
  ok(
    !node2.classList.contains("theme-selected"),
    "New node should not have selected class before clicking"
  );
  ok(
    !node2.hasAttribute("expanded"),
    "New node is not expanded before clicking"
  );
  onTreeSelect = waitForSelect();
  click(node2);
  ({ data, attachment } = await onTreeSelect);
  ok(
    node2.classList.contains("theme-selected"),
    "New node has selected class after clicking"
  );
  is(data[0], "level1", "Correct tree path is emitted for new node");
  ok(!attachment, "null attachment should be emitted for new node");
  ok(node2.hasAttribute("expanded"), "New node expanded after click");

  ok(
    !node.classList.contains("theme-selected"),
    "Old node should not have selected class after the click on new node"
  );

  // clicking again should just collapse
  // this will not emit "select" event
  const onClick = new Promise(resolve => {
    node2.addEventListener(
      "click",
      () => {
        executeSoon(() => resolve(null));
      },
      { once: true }
    );
  });
  click(node2);
  await onClick;
  ok(!node2.hasAttribute("expanded"), "New node collapsed after click again");
}
