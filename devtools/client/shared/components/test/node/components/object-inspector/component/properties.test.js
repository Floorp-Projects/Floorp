/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* global jest */

const {
  mountObjectInspector,
} = require("resource://devtools/client/shared/components/test/node/components/object-inspector/test-utils.js");
const gripRepStubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/grip.js");
const ObjectFront = require("resource://devtools/client/shared/components/test/node/__mocks__/object-front.js");

const {
  formatObjectInspector,
} = require("resource://devtools/client/shared/components/test/node/components/object-inspector/test-utils.js");

function generateDefaults(overrides) {
  return {
    autoExpandDepth: 0,
    createObjectFront: grip => ObjectFront(grip),
    ...overrides,
  };
}

function getEnumPropertiesMock() {
  return jest.fn(() => ({
    slice: () => ({}),
  }));
}

function mount(props, { initialState } = {}) {
  const enumProperties = getEnumPropertiesMock();

  const client = {
    createObjectFront: grip => ObjectFront(grip, { enumProperties }),
    getFrontByID: _id => null,
  };

  const obj = mountObjectInspector({
    client,
    props: generateDefaults(props),
    initialState,
  });

  return { ...obj, enumProperties };
}
describe("ObjectInspector - properties", () => {
  it("does not load properties if properties are already loaded", () => {
    const stub = gripRepStubs.get("testMaxProps");

    const { enumProperties } = mount(
      {
        autoExpandDepth: 1,
        roots: [
          {
            path: "root",
            contents: {
              value: stub,
            },
          },
        ],
      },
      {
        initialState: {
          objectInspector: {
            loadedProperties: new Map([
              ["root", { ownProperties: stub.preview.ownProperties }],
            ]),
            evaluations: new Map(),
          },
        },
      }
    );

    expect(enumProperties.mock.calls).toHaveLength(0);
  });

  it("calls enumProperties when expandable leaf is clicked", () => {
    const stub = gripRepStubs.get("testMaxProps");
    const { enumProperties, wrapper } = mount({
      roots: [
        {
          path: "root",
          contents: {
            value: stub,
          },
        },
      ],
      createObjectFront: grip => ObjectFront(grip, { enumProperties }),
    });

    const node = wrapper.find(".node");
    node.simulate("click");

    // The function is called twice, to get both non-indexed and indexed props.
    expect(enumProperties.mock.calls).toHaveLength(2);
    expect(enumProperties.mock.calls[0][0]).toEqual({
      ignoreNonIndexedProperties: true,
    });
    expect(enumProperties.mock.calls[1][0]).toEqual({
      ignoreIndexedProperties: true,
    });
  });

  it("renders uninitialized bindings", () => {
    const { wrapper } = mount({
      roots: [
        {
          name: "someFoo",
          path: "root/someFoo",
          contents: {
            value: {
              uninitialized: true,
            },
          },
        },
      ],
    });

    expect(formatObjectInspector(wrapper)).toMatchSnapshot();
  });

  it("renders unmapped bindings", () => {
    const { wrapper } = mount({
      roots: [
        {
          name: "someFoo",
          path: "root/someFoo",
          contents: {
            value: {
              unmapped: true,
            },
          },
        },
      ],
    });

    expect(formatObjectInspector(wrapper)).toMatchSnapshot();
  });

  it("renders unscoped bindings", () => {
    const { wrapper } = mount({
      roots: [
        {
          name: "someFoo",
          path: "root/someFoo",
          contents: {
            value: {
              unscoped: true,
            },
          },
        },
      ],
    });

    expect(formatObjectInspector(wrapper)).toMatchSnapshot();
  });
});
