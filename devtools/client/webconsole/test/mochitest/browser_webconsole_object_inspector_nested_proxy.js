/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check evaluating and expanding getters in the console.
const TEST_URI = "data:text/html;charset=utf8,"
 + "<h1>Object Inspector on deeply nested proxies</h1>";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    let proxy = new Proxy({}, {});
    for (let i = 0; i < 1e5; ++i) {
      proxy = new Proxy(proxy, proxy);
    }
    content.wrappedJSObject.console.log("oi-test", proxy);
  });

  const node = await waitFor(() => findMessage(hud, "oi-test"));
  const oi = node.querySelector(".tree");
  const [proxyNode] = getObjectInspectorNodes(oi);

  expandObjectInspectorNode(proxyNode);
  await waitFor(() => getObjectInspectorNodes(oi).length > 1);
  checkChildren(proxyNode, [`<target>`, `<handler>`]);

  const targetNode = findObjectInspectorNode(oi, "<target>");
  expandObjectInspectorNode(targetNode);
  await waitFor(() => getObjectInspectorChildrenNodes(targetNode).length > 0);
  checkChildren(targetNode, [`<target>`, `<handler>`]);

  const handlerNode = findObjectInspectorNode(oi, "<handler>");
  expandObjectInspectorNode(handlerNode);
  await waitFor(() => getObjectInspectorChildrenNodes(handlerNode).length > 0);
  checkChildren(handlerNode, [`<target>`, `<handler>`]);
});

function checkChildren(node, expectedChildren) {
  const children = getObjectInspectorChildrenNodes(node);
  is(children.length, expectedChildren.length,
    "There is the expected number of children");
  children.forEach((child, index) => {
    ok(child.textContent.includes(expectedChildren[index]),
      `Expected "${expectedChildren[index]}" child`);
  });
}
