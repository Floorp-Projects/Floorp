/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* global jest */

const { mountObjectInspector } = require("../test-utils");
const ObjectClient = require("../__mocks__/object-client");

const {
  createNode,
  makeNodesForEntries,
  makeNumericalBuckets,
} = require("../../utils/node");

const repsPath = "../../../reps";
const gripRepStubs = require(`${repsPath}/stubs/grip`);
const gripArrayRepStubs = require(`${repsPath}/stubs/grip-array`);

function mount(props, overrides = {}) {
  const client = {
    createObjectClient:
      overrides.createObjectClient || jest.fn(grip => ObjectClient(grip)),
  };

  return mountObjectInspector({
    client,
    props,
  });
}

describe("createObjectClient", () => {
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

    expect(client.createObjectClient.mock.calls[0][0]).toBe(stub);
  });

  it("is called with the expected object for entries node", () => {
    const grip = Symbol();
    const mapStubNode = createNode({ name: "map", contents: { value: grip } });
    const entriesNode = makeNodesForEntries(mapStubNode);

    const { client } = mount({
      autoExpandDepth: 1,
      roots: [entriesNode],
    });

    expect(client.createObjectClient.mock.calls[0][0]).toBe(grip);
  });

  it("is called with the expected object for bucket node", () => {
    const grip = gripArrayRepStubs.get("testMaxProps");
    const root = createNode({ name: "root", contents: { value: grip } });
    const [bucket] = makeNumericalBuckets(root);

    const { client } = mount({
      autoExpandDepth: 1,
      roots: [bucket],
    });
    expect(client.createObjectClient.mock.calls[0][0]).toBe(grip);
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

    expect(client.createObjectClient.mock.calls[0][0]).toBe(grip);
  });

  it("doesn't fail when ObjectClient doesn't have expected methods", () => {
    const stub = gripRepStubs.get("testMoreThanMaxProps");
    const root = createNode({ name: "root", contents: { value: stub } });

    // Override console.error so we don't spam test results.
    const originalConsoleError = console.error;
    console.error = () => {};

    const createObjectClient = x => ({});
    mount(
      {
        autoExpandDepth: 1,
        roots: [root],
      },
      { createObjectClient }
    );

    // rollback console.error.
    console.error = originalConsoleError;
  });
});
