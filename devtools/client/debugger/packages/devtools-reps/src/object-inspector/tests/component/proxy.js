/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* global jest */
const { mountObjectInspector } = require("../test-utils");

const { MODE } = require("../../../reps/constants");
const stub = require("../../../reps/stubs/grip").get("testProxy");
const { formatObjectInspector } = require("../test-utils");

const ObjectClient = require("../__mocks__/object-client");
function generateDefaults(overrides) {
  return {
    roots: [
      {
        path: "root",
        contents: { value: stub }
      }
    ],
    autoExpandDepth: 1,
    mode: MODE.LONG,
    ...overrides
  };
}

function getEnumPropertiesMock() {
  return jest.fn(() => ({
    iterator: {
      slice: () => ({})
    }
  }));
}

function mount(props, { initialState } = {}) {
  const enumProperties = getEnumPropertiesMock();

  const client = {
    createObjectClient: grip => ObjectClient(grip, { enumProperties })
  };

  const obj = mountObjectInspector({
    client,
    props: generateDefaults(props),
    initialState
  });

  return { ...obj, enumProperties };
}

describe("ObjectInspector - Proxy", () => {
  it("renders Proxy as expected", () => {
    const { wrapper, enumProperties } = mount(
      {},
      {
        initialState: {
          objectInspector: {
            // Have the prototype already loaded so the component does not call
            // enumProperties for the root's properties.
            loadedProperties: new Map([["root", { prototype: {} }]]),
            evaluations: new Map()
          }
        }
      }
    );

    expect(formatObjectInspector(wrapper)).toMatchSnapshot();

    // enumProperties should not have been called.
    expect(enumProperties.mock.calls).toHaveLength(0);
  });

  it("calls enumProperties on <target> and <handler> clicks", () => {
    const { wrapper, enumProperties } = mount(
      {},
      {
        initialState: {
          objectInspector: {
            // Have the prototype already loaded so the component does not call
            // enumProperties for the root's properties.
            loadedProperties: new Map([["root", { prototype: {} }]]),
            evaluations: new Map()
          }
        }
      }
    );

    const nodes = wrapper.find(".node");

    const targetNode = nodes.at(1);
    const handlerNode = nodes.at(2);

    targetNode.simulate("click");
    // The function is called twice,
    // to get both non-indexed and indexed properties.
    expect(enumProperties.mock.calls).toHaveLength(2);
    expect(enumProperties.mock.calls[0][0]).toEqual({
      ignoreNonIndexedProperties: true
    });
    expect(enumProperties.mock.calls[1][0]).toEqual({
      ignoreIndexedProperties: true
    });

    handlerNode.simulate("click");
    // The function is called twice,
    // to get  both non-indexed and indexed properties.
    expect(enumProperties.mock.calls).toHaveLength(4);
    expect(enumProperties.mock.calls[2][0]).toEqual({
      ignoreNonIndexedProperties: true
    });
    expect(enumProperties.mock.calls[3][0]).toEqual({
      ignoreIndexedProperties: true
    });
  });
});
