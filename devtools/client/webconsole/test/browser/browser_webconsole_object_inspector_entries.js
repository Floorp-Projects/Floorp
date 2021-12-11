/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check expanding/collapsing maps and sets in the console.
const TEST_URI =
  "data:text/html;charset=utf8,<h1>Object Inspector on Maps & Sets</h1>";
const { ELLIPSIS } = require("devtools/shared/l10n");

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  logAllStoreChanges(hud);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.wrappedJSObject.console.log(
      "oi-entries-test",
      new Map(
        Array.from({ length: 2 }).map((el, i) => [{ key: i }, content.document])
      ),
      new Map(Array.from({ length: 20 }).map((el, i) => [Symbol(i), i])),
      new Map(Array.from({ length: 331 }).map((el, i) => [Symbol(i), i])),
      new Set(Array.from({ length: 2 }).map((el, i) => ({ value: i }))),
      new Set(Array.from({ length: 20 }).map((el, i) => i)),
      new Set(Array.from({ length: 222 }).map((el, i) => i))
    );
  });

  const node = await waitFor(() => findMessage(hud, "oi-entries-test"));
  const objectInspectors = [...node.querySelectorAll(".tree")];
  is(
    objectInspectors.length,
    6,
    "There is the expected number of object inspectors"
  );

  const [
    smallMapOi,
    mapOi,
    largeMapOi,
    smallSetOi,
    setOi,
    largeSetOi,
  ] = objectInspectors;

  await testSmallMap(smallMapOi);
  await testMap(mapOi);
  await testLargeMap(largeMapOi);
  await testSmallSet(smallSetOi);
  await testSet(setOi);
  await testLargeSet(largeSetOi);
});

async function testSmallMap(oi) {
  info("Expanding the Map");
  let onMapOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  oi.querySelector(".arrow").click();
  await onMapOiMutation;

  ok(
    oi.querySelector(".arrow").classList.contains("expanded"),
    "The arrow of the node has the expected class after clicking on it"
  );

  let oiNodes = oi.querySelectorAll(".node");
  // There are 4 nodes: the root, size, entries and the proto.
  is(oiNodes.length, 4, "There is the expected number of nodes in the tree");

  info("Expanding the <entries> leaf of the map");
  const entriesNode = oiNodes[2];
  is(
    entriesNode.textContent,
    "<entries>",
    "There is the expected <entries> node"
  );
  onMapOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  entriesNode.querySelector(".arrow").click();
  await onMapOiMutation;

  oiNodes = oi.querySelectorAll(".node");
  // There are now 6 nodes, the 4 original ones, and the 2 entries.
  is(oiNodes.length, 6, "There is the expected number of nodes in the tree");

  info("Expand first entry");
  onMapOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });
  oiNodes[3].querySelector(".arrow").click();
  await onMapOiMutation;

  oiNodes = oi.querySelectorAll(".node");
  /*
   * ▼ Map (2)
   * |   size: 2
   * | ▼ <entries>
   * | | ▼ 0: {…} -> HTMLDocument
   * | | | ▶︎ <key>: Object {…}
   * | | | ▶︎ <value>: HTMLDocument
   * | | ▶︎ 1: {…} -> HTMLDocument
   * | ▶︎ <prototype>
   */
  is(oiNodes.length, 8, "There is the expected number of nodes in the tree");

  info("Expand <key> for first entry");
  onMapOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });
  oiNodes[4].querySelector(".arrow").click();
  await onMapOiMutation;

  oiNodes = oi.querySelectorAll(".node");
  /*
   * ▼ Map (2)
   * |   size: 2
   * | ▼ <entries>
   * | | ▼ 0: {…} -> HTMLDocument
   * | | | ▼ <key>: Object {…}
   * | | | |   key: 0
   * | | | | ▶︎ <prototype>
   * | | | ▶︎ <value>: HTMLDocument
   * | | ▶︎ 1: {…} -> HTMLDocument
   * | ▶︎ <prototype>
   */
  is(oiNodes.length, 10, "There is the expected number of nodes in the tree");

  info("Expand <value> for first entry");
  onMapOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });
  oiNodes[7].querySelector(".arrow").click();
  await onMapOiMutation;

  oiNodes = oi.querySelectorAll(".node");
  ok(oiNodes.length > 10, "The document node was expanded");
}

async function testMap(oi) {
  info("Expanding the Map");
  let onMapOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  oi.querySelector(".arrow").click();
  await onMapOiMutation;

  ok(
    oi.querySelector(".arrow").classList.contains("expanded"),
    "The arrow of the node has the expected class after clicking on it"
  );

  let oiNodes = oi.querySelectorAll(".node");
  // There are 4 nodes: the root, size, entries and the proto.
  is(oiNodes.length, 4, "There is the expected number of nodes in the tree");

  info("Expanding the <entries> leaf of the map");
  const entriesNode = oiNodes[2];
  is(
    entriesNode.textContent,
    "<entries>",
    "There is the expected <entries> node"
  );
  onMapOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  entriesNode.querySelector(".arrow").click();
  await onMapOiMutation;

  oiNodes = oi.querySelectorAll(".node");
  // There are now 24 nodes, the 4 original ones, and the 20 entries.
  is(oiNodes.length, 24, "There is the expected number of nodes in the tree");
}

