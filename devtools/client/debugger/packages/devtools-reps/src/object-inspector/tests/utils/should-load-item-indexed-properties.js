/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const Utils = require("../../utils");
const {
  createNode,
  createGetterNode,
  createSetterNode,
  getChildren,
  makeNodesForEntries,
  nodeIsDefaultProperties,
} = Utils.node;

const { shouldLoadItemIndexedProperties } = Utils.loadProperties;

const GripMapEntryRep = require("../../../reps/grip-map-entry");
const accessorStubs = require("../../../reps/stubs/accessor");
const gripMapStubs = require("../../../reps/stubs/grip-map");
const gripArrayStubs = require("../../../reps/stubs/grip-array");
const gripStubs = require("../../../reps/stubs/grip");
const windowStubs = require("../../../reps/stubs/window");

describe("shouldLoadItemIndexedProperties", () => {
  it("returns true for an array", () => {
    const node = createNode({
      name: "root",
      contents: {
        value: gripArrayStubs.get("testMaxProps"),
      },
    });
    expect(shouldLoadItemIndexedProperties(node)).toBeTruthy();
  });

  it("returns false for an already loaded item", () => {
    const node = createNode({
      name: "root",
      contents: {
        value: gripArrayStubs.get("testMaxProps"),
      },
    });
    const loadedProperties = new Map([[node.path, true]]);
    expect(shouldLoadItemIndexedProperties(node, loadedProperties)).toBeFalsy();
  });

  it("returns false for an array node with buckets", () => {
    const node = createNode({
      name: "root",
      contents: {
        value: gripArrayStubs.get("Array(234)"),
      },
    });
    expect(shouldLoadItemIndexedProperties(node)).toBeFalsy();
  });

  it("returns true for an array bucket node", () => {
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
    expect(shouldLoadItemIndexedProperties(bucketNodes[0])).toBeTruthy();
  });

  it("returns false for an array bucket node with sub-buckets", () => {
    const node = createNode({
      name: "root",
      contents: {
        value: gripArrayStubs.get("Array(23456)"),
      },
    });
    const bucketNodes = getChildren({
      item: node,
      loadedProperties: new Map([[node.path, true]]),
    });

    // Make sure we do have a bucket.
    expect(bucketNodes[0].name).toBe("[0…999]");
    expect(shouldLoadItemIndexedProperties(bucketNodes[0])).toBeFalsy();
  });

  it("returns true for an array sub-bucket node", () => {
    const node = createNode({
      name: "root",
      contents: {
        value: gripArrayStubs.get("Array(23456)"),
      },
    });
    const bucketNodes = getChildren({
      item: node,
      loadedProperties: new Map([[node.path, true]]),
    });
    // Make sure we do have a bucket.
    expect(bucketNodes[0].name).toBe("[0…999]");

    // Get the sub-buckets
    const subBucketNodes = getChildren({
      item: bucketNodes[0],
      loadedProperties: new Map([[bucketNodes[0].path, true]]),
    });
    // Make sure we do have a bucket.
    expect(subBucketNodes[0].name).toBe("[0…99]");
    expect(shouldLoadItemIndexedProperties(subBucketNodes[0])).toBeTruthy();
  });

  it("returns false for an entries node", () => {
    const mapStubNode = createNode({
      name: "map",
      contents: {
        value: gripMapStubs.get("20-entries Map"),
      },
    });
    const entriesNode = makeNodesForEntries(mapStubNode);
    expect(shouldLoadItemIndexedProperties(entriesNode)).toBeFalsy();
  });

  it("returns true for an Object", () => {
    const node = createNode({
      name: "root",
      contents: {
        value: gripStubs.get("testMaxProps"),
      },
    });
    expect(shouldLoadItemIndexedProperties(node)).toBeTruthy();
  });

  it("returns true for a Map", () => {
    const node = createNode({
      name: "root",
      contents: {
        value: gripMapStubs.get("20-entries Map"),
      },
    });
    expect(shouldLoadItemIndexedProperties(node)).toBeTruthy();
  });

  it("returns true for a Set", () => {
    const node = createNode({
      name: "root",
      contents: {
        value: gripArrayStubs.get("new Set([1,2,3,4])"),
      },
    });
    expect(shouldLoadItemIndexedProperties(node)).toBeTruthy();
  });

  it("returns true for a Window", () => {
    const node = createNode({
      name: "root",
      contents: {
        value: windowStubs.get("Window"),
      },
    });
    expect(shouldLoadItemIndexedProperties(node)).toBeTruthy();
  });

  it("returns false for a <default properties> node", () => {
    const windowNode = createNode({
      name: "root",
      contents: {
        value: windowStubs.get("Window"),
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
    expect(shouldLoadItemIndexedProperties(defaultPropertiesNode)).toBeFalsy();
  });

  it("returns false for a MapEntry node", () => {
    const node = GripMapEntryRep.createGripMapEntry("key", "value");
    expect(shouldLoadItemIndexedProperties(node)).toBeFalsy();
  });

  it("returns false for a Proxy node", () => {
    const node = createNode({
      name: "root",
      contents: {
        value: gripStubs.get("testProxy"),
      },
    });
    expect(shouldLoadItemIndexedProperties(node)).toBeFalsy();
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
    expect(shouldLoadItemIndexedProperties(targetNode)).toBeTruthy();
  });

  it("returns false for an accessor node", () => {
    const accessorNode = createNode({
      name: "root",
      contents: {
        value: accessorStubs.get("getter"),
      },
    });
    expect(shouldLoadItemIndexedProperties(accessorNode)).toBeFalsy();
  });

  it("returns true for an accessor <get> node", () => {
    const getNode = createGetterNode({
      name: "root",
      property: accessorStubs.get("getter"),
    });
    expect(getNode.name).toBe("<get root()>");
    expect(shouldLoadItemIndexedProperties(getNode)).toBeTruthy();
  });

  it("returns true for an accessor <set> node", () => {
    const setNode = createSetterNode({
      name: "root",
      property: accessorStubs.get("setter"),
    });
    expect(setNode.name).toBe("<set root()>");
    expect(shouldLoadItemIndexedProperties(setNode)).toBeTruthy();
  });

  it("returns false for a primitive node", () => {
    const node = createNode({
      name: "root",
      contents: { value: 42 },
    });
    expect(shouldLoadItemIndexedProperties(node)).toBeFalsy();
  });
});
