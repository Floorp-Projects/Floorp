/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const {
  createNode,
  getClosestGripNode,
  makeNodesForEntries,
  makeNumericalBuckets,
} = require("../../utils/node");

const repsPath = "../../../reps";
const gripRepStubs = require(`${repsPath}/stubs/grip`);
const gripArrayRepStubs = require(`${repsPath}/stubs/grip-array`);

describe("getClosestGripNode", () => {
  it("returns grip node itself", () => {
    const stub = gripRepStubs.get("testMoreThanMaxProps");
    const node = createNode({ name: "root", contents: { value: stub } });
    expect(getClosestGripNode(node)).toBe(node);
  });

  it("returns the expected node for entries node", () => {
    const mapStubNode = createNode({ name: "map", contents: { value: {} } });
    const entriesNode = makeNodesForEntries(mapStubNode);
    expect(getClosestGripNode(entriesNode)).toBe(mapStubNode);
  });

  it("returns the expected node for bucket node", () => {
    const grip = gripArrayRepStubs.get("testMaxProps");
    const root = createNode({ name: "root", contents: { value: grip } });
    const [bucket] = makeNumericalBuckets(root);
    expect(getClosestGripNode(bucket)).toBe(root);
  });

  it("returns the expected node for sub-bucket node", () => {
    const grip = gripArrayRepStubs.get("testMaxProps");
    const root = createNode({ name: "root", contents: { value: grip } });
    const [bucket] = makeNumericalBuckets(root);
    const [subBucket] = makeNumericalBuckets(bucket);
    expect(getClosestGripNode(subBucket)).toBe(root);
  });

  it("returns the expected node for deep sub-bucket node", () => {
    const grip = gripArrayRepStubs.get("testMaxProps");
    const root = createNode({ name: "root", contents: { value: grip } });
    let [bucket] = makeNumericalBuckets(root);
    for (let i = 0; i < 10; i++) {
      bucket = makeNumericalBuckets({ ...bucket })[0];
    }
    expect(getClosestGripNode(bucket)).toBe(root);
  });
});
