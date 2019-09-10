/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const {
  createNode,
  makeNodesForProperties,
  nodeIsDefaultProperties,
  nodeIsEntries,
  nodeIsMapEntry,
  nodeIsPrototype,
} = require("../../utils/node");
const gripArrayStubs = require("../../../reps/stubs/grip-array");

const root = {
  path: "root",
  contents: {
    value: gripArrayStubs.get("testBasic"),
  },
};

const objProperties = {
  ownProperties: {
    "0": {
      value: {},
    },
    "2": {},
    length: {
      value: 3,
    },
  },
  prototype: {
    type: "object",
    actor: "server2.conn1.child1/pausedobj618",
    class: "bla",
  },
};

describe("makeNodesForProperties", () => {
  it("kitchen sink", () => {
    const nodes = makeNodesForProperties(objProperties, root);

    const names = nodes.map(n => n.name);
    expect(names).toEqual(["0", "length", "<prototype>"]);

    const paths = nodes.map(n => n.path.toString());
    expect(paths).toEqual(["root◦0", "root◦length", "root◦<prototype>"]);
  });

  it("includes getters and setters", () => {
    const nodes = makeNodesForProperties(
      {
        ownProperties: {
          foo: { value: "foo" },
          bar: {
            get: {
              type: "object",
            },
            set: {
              type: "undefined",
            },
          },
          baz: {
            get: {
              type: "undefined",
            },
            set: {
              type: "object",
            },
          },
        },
        prototype: {
          class: "bla",
        },
      },
      root
    );

    const names = nodes.map(n => n.name);
    const paths = nodes.map(n => n.path.toString());

    expect(names).toEqual([
      "bar",
      "baz",
      "foo",
      "<get bar()>",
      "<set baz()>",
      "<prototype>",
    ]);

    expect(paths).toEqual([
      "root◦bar",
      "root◦baz",
      "root◦foo",
      "root◦<get bar()>",
      "root◦<set baz()>",
      "root◦<prototype>",
    ]);
  });

  it("does not include unrelevant properties", () => {
    const nodes = makeNodesForProperties(
      {
        ownProperties: {
          foo: undefined,
          bar: null,
          baz: {},
        },
      },
      root
    );

    const names = nodes.map(n => n.name);
    const paths = nodes.map(n => n.path);

    expect(names).toEqual([]);
    expect(paths).toEqual([]);
  });

  it("sorts keys", () => {
    const nodes = makeNodesForProperties(
      {
        ownProperties: {
          bar: { value: {} },
          1: { value: {} },
          11: { value: {} },
          2: { value: {} },
          _bar: { value: {} },
        },
        prototype: {
          class: "bla",
        },
      },
      root
    );

    const names = nodes.map(n => n.name);
    const paths = nodes.map(n => n.path.toString());

    expect(names).toEqual(["1", "2", "11", "_bar", "bar", "<prototype>"]);
    expect(paths).toEqual([
      "root◦1",
      "root◦2",
      "root◦11",
      "root◦_bar",
      "root◦bar",
      "root◦<prototype>",
    ]);
  });

  it("prototype is included", () => {
    const nodes = makeNodesForProperties(
      {
        ownProperties: {
          bar: { value: {} },
        },
        prototype: { value: {}, class: "bla" },
      },
      root
    );

    const names = nodes.map(n => n.name);
    const paths = nodes.map(n => n.path.toString());

    expect(names).toEqual(["bar", "<prototype>"]);
    expect(paths).toEqual(["root◦bar", "root◦<prototype>"]);

    expect(nodeIsPrototype(nodes[1])).toBe(true);
  });

  it("window object", () => {
    const nodes = makeNodesForProperties(
      {
        ownProperties: {
          bar: { value: {} },
          location: { value: {} },
        },
        class: "Window",
      },
      {
        path: "root",
        contents: { value: { class: "Window" } },
      }
    );

    const names = nodes.map(n => n.name);
    const paths = nodes.map(n => n.path.toString());

    expect(names).toEqual(["bar", "<default properties>"]);
    expect(paths).toEqual(["root◦bar", "root◦<default properties>"]);

    expect(nodeIsDefaultProperties(nodes[1])).toBe(true);
  });

  it("object with entries", () => {
    const gripMapStubs = require("../../../reps/stubs/grip-map");

    const mapNode = createNode({
      name: "map",
      path: "root",
      contents: {
        value: gripMapStubs.get("testSymbolKeyedMap"),
      },
    });

    const nodes = makeNodesForProperties(
      {
        ownProperties: {
          size: { value: 1 },
          custom: { value: "customValue" },
        },
      },
      mapNode
    );

    const names = nodes.map(n => n.name);
    const paths = nodes.map(n => n.path.toString());

    expect(names).toEqual(["custom", "size", "<entries>"]);
    expect(paths).toEqual(["root◦custom", "root◦size", "root◦<entries>"]);

    const entriesNode = nodes[2];
    expect(nodeIsEntries(entriesNode)).toBe(true);

    const children = entriesNode.contents;

    // There are 2 entries in the map.
    expect(children).toHaveLength(2);
    // And the 2 nodes created are typed as map entries.
    expect(children.every(child => nodeIsMapEntry(child))).toBe(true);

    const childrenNames = children.map(n => n.name);
    const childrenPaths = children.map(n => n.path.toString());
    expect(childrenNames).toEqual([0, 1]);
    expect(childrenPaths).toEqual(["root◦<entries>◦0", "root◦<entries>◦1"]);
  });

  it("quotes property names", () => {
    const nodes = makeNodesForProperties(
      {
        ownProperties: {
          // Numbers are ok.
          332217: { value: {} },
          "needs-quotes": { value: {} },
          unquoted: { value: {} },
          "": { value: {} },
        },
        prototype: {
          class: "WindowPrototype",
        },
      },
      root
    );

    const names = nodes.map(n => n.name);
    const paths = nodes.map(n => n.path.toString());

    expect(names).toEqual([
      '""',
      "332217",
      '"needs-quotes"',
      "unquoted",
      "<prototype>",
    ]);
    expect(paths).toEqual([
      'root◦""',
      "root◦332217",
      'root◦"needs-quotes"',
      "root◦unquoted",
      "root◦<prototype>",
    ]);
  });
});
