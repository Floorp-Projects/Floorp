/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* global jest */
const {
  mountObjectInspector,
} = require("resource://devtools/client/shared/components/test/node/components/object-inspector/test-utils.js");

const {
  MODE,
} = require("resource://devtools/client/shared/components/reps/reps/constants.js");
const gripStubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/grip.js");
const stub = gripStubs.get("testProxy");
const proxySlots = gripStubs.get("testProxySlots");
const {
  formatObjectInspector,
} = require("resource://devtools/client/shared/components/test/node/components/object-inspector/test-utils.js");

const ObjectFront = require("resource://devtools/client/shared/components/test/node/__mocks__/object-front.js");
function generateDefaults(overrides) {
  return {
    roots: [
      {
        path: "root",
        contents: {
          value: stub,
        },
      },
    ],
    autoExpandDepth: 1,
    mode: MODE.LONG,
    ...overrides,
  };
}

function getEnumPropertiesMock() {
  return jest.fn(() => ({
    slice: () => ({}),
  }));
}

function getProxySlotsMock() {
  return jest.fn(() => proxySlots);
}

function mount(props, { initialState } = {}) {
  const enumProperties = getEnumPropertiesMock();
  const getProxySlots = getProxySlotsMock();

  const client = {
    createObjectFront: grip =>
      ObjectFront(grip, { enumProperties, getProxySlots }),
    getFrontByID: _id => null,
  };

  const obj = mountObjectInspector({
    client,
    props: generateDefaults(props),
    initialState,
  });

  return { ...obj, enumProperties, getProxySlots };
}

describe("ObjectInspector - Proxy", () => {
  it("renders Proxy as expected", () => {
    const { wrapper, enumProperties, getProxySlots } = mount(
      {},
      {
        initialState: {
          objectInspector: {
            // Have the prototype already loaded so the component does not call
            // enumProperties for the root's properties.
            loadedProperties: new Map([["root", proxySlots]]),
            evaluations: new Map(),
          },
        },
      }
    );

    expect(formatObjectInspector(wrapper)).toMatchSnapshot();

    // enumProperties should not have been called.
    expect(enumProperties.mock.calls).toHaveLength(0);

    // getProxySlots should not have been called.
    expect(getProxySlots.mock.calls).toHaveLength(0);
  });

  it("calls enumProperties on <target> and <handler> clicks", () => {
    const { wrapper, enumProperties } = mount(
      {},
      {
        initialState: {
          objectInspector: {
            // Have the prototype already loaded so the component does not call
            // enumProperties for the root's properties.
            loadedProperties: new Map([["root", proxySlots]]),
            evaluations: new Map(),
          },
        },
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
      ignoreNonIndexedProperties: true,
    });
    expect(enumProperties.mock.calls[1][0]).toEqual({
      ignoreIndexedProperties: true,
    });

    handlerNode.simulate("click");
    // The function is called twice,
    // to get  both non-indexed and indexed properties.
    expect(enumProperties.mock.calls).toHaveLength(4);
    expect(enumProperties.mock.calls[2][0]).toEqual({
      ignoreNonIndexedProperties: true,
    });
    expect(enumProperties.mock.calls[3][0]).toEqual({
      ignoreIndexedProperties: true,
    });
  });
});
