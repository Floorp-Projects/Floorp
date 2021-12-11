/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check expanding/collapsing object with symbol properties in the console.
const TEST_URI = "data:text/html;charset=utf8,";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    class MyClass {
      constructor(isParent = true) {
        this.publicProperty = "public property";
        // A public property can start with a # character. Here we're
        // adding a public property that looks like an existing private property
        // to check we do render both.
        this["#privateProperty"] = {
          content: "actually this is a public property",
        };
        this[Symbol()] = "first Symbol";
        this[Symbol()] = "second Symbol";

        if (isParent) {
          this.#privateProperty = new MyClass(false);
        } else {
          this.#privateProperty = null;
        }
      }

      #privateProperty;
      #privateMethod() {
        return Math.random();
      }
      getPrivateProperty() {
        return this.#privateProperty;
      }
    }

    content.wrappedJSObject.console.log(
      "private-properties-test",
      new MyClass(true)
    );
  });

  const node = await waitFor(() => findMessage(hud, "private-properties-test"));
  const objectInspectors = [...node.querySelectorAll(".tree")];
  is(
    objectInspectors.length,
    1,
    "There is the expected number of object inspectors"
  );

  const [oi] = objectInspectors;

  info("Expanding the Object");
  const onMapOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  oi.querySelector(".arrow").click();
  await onMapOiMutation;

  ok(
    oi.querySelector(".arrow").classList.contains("expanded"),
    "The arrow of the node has the expected class after clicking on it"
  );

  const oiNodes = getObjectInspectorNodes(oi);
  // The object inspector should look like this:
  /*
   * ▼ { … }
   * |  ▶︎ "#privateProperty": Object { content: "actually this is a public property" }
   * |    publicProperty: "public property"
   * |    Symbol(): "first Symbol",
   * |    Symbol(): "second Symbol",
   * |  ▶︎ #privateProperty: Object { publicProperty: "public property", "#privateProperty": { … }, #privateProperty: null, Symbol(): "first Symbol", … }
   * |  ▶︎ <prototype>
   */
  is(oiNodes.length, 7, "There is the expected number of nodes in the tree");

  const [
    publicDisguisedAsPrivateNodeEl,
    publicNodeEl,
    firstSymbolNodeEl,
    secondSymbolNodeEl,
    privatePropertyNodeEl,
  ] = Array.from(oiNodes).slice(1);

  checkOiNodeText(
    publicDisguisedAsPrivateNodeEl,
    `"#privateProperty": Object { content: "actually this is a public property" }`,
    `"fake" private property has expected text`
  );
  checkOiNodeText(
    publicNodeEl,
    `publicProperty: "public property"`,
    "public property has expected text"
  );
  checkOiNodeText(
    firstSymbolNodeEl,
    `Symbol(): "first Symbol"`,
    "first symbol has expected text"
  );
  checkOiNodeText(
    secondSymbolNodeEl,
    `Symbol(): "second Symbol"`,
    "second symbol has expected text"
  );
  checkOiNodeText(
    privatePropertyNodeEl,
    `#privateProperty: Object { publicProperty: "public property", "#privateProperty": {…}, #privateProperty: null, Symbol(): "first Symbol", … }`,
    "private property is rendered as expected"
  );

  info("Expand public property disguised as private property");
  expandObjectInspectorNode(publicDisguisedAsPrivateNodeEl);
  const publicPropChildren = await waitFor(() => {
    const children = getObjectInspectorChildrenNodes(
      publicDisguisedAsPrivateNodeEl
    );
    if (children.length === 0) {
      return null;
    }
    return children;
  });
  ok(true, "public property was expanded");

  /*
   * ObjectInspector now should look like:
   *
   * ▼ { … }
   * |  ▼ "#privateProperty": { … }
   * |  |    content: "actually this is a public property"
   * |  |  ▶︎ <prototype>
   * |    publicProperty: "public property"
   * |    Symbol(): "first Symbol",
   * |    Symbol(): "second Symbol",
   * |  ▶︎ #privateProperty: Object { publicProperty: "public property", "#privateProperty": { … }, #privateProperty: null, Symbol(): "first Symbol", … }
   * |  ▶︎ <prototype>
   */
  checkOiNodeText(
    publicPropChildren[0],
    `content: "actually this is a public property"`,
    "public property child has expected text"
  );

  info("Expand private property");
  expandObjectInspectorNode(privatePropertyNodeEl);
  const privatePropChildren = await waitFor(() => {
    const children = getObjectInspectorChildrenNodes(privatePropertyNodeEl);
    if (children.length === 0) {
      return null;
    }
    return children;
  });
  ok(true, "private property was expanded");

  /*
   * ObjectInspector now should look like:
   *
   * ▼ { … }
   * |  ▼ "#privateProperty": { … }
   * |  |    content: "actually this is a public property"
   * |  |  ▶︎ <prototype>
   * |    publicProperty: "public property"
   * |    Symbol(): "first Symbol",
   * |    Symbol(): "second Symbol",
   * |  ▼ #privateProperty: { … }
   * |  |  ▶︎ "#privateProperty": Object { content: "actually this is a public property" }
   * |  |    publicProperty: "public property"
   * |  |    Symbol(): "first Symbol"
   * |  |    Symbol(): "second Symbol"
   * |  |    #privateProperty: null
   * |  |  ▶︎ <prototype>
   * |  ▶︎ <prototype>
   */
  checkOiNodeText(
    privatePropChildren[0],
    `"#privateProperty": Object { content: "actually this is a public property" }`,
    "child private property has expected text "
  );
  checkOiNodeText(
    privatePropChildren[1],
    `publicProperty: "public property"`,
    "public property of private property object has expected text"
  );
  checkOiNodeText(
    privatePropChildren[2],
    `Symbol(): "first Symbol"`,
    "first symbol of private property object has expected text"
  );
  checkOiNodeText(
    privatePropChildren[3],
    `Symbol(): "second Symbol"`,
    "second symbol of private property object has expected text"
  );
  checkOiNodeText(
    privatePropChildren[4],
    `#privateProperty: null`,
    "private property of private property object has expected text"
  );
  checkOiNodeText(
    privatePropChildren[5],
    `<prototype>: Object { … }`,
    "prototype of private property object has expected text"
  );

  info("Expand private property prototype");
  expandObjectInspectorNode(privatePropChildren[5]);
  const privatePropertyPrototypeChildren = await waitFor(() => {
    const children = getObjectInspectorChildrenNodes(privatePropChildren[5]);
    if (children.length === 0) {
      return null;
    }
    return children;
  });
  ok(true, "private property prototype was expanded");

  /*
   * ObjectInspector now should look like:
   *
   * ▼ { … }
   * |  ▼ "#privateProperty": { … }
   * |  |    content: "actually this is a public property"
   * |  |  ▶︎ <prototype>
   * |    publicProperty: "public property"
   * |    Symbol(): "first Symbol",
   * |    Symbol(): "second Symbol",
   * |  ▼ #privateProperty: { … }
   * |  |  ▶︎ "#privateProperty": Object { content: "actually this is a public property" }
   * |  |    publicProperty: "public property"
   * |  |    Symbol(): "first Symbol"
   * |  |    Symbol(): "second Symbol"
   * |  |    #privateProperty: null
   * |  |  ▼ <prototype>
   * |  |  |   constructor: function MyClass()
   * |  |  |   getPrivateProperty: function getPrivateProperty()
   * |  ▶︎ <prototype>
   */
  checkOiNodeText(
    privatePropertyPrototypeChildren[0],
    `constructor: function MyClass()`,
    "constructor is displayed as expected"
  );

  checkOiNodeText(
    privatePropertyPrototypeChildren[1],
    `getPrivateProperty: function getPrivateProperty()`,
    "private method is displayed as expected"
  );

  // TODO: #privateMethod should be displayed
  // checkOiNodeText(
  //   privatePropertyPrototypeChildren[2],
  //   `#privateMethod: function #privateMethod()`,
  //   "private method is displayed as expected"
  // );
});

function checkOiNodeText(oiNode, expectedText, assertionName) {
  // strip out unwanted character before the label
  is(
    oiNode.querySelector(".node").textContent.trim(),
    expectedText,
    assertionName
  );
}
