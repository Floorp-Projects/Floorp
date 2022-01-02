/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that keyboard interaction works fine with the tree widget

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
  const { host, win, doc } = await createHost("bottom", TEST_URI);

  // Creating a host is not correctly waiting when DevTools run in content frame
  // See Bug 1571421.
  await wait(1000);

  const tree = new TreeWidget(doc.querySelector("div"), {
    defaultType: "store",
  });

  populateTree(tree, doc);

  await testKeyboardInteraction(tree, win);

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
      node: node,
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
 * Tests if pressing navigation keys on the tree items does the expected behavior
 */
async function testKeyboardInteraction(tree, win) {
  info("Testing keyboard interaction with the tree");
  const waitForSelect = () =>
    new Promise(resolve => {
      tree.once("select", (d, a) => resolve({ data: d, attachment: a }));
    });

  info("clicking on first top level item");
  let node = tree.root.children.firstChild.firstChild;

  // The select event handler will be called before the click event hasn't
  // fully finished, so wait for both of them.
  const clicked = once(node, "click");
  let onTreeSelect = waitForSelect();
  click(node);

  info("Wait for the click event");
  await clicked;

  info("Wait for the select event on tree");
  await onTreeSelect;

  node = tree.root.children.firstChild.nextSibling.firstChild;
  // node should not have selected class
  ok(
    !node.classList.contains("theme-selected"),
    "Node should not have selected class"
  );
  ok(!node.hasAttribute("expanded"), "Node is not expanded");

  info("Pressing down key to select next item");
  onTreeSelect = waitForSelect();
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);

  let { data, attachment } = await onTreeSelect;
  is(data.length, 1, "Correct level item was selected after keydown");
  is(data[0], "level1", "Correct item was selected after pressing down");
  ok(!attachment, "null attachment was emitted");
  ok(node.classList.contains("theme-selected"), "Node has selected class");
  ok(node.hasAttribute("expanded"), "Node is expanded now");

  info("Pressing down key again to select next item");
  onTreeSelect = waitForSelect();
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  ({ data, attachment } = await onTreeSelect);
  is(
    data.length,
    2,
    "Correct level item was selected after second down keypress"
  );
  is(data[0], "level1", "Correct parent level");
  is(data[1], "level2", "Correct second level");

  info("Pressing down key again to select next item");
  onTreeSelect = waitForSelect();
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  ({ data, attachment } = await onTreeSelect);
  is(
    data.length,
    3,
    "Correct level item was selected after third down keypress"
  );
  is(data[0], "level1", "Correct parent level");
  is(data[1], "level2", "Correct second level");
  is(data[2], "level3", "Correct third level");

  info("Pressing down key again to select next item");
  onTreeSelect = waitForSelect();
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  ({ data, attachment } = await onTreeSelect);
  is(
    data.length,
    2,
    "Correct level item was selected after fourth down keypress"
  );
  is(data[0], "level1", "Correct parent level");
  is(data[1], "level2-1", "Correct second level");

  const waitForKeydown = () =>
    new Promise(resolve => {
      tree.root.children.addEventListener(
        "keydown",
        () => {
          // executeSoon so that other listeners on the same method are executed first
          executeSoon(() => resolve());
        },
        { once: true }
      );
    });

  // pressing left to check expand collapse feature.
  // This does not emit any event, so listening for keypress
  let onTreeKeydown = waitForKeydown();
  info("Pressing left key to collapse the item");
  node = tree._selectedLabel;
  ok(node.hasAttribute("expanded"), "Item is expanded before left keypress");
  EventUtils.synthesizeKey("KEY_ArrowLeft", {}, win);
  await onTreeKeydown;

  ok(
    !node.hasAttribute("expanded"),
    "Item is not expanded after left keypress"
  );

  // pressing left on collapsed item should select the previous item

  info("Pressing left key on collapsed item to select previous");
  onTreeSelect = waitForSelect();
  // parent node should have no effect of this keypress
  node = tree.root.children.firstChild.nextSibling.firstChild;
  ok(node.hasAttribute("expanded"), "Parent is expanded");
  EventUtils.synthesizeKey("KEY_ArrowLeft", {}, win);
  ({ data } = await onTreeSelect);
  is(
    data.length,
    3,
    "Correct level item was selected after second left keypress"
  );
  is(data[0], "level1", "Correct parent level");
  is(data[1], "level2", "Correct second level");
  is(data[2], "level3", "Correct third level");
  ok(
    node.hasAttribute("expanded"),
    "Parent is still expanded after left keypress"
  );

  // pressing down again

  info("Pressing down key to select next item");
  onTreeSelect = waitForSelect();
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  ({ data, attachment } = await onTreeSelect);
  is(
    data.length,
    2,
    "Correct level item was selected after fifth down keypress"
  );
  is(data[0], "level1", "Correct parent level");
  is(data[1], "level2-1", "Correct second level");

  // collapsing the item to check expand feature.
  onTreeKeydown = waitForKeydown();
  info("Pressing left key to collapse the item");
  node = tree._selectedLabel;
  ok(node.hasAttribute("expanded"), "Item is expanded before left keypress");
  EventUtils.synthesizeKey("KEY_ArrowLeft", {}, win);
  await onTreeKeydown;
  ok(!node.hasAttribute("expanded"), "Item is collapsed after left keypress");

  // pressing right should expand this now.
  onTreeKeydown = waitForKeydown();
  info("Pressing right key to expend the collapsed item");
  node = tree._selectedLabel;
  ok(!node.hasAttribute("expanded"), "Item is collapsed before right keypress");
  EventUtils.synthesizeKey("KEY_ArrowRight", {}, win);
  await onTreeKeydown;
  ok(node.hasAttribute("expanded"), "Item is expanded after right keypress");

  // selecting last item node to test edge navigation case

  tree.selectedItem = ["level1.1", "level2", "level3"];
  node = tree._selectedLabel;
  // pressing down again should not change selection
  onTreeKeydown = waitForKeydown();
  info("Pressing down key on last item of the tree");
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  await onTreeKeydown;

  ok(
    tree.isSelected(["level1.1", "level2", "level3"]),
    "Last item is still selected after pressing down on last item of the tree"
  );
}
