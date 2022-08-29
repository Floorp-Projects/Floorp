/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const Utils = require("devtools/client/shared/components/object-inspector/utils/index");
const {
  createNode,
  createGetterNode,
  createSetterNode,
  getChildren,
  makeNodesForEntries,
  nodeIsDefaultProperties,
} = Utils.node;

const { shouldLoadItemPrototype } = Utils.loadProperties;

const {
  createGripMapEntry,
} = require("devtools/client/shared/components/test/node/components/reps/test-helpers");
const accessorStubs = require("devtools/client/shared/components/test/node/stubs/reps/accessor");
const gripMapStubs = require("devtools/client/shared/components/test/node/stubs/reps/grip-map");
const gripArrayStubs = require("devtools/client/shared/components/test/node/stubs/reps/grip-array");
const gripStubs = require("devtools/client/shared/components/test/node/stubs/reps/grip");
const windowStubs = require("devtools/client/shared/components/test/node/stubs/reps/window");

describe("shouldLoadItemPrototype", () => {
  it("returns true for an array", () => {
    const node = createNode({
      name: "root",
      contents: {
        value: gripArrayStubs.get("testMaxProps"),
      },
    });
    expect(shouldLoadItemPrototype(node)).toBeTruthy();
  });

  it("returns false for an already loaded item", () => {
    const node = createNode({
      name: "root",
      contents: {
        value: gripArrayStubs.get("testMaxProps"),
      },
    });
    const loadedProperties = new Map([[node.path, true]]);
    expect(shouldLoadItemPrototype(node, loadedProperties)).toBeFalsy();
  });

  it("returns true for an array node with buckets", () => {
    const node = createNode({
      name: "root",
      contents: {
        value: gripArrayStubs.get("Array(234)"),
      },
    });
    expect(shouldLoadItemPrototype(node)).toBeTruthy();
  });

  it("returns false for an array bucket node", () => {
    const node = createNode({
      name: "root",
      contents: {
        value: gripArrayStubs.get("Array(234)"),
      },
    });
    const bucketNodes = getChildren({
      item: node,
      loadedProperties: new Map([[node.path, true]]),
    });

    // Make sure we do have a bucket.
    expect(bucketNodes[0].name).toBe("[0…99]");
    expect(shouldLoadItemPrototype(bucketNodes[0])).toBeFalsy();
  });

  it("returns false for an entries node", () => {
    const mapStubNode = createNode({
      name: "map",
      contents: {
        value: gripMapStubs.get("20-entries Map"),
      },
    });
    const entriesNode = makeNodesForEntries(mapStubNode);
    expect(shouldLoadItemPrototype(entriesNode)).toBeFalsy();
  });

  it("returns true for an Object", () => {
    const node = createNode({
      name: "root",
      contents: {
        value: gripStubs.get("testMaxProps"),
      },
    });
    expect(shouldLoadItemPrototype(node)).toBeTruthy();
  });

  it("returns true for a Map", () => {
    const node = createNode({
      name: "root",
      contents: {
        value: gripMapStubs.get("20-entries Map"),
      },
    });
    expect(shouldLoadItemPrototype(node)).toBeTruthy();
  });

  it("returns true for a Set", () => {
    const node = createNode({
      name: "root",
      contents: {
        value: gripArrayStubs.get("new Set([1,2,3,4])"),
      },
    });
    expect(shouldLoadItemPrototype(node)).toBeTruthy();
  });

  it("returns true for a Window", () => {
    const node = createNode({
      name: "root",
      contents: {
        value: windowStubs.get("Window")._grip,
      },
    });
    expect(shouldLoadItemPrototype(node)).toBeTruthy();
  });

  it("returns false for a <default properties> node", () => {
    const windowNode = createNode({
      name: "root",
      contents: {
        value: windowStubs.get("Window")._grip,
      },
    });
    const loadedProperties = new Map([
      [
        windowNode.path,
        {
          ownProperties: {
            foo: { value: "bar" },
            location: { value: "a" },
          },
        },
      ],
    ]);
    const [, defaultPropertiesNode] = getChildren({
      item: windowNode,
      loadedProperties,
    });
    expect(nodeIsDefaultProperties(defaultPropertiesNode)).toBe(true);
    expect(shouldLoadItemPrototype(defaultPropertiesNode)).toBeFalsy();
  });

  it("returns false for a MapEntry node", () => {
    const node = createGripMapEntry("key", "value");
    expect(shouldLoadItemPrototype(node)).toBeFalsy();
  });

  it("returns false for a Proxy node", () => {
    const node = createNode({
      name: "root",
      contents: {
        value: gripStubs.get("testProxy"),
      },
    });
    expect(shouldLoadItemPrototype(node)).toBeFalsy();
  });

  it("returns true for a Proxy target node", () => {
    const proxyNode = createNode({
      name: "root",
      contents: {
        value: gripStubs.get("testProxy"),
      },
    });
    const loadedProperties = new Map([
      [proxyNode.path, gripStubs.get("testProxySlots")],
    ]);
    const [targetNode] = getChildren({ item: proxyNode, loadedProperties });
    // Make sure we have the target node.
    expect(targetNode.name).toBe("<target>");
    expect(shouldLoadItemPrototype(targetNode)).toBeTruthy();
  });

  it("returns false for an accessor node", () => {
    const accessorNode = createNode({
      name: "root",
      contents: {
        value: accessorStubs.get("getter"),
      },
    });
    expect(shouldLoadItemPrototype(accessorNode)).toBeFalsy();
  });

  it("returns true for an accessor <get> node", () => {
    const getNode = createGetterNode({
      name: "root",
      property: accessorStubs.get("getter"),
    });
    expect(getNode.name).toBe("<get root()>");
    expect(shouldLoadItemPrototype(getNode)).toBeTruthy();
  });

  it("returns true for an accessor <set> node", () => {
    const setNode = createSetterNode({
      name: "root",
      property: accessorStubs.get("setter"),
    });
    expect(setNode.name).toBe("<set root()>");
    expect(shouldLoadItemPrototype(setNode)).toBeTruthy();
  });

  it("returns false for a primitive node", () => {
    const node = createNode({
      name: "root",
      contents: { value: 42 },
    });
    expect(shouldLoadItemPrototype(node)).toBeFalsy();
  });
});
