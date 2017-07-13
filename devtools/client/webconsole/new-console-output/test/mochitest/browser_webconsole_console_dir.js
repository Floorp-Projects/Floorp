/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check console.dir() calls.
const TEST_URI = "data:text/html;charset=utf8,<h1>test console.dir</h1>";

add_task(async function () {
  let toolbox = await openNewTabAndToolbox(TEST_URI, "webconsole");
  let hud = toolbox.getCurrentPanel().hud;

  const store = hud.ui.newConsoleOutput.getStore();
  // Adding logging each time the store is modified in order to check
  // the store state in case of failure.
  store.subscribe(() => {
    const messages = store.getState().messages.messagesById
      .reduce(function (res, {id, type, parameters, messageText}) {
        res.push({id, type, parameters, messageText});
        return res;
      }, []);
    info("messages : " + JSON.stringify(messages));
  });

  info("console.dir on an array");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function () {
    content.wrappedJSObject.console.dir(
      [1, 2, {a: "a", b: "b"}],
    );
  });

  let dirMessageNode = await waitFor(() =>
    findConsoleDir(hud.ui.experimentalOutputNode, 0));
  let objectInspectors = [...dirMessageNode.querySelectorAll(".tree")];
  is(objectInspectors.length, 1, "There is the expected number of object inspectors");

  const [arrayOi] = objectInspectors;
  let arrayOiNodes = arrayOi.querySelectorAll(".node");

  // The tree can be collapsed since the properties are fetched asynchronously.
  if (arrayOiNodes.length === 1) {
    // If this is the case, we wait for the properties to be fetched and displayed.
    await waitForNodeMutation(arrayOi, {
      childList: true
    });
    arrayOiNodes = arrayOi.querySelectorAll(".node");
  }

  // There are 6 nodes: the root, 1, 2, {a: "a", b: "b"}, length and the proto.
  is(arrayOiNodes.length, 6, "There is the expected number of nodes in the tree");
  let propertiesNodes = [...arrayOi.querySelectorAll(".object-label")]
    .map(el => el.textContent);
  const arrayPropertiesNames = ["0", "1", "2", "length", "__proto__"];
  is(JSON.stringify(propertiesNodes), JSON.stringify(arrayPropertiesNames));

  info("console.dir on a long object");
  const obj = Array.from({length: 100}).reduce((res, _, i) => {
    res["item-" + (i + 1).toString().padStart(3, "0")] = i + 1;
    return res;
  }, {});

  await ContentTask.spawn(gBrowser.selectedBrowser, obj, function (data) {
    content.wrappedJSObject.console.dir(data);
  });

  dirMessageNode = await waitFor(() => findConsoleDir(hud.ui.experimentalOutputNode, 1));
  objectInspectors = [...dirMessageNode.querySelectorAll(".tree")];
  is(objectInspectors.length, 1, "There is the expected number of object inspectors");

  const [objectOi] = objectInspectors;
  let objectOiNodes = objectOi.querySelectorAll(".node");

  // The tree can be collapsed since the properties are fetched asynchronously.
  if (objectOiNodes.length === 1) {
    // If this is the case, we wait for the properties to be fetched and displayed.
    await waitForNodeMutation(objectOi, {
      childList: true
    });
    objectOiNodes = objectOi.querySelectorAll(".node");
  }

  // There are 102 nodes: the root, 100 "item-N" properties, and the proto.
  is(objectOiNodes.length, 102, "There is the expected number of nodes in the tree");
  const objectPropertiesNames = Object.getOwnPropertyNames(obj).map(name => `"${name}"`);
  objectPropertiesNames.push("__proto__");
  propertiesNodes = [...objectOi.querySelectorAll(".object-label")]
    .map(el => el.textContent);
  is(JSON.stringify(propertiesNodes), JSON.stringify(objectPropertiesNames));
});

function findConsoleDir(node, index) {
  return node.querySelectorAll(".dir.message")[index];
}

function waitForNodeMutation(node, observeConfig = {}) {
  return new Promise(resolve => {
    const observer = new MutationObserver(mutations => {
      resolve(mutations);
      observer.disconnect();
    });
    observer.observe(node, observeConfig);
  });
}