async function testLargeMap(oi) {
  info("Expanding the large map");
  let onMapOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  oi.querySelector(".arrow").click();
  await onMapOiMutation;

  ok(
    oi.querySelector(".arrow").classList.contains("expanded"),
    "The arrow of the node has the expected class after clicking on it"
  );

  let oiNodes = oi.querySelectorAll(".node");
  // There are 4 nodes: the root, size, entries and the proto.
  is(oiNodes.length, 4, "There is the expected number of nodes in the tree");

  info("Expanding the <entries> leaf of the map");
  const entriesNode = oiNodes[2];
  is(
    entriesNode.textContent,
    "<entries>",
    "There is the expected <entries> node"
  );
  onMapOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  entriesNode.querySelector(".arrow").click();
  await onMapOiMutation;

  oiNodes = oi.querySelectorAll(".node");
  // There are now 8 nodes, the 4 original ones, and the 4 buckets.
  is(oiNodes.length, 8, "There is the expected number of nodes in the tree");
  is(oiNodes[3].textContent, `[0${ELLIPSIS}99]`);
  is(oiNodes[4].textContent, `[100${ELLIPSIS}199]`);
  is(oiNodes[5].textContent, `[200${ELLIPSIS}299]`);
  is(oiNodes[6].textContent, `[300${ELLIPSIS}330]`);
}

async function testSmallSet(oi) {
  info("Expanding the Set");
  let onMapOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  oi.querySelector(".arrow").click();
  await onMapOiMutation;

  ok(
    oi.querySelector(".arrow").classList.contains("expanded"),
    "The arrow of the node has the expected class after clicking on it"
  );

  let oiNodes = oi.querySelectorAll(".node");
  // There are 4 nodes: the root, size, entries and the proto.
  is(oiNodes.length, 4, "There is the expected number of nodes in the tree");

  info("Expanding the <entries> leaf of the map");
  const entriesNode = oiNodes[2];
  is(
    entriesNode.textContent,
    "<entries>",
    "There is the expected <entries> node"
  );
  onMapOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  entriesNode.querySelector(".arrow").click();
  await onMapOiMutation;

  oiNodes = oi.querySelectorAll(".node");
  // There are now 6 nodes, the 4 original ones, and the 2 entries.
  is(oiNodes.length, 6, "There is the expected number of nodes in the tree");

  info("Expand first entry");
  onMapOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });
  oiNodes[3].querySelector(".arrow").click();
  await onMapOiMutation;

  oiNodes = oi.querySelectorAll(".node");
  /*
   * ▼ Set (2)
   * |   size: 2
   * | ▼ <entries>
   * | | ▼ 0: {…}
   * | | | | value: 0
   * | | | ▶︎ <prototype>
   * | | ▶︎ 1: {…}
   * | ▶︎ <prototype>
   */
  is(oiNodes.length, 8, "There is the expected number of nodes in the tree");
}

async function testSet(oi) {
  info("Expanding the Set");
  let onSetOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  oi.querySelector(".arrow").click();
  await onSetOiMutation;

  ok(
    oi.querySelector(".arrow").classList.contains("expanded"),
    "The arrow of the node has the expected class after clicking on it"
  );

  let oiNodes = oi.querySelectorAll(".node");
  // There are 4 nodes: the root, size, entries and the proto.
  is(oiNodes.length, 4, "There is the expected number of nodes in the tree");

  info("Expanding the <entries> leaf of the Set");
  const entriesNode = oiNodes[2];
  is(
    entriesNode.textContent,
    "<entries>",
    "There is the expected <entries> node"
  );
  onSetOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  entriesNode.querySelector(".arrow").click();
  await onSetOiMutation;

  oiNodes = oi.querySelectorAll(".node");
  // There are now 24 nodes, the 4 original ones, and the 20 entries.
  is(oiNodes.length, 24, "There is the expected number of nodes in the tree");
}

async function testLargeSet(oi) {
  info("Expanding the large Set");
  let onSetOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  oi.querySelector(".arrow").click();
  await onSetOiMutation;

  ok(
    oi.querySelector(".arrow").classList.contains("expanded"),
    "The arrow of the node has the expected class after clicking on it"
  );

  let oiNodes = oi.querySelectorAll(".node");
  // There are 4 nodes: the root, size, entries and the proto.
  is(oiNodes.length, 4, "There is the expected number of nodes in the tree");

  info("Expanding the <entries> leaf of the Set");
  const entriesNode = oiNodes[2];
  is(
    entriesNode.textContent,
    "<entries>",
    "There is the expected <entries> node"
  );
  onSetOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  entriesNode.querySelector(".arrow").click();
  await onSetOiMutation;

  oiNodes = oi.querySelectorAll(".node");
  // There are now 7 nodes, the 4 original ones, and the 3 buckets.
  is(oiNodes.length, 7, "There is the expected number of nodes in the tree");
  is(oiNodes[3].textContent, `[0${ELLIPSIS}99]`);
  is(oiNodes[4].textContent, `[100${ELLIPSIS}199]`);
  is(oiNodes[5].textContent, `[200${ELLIPSIS}221]`);
}
