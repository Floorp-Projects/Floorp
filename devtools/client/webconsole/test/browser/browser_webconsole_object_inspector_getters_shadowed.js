/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check evaluating shadowed getters in the console.
const TEST_URI =
  "data:text/html;charset=utf8,<h1>Object Inspector on Getters</h1>";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    const a = {
      getter: "[A]",
      __proto__: {
        get getter() {
          return "[B]";
        },
        __proto__: {
          get getter() {
            return "[C]";
          },
        },
      },
    };
    const b = {
      value: 1,
      get getter() {
        return `[A-${this.value}]`;
      },
      __proto__: {
        value: 2,
        get getter() {
          return `[B-${this.value}]`;
        },
      },
    };
    content.wrappedJSObject.console.log("oi-test", a, b);
  });

  const node = await waitFor(() => findMessage(hud, "oi-test"));
  const [a, b] = node.querySelectorAll(".tree");

  await testObject(a, [null, "[B]", "[C]"]);
  await testObject(b, ["[A-1]", "[B-1]"]);
});

async function testObject(oi, values) {
  let node = oi.querySelector(".tree-node");
  for (const value of values) {
    await expand(node);
    if (value != null) {
      const getter = findObjectInspectorNodeChild(node, "getter");
      await invokeGetter(getter);
      ok(
        getter.textContent.includes(`getter: "${value}"`),
        `Getter now has the expected "${value}" content`
      );
    }
    node = findObjectInspectorNodeChild(node, "<prototype>");
  }
}

function expand(node) {
  expandObjectInspectorNode(node);
  return waitFor(() => getObjectInspectorChildrenNodes(node).length > 0);
}

function invokeGetter(node) {
  getObjectInspectorInvokeGetterButton(node).click();
  return waitFor(() => !getObjectInspectorInvokeGetterButton(node));
}

function findObjectInspectorNodeChild(node, nodeLabel) {
  return getObjectInspectorChildrenNodes(node).find(child => {
    const label = child.querySelector(".object-label");
    return label && label.textContent === nodeLabel;
  });
}
