/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { createNode, NODE_TYPES } = require("../../utils/node");

describe("createNode", () => {
  it("returns null when contents is undefined", () => {
    expect(createNode({ name: "name" })).toBeNull();
  });

  it("does not return null when contents is null", () => {
    expect(
      createNode({
        name: "name",
        path: "path",
        contents: null,
      })
    ).not.toBe(null);
  });

  it("returns the expected object when parent is undefined", () => {
    const node = createNode({
      name: "name",
      path: "path",
      contents: "contents",
    });
    expect(node).toEqual({
      name: "name",
      path: node.path,
      contents: "contents",
      type: NODE_TYPES.GRIP,
    });
  });

  it("returns the expected object when parent is not null", () => {
    const root = createNode({ name: "name", contents: null });
    const child = createNode({
      parent: root,
      name: "name",
      path: "path",
      contents: "contents",
    });
    expect(child.parent).toEqual(root);
  });

  it("returns the expected object when type is not undefined", () => {
    const root = createNode({ name: "name", contents: null });
    const child = createNode({
      parent: root,
      name: "name",
      path: "path",
      contents: "contents",
      type: NODE_TYPES.BUCKET,
    });

    expect(child.type).toEqual(NODE_TYPES.BUCKET);
  });

  it("uses the name property for the path when path is not provided", () => {
    expect(
      createNode({ name: "name", contents: "contents" }).path.toString()
    ).toBe("name");
  });

  it("wraps the path in a Symbol when provided", () => {
    expect(
      createNode({
        name: "name",
        path: "path",
        contents: "contents",
      }).path.toString()
    ).toBe("path");
  });

  it("uses parent path to compute its path", () => {
    const root = createNode({ name: "root", contents: null });
    expect(
      createNode({
        parent: root,
        name: "name",
        path: "path",
        contents: "contents",
      }).path.toString()
    ).toBe("root◦path");
  });
});
