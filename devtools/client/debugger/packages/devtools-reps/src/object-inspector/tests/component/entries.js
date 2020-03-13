/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* global jest */

const { mountObjectInspector } = require("../test-utils");
const { MODE } = require("../../../reps/constants");
const {
  formatObjectInspector,
  waitForDispatch,
  waitForLoadedProperties,
} = require("../test-utils");
const gripMapRepStubs = require("../../../reps/stubs/grip-map");
const mapStubs = require("../../stubs/map");
const ObjectFront = require("../__mocks__/object-front");

function generateDefaults(overrides) {
  return {
    autoExpandDepth: 0,
    createObjectFront: grip => ObjectFront(grip),
    ...overrides,
  };
}

function getEnumEntriesMock() {
  return jest.fn(() => ({
    slice: () => mapStubs.get("11-entries"),
  }));
}

function mount(props, { initialState }) {
  const enumEntries = getEnumEntriesMock();

  const client = {
    createObjectFront: grip => ObjectFront(grip, { enumEntries }),
  };
  const obj = mountObjectInspector({
    client,
    props: generateDefaults(props),
    initialState: {
      objectInspector: {
        ...initialState,
        evaluations: new Map(),
      },
    },
  });

  return { ...obj, enumEntries };
}

describe("ObjectInspector - entries", () => {
  it("renders Object with entries as expected", async () => {
    const stub = gripMapRepStubs.get("testSymbolKeyedMap");

    const { store, wrapper, enumEntries } = mount(
      {
        autoExpandDepth: 3,
        roots: [
          {
            path: "root",
            contents: { value: stub },
          },
        ],
        mode: MODE.LONG,
      },
      {
        initialState: {
          loadedProperties: new Map([["root", mapStubs.get("properties")]]),
        },
      }
    );

    await waitForLoadedProperties(store, [
      "root◦<entries>◦0",
      "root◦<entries>◦1",
    ]);

    wrapper.update();
    expect(formatObjectInspector(wrapper)).toMatchSnapshot();
  });

  it("calls ObjectFront.enumEntries when expected", async () => {
    const stub = gripMapRepStubs.get("testMoreThanMaxEntries");

    const { wrapper, store, enumEntries } = mount(
      {
        autoExpandDepth: 1,
        injectWaitService: true,
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
          loadedProperties: new Map([
            ["root", { ownProperties: stub.preview.entries }],
          ]),
        },
      }
    );

    expect(formatObjectInspector(wrapper)).toMatchSnapshot();

    const nodes = wrapper.find(".node");
    const entriesNode = nodes.at(1);
    expect(entriesNode.text()).toBe("<entries>");

    const onEntrieLoad = waitForDispatch(store, "NODE_PROPERTIES_LOADED");
    entriesNode.simulate("click");
    await onEntrieLoad;
    wrapper.update();

    expect(formatObjectInspector(wrapper)).toMatchSnapshot();
    expect(enumEntries.mock.calls).toHaveLength(1);

    entriesNode.simulate("click");
    expect(formatObjectInspector(wrapper)).toMatchSnapshot();

    entriesNode.simulate("click");

    expect(formatObjectInspector(wrapper)).toMatchSnapshot();
    // it does not call enumEntries if entries were already loaded.
    expect(enumEntries.mock.calls).toHaveLength(1);
  });
});
