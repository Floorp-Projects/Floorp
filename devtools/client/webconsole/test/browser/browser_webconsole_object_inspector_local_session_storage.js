/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check expanding/collapsing local and session storage in the console.
const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-local-session-storage.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const messages = await logMessages(hud);
  const objectInspectors = messages.map(node => node.querySelector(".tree"));

  is(
    objectInspectors.length,
    2,
    "There is the expected number of object inspectors"
  );

  await checkValues(objectInspectors[0], "localStorage");
  await checkValues(objectInspectors[1], "sessionStorage");
});

async function logMessages(hud) {
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.console.log("localStorage", content.localStorage);
  });
  const localStorageMsg = await waitFor(() => findMessage(hud, "localStorage"));

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.console.log("sessionStorage", content.sessionStorage);
  });
  const sessionStorageMsg = await waitFor(() =>
    findMessage(hud, "sessionStorage")
  );

  return [localStorageMsg, sessionStorageMsg];
}

async function checkValues(oi, storageType) {
  info(`Expanding the ${storageType} object`);
  let onMapOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  oi.querySelector(".arrow").click();
  await onMapOiMutation;

  ok(
    oi.querySelector(".arrow").classList.contains("expanded"),
    "The arrow of the node has the expected class after clicking on it"
  );

  let nodes = oi.querySelectorAll(".node");
  // There are 4 nodes: the root, size, entries and the proto.
  is(nodes.length, 5, "There is the expected number of nodes in the tree");

  info("Expanding the <entries> leaf of the map");
  const entriesNode = nodes[3];
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

  nodes = oi.querySelectorAll(".node");
  // There are now 7 nodes, the 5 original ones, and the 2 entries.
  is(nodes.length, 7, "There is the expected number of nodes in the tree");

  const title = nodes[0].querySelector(".objectTitle").textContent;
  const name1 = nodes[1].querySelector(".object-label").textContent;
  const value1 = nodes[1].querySelector(".objectBox").textContent;

  const length = [...nodes[2].querySelectorAll(".object-label,.objectBox")].map(
    node => node.textContent
  );
  const key2 = [
    ...nodes[4].querySelectorAll(".object-label,.nodeName,.objectBox-string"),
  ].map(node => node.textContent);
  const key = [
    ...nodes[5].querySelectorAll(".object-label,.nodeName,.objectBox-string"),
  ].map(node => node.textContent);

  is(title, "Storage", `${storageType} object has the expected title`);
  is(length[0], "length", `${storageType} length property name is correct`);
  is(length[1], "2", `${storageType} length property value is correct`);
  is(key2[0], "0", `1st entry of ${storageType} entry has the correct index`);
  is(key2[1], "key2", `1st entry of ${storageType} entry has the correct key`);

  const firstValue = storageType === "localStorage" ? `"value2"` : `"value4"`;
  is(name1, "key2", "Name of short descriptor is correct");
  is(value1, firstValue, "Value of short descriptor is correct");
  is(
    key2[2],
    firstValue,
    `1st entry of ${storageType} entry has the correct value`
  );
  is(key[0], "1", `2nd entry of ${storageType} entry has the correct index`);
  is(key[1], "key", `2nd entry of ${storageType} entry has the correct key`);

  const secondValue = storageType === "localStorage" ? `"value1"` : `"value3"`;
  is(
    key[2],
    secondValue,
    `2nd entry of ${storageType} entry has the correct value`
  );
}
