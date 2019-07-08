/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check expanding/collapsing object inspector in the console.
const TEST_URI = "data:text/html;charset=utf8,<h1>test Object Inspector</h1>";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  logAllStoreChanges(hud);

  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    content.wrappedJSObject.console.log("oi-test", [1, 2, { a: "a", b: "b" }], {
      c: "c",
      d: [3, 4],
      length: 987,
    });
  });

  const node = await waitFor(() => findMessage(hud, "oi-test"));
  const objectInspectors = [...node.querySelectorAll(".tree")];
  is(
    objectInspectors.length,
    2,
    "There is the expected number of object inspectors"
  );

  const [arrayOi, objectOi] = objectInspectors;

  info("Expanding the array object inspector");

  let onArrayOiMutation = waitForNodeMutation(arrayOi, {
    childList: true,
  });

  arrayOi.querySelector(".arrow").click();
  await onArrayOiMutation;

  ok(
    arrayOi.querySelector(".arrow").classList.contains("expanded"),
    "The arrow of the root node of the tree is expanded after clicking on it"
  );

  let arrayOiNodes = arrayOi.querySelectorAll(".node");

  // The object inspector now looks like:
  // ▼ […]
  // |  0: 1
  // |  1: 2
  // |  ▶︎ 2: {a: "a", b: "b"}
  // |  length: 3
  // |  ▶︎ <prototype>
  is(
    arrayOiNodes.length,
    6,
    "There is the expected number of nodes in the tree"
  );

  info("Expanding a leaf of the array object inspector");
  let arrayOiNestedObject = arrayOiNodes[3];
  onArrayOiMutation = waitForNodeMutation(arrayOi, {
    childList: true,
  });

  arrayOiNestedObject.querySelector(".arrow").click();
  await onArrayOiMutation;

  ok(
    arrayOiNestedObject.querySelector(".arrow").classList.contains("expanded"),
    "The arrow of the root node of the tree is expanded after clicking on it"
  );

  arrayOiNodes = arrayOi.querySelectorAll(".node");

  // The object inspector now looks like:
  // ▼ […]
  // |  0: 1
  // |  1: 2
  // |  ▼ 2: {…}
  // |  |  a: "a"
  // |  |  b: "b"
  // |  |  ▶︎ <prototype>
  // |  length: 3
  // |  ▶︎ <prototype>
  is(
    arrayOiNodes.length,
    9,
    "There is the expected number of nodes in the tree"
  );

  info("Collapsing the root");
  onArrayOiMutation = waitForNodeMutation(arrayOi, {
    childList: true,
  });
  arrayOi.querySelector(".arrow").click();

  is(
    arrayOi.querySelector(".arrow").classList.contains("expanded"),
    false,
    "The arrow of the root node of the tree is collapsed after clicking on it"
  );

  arrayOiNodes = arrayOi.querySelectorAll(".node");
  is(arrayOiNodes.length, 1, "Only the root node is visible");

  info("Expanding the root again");
  onArrayOiMutation = waitForNodeMutation(arrayOi, {
    childList: true,
  });
  arrayOi.querySelector(".arrow").click();

  ok(
    arrayOi.querySelector(".arrow").classList.contains("expanded"),
    "The arrow of the root node of the tree is expanded again after clicking on it"
  );

  arrayOiNodes = arrayOi.querySelectorAll(".node");
  arrayOiNestedObject = arrayOiNodes[3];
  ok(
    arrayOiNestedObject.querySelector(".arrow").classList.contains("expanded"),
    "The object tree is still expanded"
  );

  is(
    arrayOiNodes.length,
    9,
    "There is the expected number of nodes in the tree"
  );

  const onObjectOiMutation = waitForNodeMutation(objectOi, {
    childList: true,
  });

  objectOi.querySelector(".arrow").click();
  await onObjectOiMutation;

  ok(
    objectOi.querySelector(".arrow").classList.contains("expanded"),
    "The arrow of the root node of the tree is expanded after clicking on it"
  );

  const objectOiNodes = objectOi.querySelectorAll(".node");
  // The object inspector now looks like:
  // ▼ {…}
  // |  c: "c"
  // |  ▶︎ d: [3, 4]
  // |  length: 987
  // |  ▶︎ <prototype>
  is(
    objectOiNodes.length,
    5,
    "There is the expected number of nodes in the tree"
  );
});
