/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const accessorStubs = require("devtools/client/shared/components/test/node/stubs/reps/accessor");
const performanceStubs = require("devtools/client/shared/components/test/node/stubs/object-inspector/performance");
const gripMapStubs = require("devtools/client/shared/components/test/node/stubs/reps/grip-map");
const gripArrayStubs = require("devtools/client/shared/components/test/node/stubs/reps/grip-array");
const gripEntryStubs = require("devtools/client/shared/components/test/node/stubs/reps/grip-entry");
const gripStubs = require("devtools/client/shared/components/test/node/stubs/reps/grip");

const {
  createNode,
  getChildren,
  getValue,
  makeNodesForProperties,
} = require("devtools/client/shared/components/object-inspector/utils/node");

function createRootNodeWithAccessorProperty(accessorStub) {
  const node = { name: "root", path: "rootpath" };
  const nodes = makeNodesForProperties(
    {
      ownProperties: {
        x: accessorStub,
      },
    },
    node
  );
  node.contents = nodes;

  return createNode(node);
}

describe("getChildren", () => {
  it("accessors - getter", () => {
    const children = getChildren({
      item: createRootNodeWithAccessorProperty(accessorStubs.get("getter")),
    });

    const names = children.map(n => n.name);
    const paths = children.map(n => n.path.toString());

    expect(names).toEqual(["x", "<get x()>"]);
    expect(paths).toEqual(["rootpath◦x", "rootpath◦<get x()>"]);
  });

  it("accessors - setter", () => {
    const children = getChildren({
      item: createRootNodeWithAccessorProperty(accessorStubs.get("setter")),
    });

    const names = children.map(n => n.name);
    const paths = children.map(n => n.path.toString());

    expect(names).toEqual(["x", "<set x()>"]);
    expect(paths).toEqual(["rootpath◦x", "rootpath◦<set x()>"]);
  });

  it("accessors - getter & setter", () => {
    const children = getChildren({
      item: createRootNodeWithAccessorProperty(
        accessorStubs.get("getter setter")
      ),
    });

    const names = children.map(n => n.name);
    const paths = children.map(n => n.path.toString());

    expect(names).toEqual(["x", "<get x()>", "<set x()>"]);
    expect(paths).toEqual([
      "rootpath◦x",
      "rootpath◦<get x()>",
      "rootpath◦<set x()>",
    ]);
  });

  it("returns the expected nodes for Proxy", () => {
    const proxyNode = createNode({
      name: "root",
      path: "rootpath",
      contents: { value: gripStubs.get("testProxy") },
    });
    const loadedProperties = new Map([
      [proxyNode.path, gripStubs.get("testProxySlots")],
    ]);
    const nodes = getChildren({ item: proxyNode, loadedProperties });
    const names = nodes.map(n => n.name);
    const paths = nodes.map(n => n.path.toString());

    expect(names).toEqual(["<target>", "<handler>"]);
    expect(paths).toEqual(["rootpath◦<target>", "rootpath◦<handler>"]);
  });

  it("safeGetterValues", () => {
    const stub = performanceStubs.get("timing");
    const root = createNode({
      name: "root",
      path: "rootpath",
      contents: {
        value: {
          actor: "rootactor",
          type: "object",
        },
      },
    });
    const nodes = getChildren({
      item: root,
      loadedProperties: new Map([[root.path, stub]]),
    });

    const nodeEntries = nodes.map(n => [n.name, getValue(n)]);
    const nodePaths = nodes.map(n => n.path.toString());

    const childrenEntries = [
      ["connectEnd", 1500967716401],
      ["connectStart", 1500967716401],
      ["domComplete", 1500967716719],
      ["domContentLoadedEventEnd", 1500967716715],
      ["domContentLoadedEventStart", 1500967716696],
      ["domInteractive", 1500967716552],
      ["domLoading", 1500967716426],
      ["domainLookupEnd", 1500967716401],
      ["domainLookupStart", 1500967716401],
      ["fetchStart", 1500967716401],
      ["loadEventEnd", 1500967716720],
      ["loadEventStart", 1500967716719],
      ["navigationStart", 1500967716401],
      ["redirectEnd", 0],
      ["redirectStart", 0],
      ["requestStart", 1500967716401],
      ["responseEnd", 1500967716401],
      ["responseStart", 1500967716401],
      ["secureConnectionStart", 1500967716401],
      ["unloadEventEnd", 0],
      ["unloadEventStart", 0],
      ["<prototype>", stub.prototype],
    ];
    const childrenPaths = childrenEntries.map(([name]) => `rootpath◦${name}`);

    expect(nodeEntries).toEqual(childrenEntries);
    expect(nodePaths).toEqual(childrenPaths);
  });

  it("gets data from the cache when it exists", () => {
    const mapNode = createNode({
      name: "map",
      contents: {
        value: gripMapStubs.get("testSymbolKeyedMap"),
      },
    });
    const cachedData = "";
    const children = getChildren({
      cachedNodes: new Map([[mapNode.path, cachedData]]),
      item: mapNode,
    });
    expect(children).toBe(cachedData);
  });

  it("returns an empty array if the node does not represent an object", () => {
    const node = createNode({ name: "root", contents: { value: 42 } });
    expect(
      getChildren({
        item: node,
      })
    ).toEqual([]);
  });

  it("returns an empty array if a grip node has no loaded properties", () => {
    const node = createNode({
      name: "root",
      contents: { value: gripMapStubs.get("testMaxProps") },
    });
    expect(
      getChildren({
        item: node,
      })
    ).toEqual([]);
  });

  it("adds children to cache when a grip node has loaded properties", () => {
    const stub = performanceStubs.get("timing");
    const cachedNodes = new Map();

    const rootNode = createNode({
      name: "root",
      contents: {
        value: {
          actor: "rootactor",
          type: "object",
        },
      },
    });
    const children = getChildren({
      cachedNodes,
      item: rootNode,
      loadedProperties: new Map([[rootNode.path, stub]]),
    });
    expect(cachedNodes.get(rootNode.path)).toBe(children);
  });

  it("adds children to cache when it already has some", () => {
    const cachedNodes = new Map();
    const children = [""];
    const rootNode = createNode({ name: "root", contents: children });
    getChildren({
      cachedNodes,
      item: rootNode,
    });
    expect(cachedNodes.get(rootNode.path)).toBe(children);
  });

  it("adds children to cache on a node with accessors", () => {
    const cachedNodes = new Map();
    const node = createRootNodeWithAccessorProperty(
      accessorStubs.get("getter setter")
    );

    const children = getChildren({
      cachedNodes,
      item: node,
    });
    expect(cachedNodes.get(node.path)).toBe(children);
  });

  it("adds children to cache on a map entry node", () => {
    const cachedNodes = new Map();
    const node = createNode({
      name: "root",
      contents: { value: gripEntryStubs.get("A → 0") },
    });
    const children = getChildren({
      cachedNodes,
      item: node,
    });
    expect(cachedNodes.get(node.path)).toBe(children);
  });

  it("adds children to cache on a proxy node having loaded props", () => {
    const cachedNodes = new Map();
    const node = createNode({
      name: "root",
      contents: { value: gripStubs.get("testProxy") },
    });
    const children = getChildren({
      cachedNodes,
      item: node,
      loadedProperties: new Map([[node.path, gripStubs.get("testProxySlots")]]),
    });
    expect(cachedNodes.get(node.path)).toBe(children);
  });

  it("doesn't cache children on node with buckets and no loaded props", () => {
    const cachedNodes = new Map();
    const node = createNode({
      name: "root",
      contents: { value: gripArrayStubs.get("Array(234)") },
    });
    getChildren({
      cachedNodes,
      item: node,
    });
    expect(cachedNodes.has(node.path)).toBeFalsy();
  });

  it("caches children on a node with buckets having loaded props", () => {
    const cachedNodes = new Map();
    const node = createNode({
      name: "root",
      contents: { value: gripArrayStubs.get("Array(234)") },
    });
    const children = getChildren({
      cachedNodes,
      item: node,
      loadedProperties: new Map([[node.path, { prototype: {} }]]),
    });
    expect(cachedNodes.get(node.path)).toBe(children);
  });
});
