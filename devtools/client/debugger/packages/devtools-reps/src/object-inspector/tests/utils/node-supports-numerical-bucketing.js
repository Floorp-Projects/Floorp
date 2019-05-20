/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const {
  createNode,
  makeNodesForEntries,
  nodeSupportsNumericalBucketing,
} = require("../../utils/node");

const createRootNode = stub =>
  createNode({
    name: "root",
    contents: { value: stub },
  });

const gripArrayStubs = require("../../../reps/stubs/grip-array");
const gripMapStubs = require("../../../reps/stubs/grip-map");

describe("nodeSupportsNumericalBucketing", () => {
  it("returns true for Arrays", () => {
    expect(
      nodeSupportsNumericalBucketing(
        createRootNode(gripArrayStubs.get("testBasic"))
      )
    ).toBe(true);
  });

  it("returns true for NodeMap", () => {
    expect(
      nodeSupportsNumericalBucketing(
        createRootNode(gripArrayStubs.get("testNamedNodeMap"))
      )
    ).toBe(true);
  });

  it("returns true for NodeList", () => {
    expect(
      nodeSupportsNumericalBucketing(
        createRootNode(gripArrayStubs.get("testNodeList"))
      )
    ).toBe(true);
  });

  it("returns true for DocumentFragment", () => {
    expect(
      nodeSupportsNumericalBucketing(
        createRootNode(gripArrayStubs.get("testDocumentFragment"))
      )
    ).toBe(true);
  });

  it("returns true for <entries> node", () => {
    expect(
      nodeSupportsNumericalBucketing(
        makeNodesForEntries(
          createRootNode(gripMapStubs.get("testSymbolKeyedMap"))
        )
      )
    ).toBe(true);
  });

  it("returns true for buckets node", () => {
    expect(
      nodeSupportsNumericalBucketing(
        makeNodesForEntries(
          createRootNode(gripMapStubs.get("testSymbolKeyedMap"))
        )
      )
    ).toBe(true);
  });
});
