/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check evaluating getters on prototype nodes in the console.
const TEST_URI =
  "data:text/html;charset=utf8,<!DOCTYPE html><h1>Object Inspector on Getters</h1>";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    class A {
      constructor() {
        this.myValue = "foo";
      }
      get value() {
        return `A-value:${this.myValue}`;
      }
    }

    class B extends A {
      get value() {
        return `B-value:${this.myValue}`;
      }
    }

    class C extends A {
      constructor() {
        super();
        this.myValue = "bar";
      }
    }

    const a = new A();
    const b = new B();
    const c = new C();

    const d = new C();
    d.myValue = "d";

    content.wrappedJSObject.console.log("oi-test", a, b, c, d);
  });

  const node = await waitFor(() => findConsoleAPIMessage(hud, "oi-test"));
  const [a, b, c, d] = node.querySelectorAll(".tree");

  await testObject(a, {
    myValue: `myValue: "foo"`,
    value: `value: "A-value:foo"`,
  });

  await testObject(b, {
    myValue: `myValue: "foo"`,
    value: `value: "B-value:foo"`,
  });

  await testObject(c, {
    myValue: `myValue: "bar"`,
    value: `value: "A-value:bar"`,
  });

  await testObject(d, {
    myValue: `myValue: "d"`,
    value: `value: "A-value:d"`,
  });
});

async function testObject(oi, { myValue, value }) {
  expandObjectInspectorNode(oi.querySelector(".tree-node"));
  const prototypeNode = await waitFor(() =>
    findObjectInspectorNode(oi, "<prototype>")
  );
  let valueGetterNode = await getValueNode(prototypeNode);

  ok(
    findObjectInspectorNode(oi, "myValue").textContent.includes(myValue),
    `myValue has expected "${myValue}" content`
  );

  getObjectInspectorInvokeGetterButton(valueGetterNode).click();

  await waitFor(
    () =>
      !getObjectInspectorInvokeGetterButton(
        findObjectInspectorNode(oi, "value")
      )
  );
  valueGetterNode = findObjectInspectorNode(oi, "value");
  ok(
    valueGetterNode.textContent.includes(value),
    `Getter now has the expected "${value}" content`
  );
}

async function getValueNode(prototypeNode) {
  expandObjectInspectorNode(prototypeNode);

  await waitFor(() => !!getObjectInspectorChildrenNodes(prototypeNode).length);

  const children = getObjectInspectorChildrenNodes(prototypeNode);
  const valueNode = children.find(
    child => child.querySelector(".object-label").textContent === "value"
  );

  if (valueNode) {
    return valueNode;
  }

  const childPrototypeNode = children.find(
    child => child.querySelector(".object-label").textContent === "<prototype>"
  );
  if (!childPrototypeNode) {
    return null;
  }

  return getValueNode(childPrototypeNode);
}
