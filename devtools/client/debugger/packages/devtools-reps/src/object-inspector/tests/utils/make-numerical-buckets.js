/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { createNode, makeNumericalBuckets } = require("../../utils/node");
const gripArrayStubs = require("../../../reps/stubs/grip-array");

describe("makeNumericalBuckets", () => {
  it("handles simple numerical buckets", () => {
    const node = createNode({
      name: "root",
      contents: {
        value: gripArrayStubs.get("Array(234)"),
      },
    });
    const nodes = makeNumericalBuckets(node);

    const names = nodes.map(n => n.name);
    const paths = nodes.map(n => n.path.toString());

    expect(names).toEqual(["[0…99]", "[100…199]", "[200…233]"]);

    expect(paths).toEqual([
      "root◦[0…99]",
      "root◦[100…199]",
      "root◦[200…233]",
    ]);
  });

  // TODO: Re-enable when we have support for lonely node.
  it.skip("does not create a numerical bucket for a single node", () => {
    const node = createNode({
      name: "root",
      contents: {
        value: gripArrayStubs.get("Array(101)"),
      },
    });
    const nodes = makeNumericalBuckets(node);

    const names = nodes.map(n => n.name);
    const paths = nodes.map(n => n.path.toString());

    expect(names).toEqual(["[0…99]", "100"]);

    expect(paths).toEqual(["root◦bucket_0-99", "root◦100"]);
  });

  // TODO: Re-enable when we have support for lonely node.
  it.skip("does create a numerical bucket for two node", () => {
    const node = createNode({
      name: "root",
      contents: {
        value: gripArrayStubs.get("Array(234)"),
      },
    });
    const nodes = makeNumericalBuckets(node);

    const names = nodes.map(n => n.name);
    const paths = nodes.map(n => n.path.toString());

    expect(names).toEqual(["[0…99]", "[100…101]"]);

    expect(paths).toEqual([
      "root◦bucket_0-99",
      "root◦bucket_100-101",
    ]);
  });

  it("creates sub-buckets when needed", () => {
    const node = createNode({
      name: "root",
      contents: {
        value: gripArrayStubs.get("Array(23456)"),
      },
    });
    const nodes = makeNumericalBuckets(node);
    const names = nodes.map(n => n.name);

    expect(names).toEqual([
      "[0…999]",
      "[1000…1999]",
      "[2000…2999]",
      "[3000…3999]",
      "[4000…4999]",
      "[5000…5999]",
      "[6000…6999]",
      "[7000…7999]",
      "[8000…8999]",
      "[9000…9999]",
      "[10000…10999]",
      "[11000…11999]",
      "[12000…12999]",
      "[13000…13999]",
      "[14000…14999]",
      "[15000…15999]",
      "[16000…16999]",
      "[17000…17999]",
      "[18000…18999]",
      "[19000…19999]",
      "[20000…20999]",
      "[21000…21999]",
      "[22000…22999]",
      "[23000…23455]",
    ]);

    const firstBucketNodes = makeNumericalBuckets(nodes[0]);
    const firstBucketNames = firstBucketNodes.map(n => n.name);
    const firstBucketPaths = firstBucketNodes.map(n => n.path.toString());

    expect(firstBucketNames).toEqual([
      "[0…99]",
      "[100…199]",
      "[200…299]",
      "[300…399]",
      "[400…499]",
      "[500…599]",
      "[600…699]",
      "[700…799]",
      "[800…899]",
      "[900…999]",
    ]);
    expect(firstBucketPaths[0]).toEqual("root◦[0…999]◦[0…99]");
    expect(firstBucketPaths[firstBucketPaths.length - 1]).toEqual(
      "root◦[0…999]◦[900…999]"
    );

    const lastBucketNodes = makeNumericalBuckets(nodes[nodes.length - 1]);
    const lastBucketNames = lastBucketNodes.map(n => n.name);
    const lastBucketPaths = lastBucketNodes.map(n => n.path.toString());
    expect(lastBucketNames).toEqual([
      "[23000…23099]",
      "[23100…23199]",
      "[23200…23299]",
      "[23300…23399]",
      "[23400…23455]",
    ]);
    expect(lastBucketPaths[0]).toEqual(
      "root◦[23000…23455]◦[23000…23099]"
    );
    expect(lastBucketPaths[lastBucketPaths.length - 1]).toEqual(
      "root◦[23000…23455]◦[23400…23455]"
    );
  });
});
