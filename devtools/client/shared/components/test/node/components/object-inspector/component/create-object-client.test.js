/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* global jest */

const {
  mountObjectInspector,
} = require("devtools/client/shared/components/test/node/components/object-inspector/test-utils");
const ObjectFront = require("devtools/client/shared/components/test/node/__mocks__/object-front");

const {
  createNode,
  makeNodesForEntries,
  makeNumericalBuckets,
} = require("devtools/client/shared/components/object-inspector/utils/node");

const gripRepStubs = require(`devtools/client/shared/components/test/node/stubs/reps/grip`);
const gripArrayRepStubs = require(`devtools/client/shared/components/test/node/stubs/reps/grip-array`);

function mount(props, overrides = {}) {
  const client = {
    createObjectFront:
      overrides.createObjectFront || jest.fn(grip => ObjectFront(grip)),
    getFrontByID: _id => null,
  };

  return mountObjectInspector({
    client,
    props,
  });
}

describe("createObjectFront", () => {
  it("is called with the expected object for regular node", () => {
    const stub = gripRepStubs.get("testMoreThanMaxProps");
    const { client } = mount({
      autoExpandDepth: 1,
      roots: [
        {
          path: "root",
          contents: {
            value: stub,
          },
        },
      ],
    });

    expect(client.createObjectFront.mock.calls[0][0]).toBe(stub);
  });

  it("is called with the expected object for entries node", () => {
    const grip = Symbol();
    const mapStubNode = createNode({
      name: "map",
      contents: { value: grip },
    });
    const entriesNode = makeNodesForEntries(mapStubNode);

    const { client } = mount({
      autoExpandDepth: 1,
      roots: [entriesNode],
    });

    expect(client.createObjectFront.mock.calls[0][0]).toBe(grip);
  });

  it("is called with the expected object for bucket node", () => {
    const grip = gripArrayRepStubs.get("testMaxProps");
    const root = createNode({ name: "root", contents: { value: grip } });
    const [bucket] = makeNumericalBuckets(root);

    const { client } = mount({
      autoExpandDepth: 1,
      roots: [bucket],
    });
    expect(client.createObjectFront.mock.calls[0][0]).toBe(grip);
  });

  it("is called with the expected object for sub-bucket node", () => {
    const grip = gripArrayRepStubs.get("testMaxProps");
    const root = createNode({ name: "root", contents: { value: grip } });
    const [bucket] = makeNumericalBuckets(root);
    const [subBucket] = makeNumericalBuckets(bucket);

    const { client } = mount({
      autoExpandDepth: 1,
      roots: [subBucket],
    });

    expect(client.createObjectFront.mock.calls[0][0]).toBe(grip);
  });

  it("doesn't fail when ObjectFront doesn't have expected methods", () => {
    const stub = gripRepStubs.get("testMoreThanMaxProps");
    const root = createNode({ name: "root", contents: { value: stub } });

    // Override console.error so we don't spam test results.
    const originalConsoleError = console.error;
    console.error = () => {};

    const createObjectFront = x => ({});
    mount(
      {
        autoExpandDepth: 1,
        roots: [root],
      },
      { createObjectFront }
    );

    // rollback console.error.
    console.error = originalConsoleError;
  });
});
