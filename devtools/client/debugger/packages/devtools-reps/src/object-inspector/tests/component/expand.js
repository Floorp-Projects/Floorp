/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const { mountObjectInspector } = require("../test-utils");

const repsPath = "../../../reps";
const { MODE } = require(`${repsPath}/constants`);
const ObjectClient = require("../__mocks__/object-client");
const gripRepStubs = require(`${repsPath}/stubs/grip`);
const gripPropertiesStubs = require("../../stubs/grip");
const {
  formatObjectInspector,
  storeHasExactExpandedPaths,
  storeHasExpandedPath,
  storeHasLoadedProperty,
  waitForDispatch,
} = require("../test-utils");
const { createNode, NODE_TYPES } = require("../../utils/node");
const { getActors, getExpandedPaths } = require("../../reducer");

const protoStub = {
  prototype: {
    type: "object",
    actor: "server2.conn0.child1/obj628",
    class: "Object",
  },
};

function generateDefaults(overrides) {
  return {
    autoExpandDepth: 0,
    roots: [
      {
        path: "root-1",
        contents: {
          value: gripRepStubs.get("testMoreThanMaxProps"),
        },
      },
      {
        path: "root-2",
        contents: {
          value: gripRepStubs.get("testProxy"),
        },
      },
    ],
    createObjectClient: grip => ObjectClient(grip),
    mode: MODE.LONG,
    ...overrides,
  };
}
const LongStringClientMock = require("../__mocks__/long-string-client");

function mount(props, { initialState } = {}) {
  const client = {
    createObjectClient: grip =>
      ObjectClient(grip, {
        getPrototype: () => Promise.resolve(protoStub),
        getProxySlots: () =>
          Promise.resolve(gripRepStubs.get("testProxySlots")),
      }),

    createLongStringClient: grip =>
      LongStringClientMock(grip, {
        substring: function(initiaLength, length, cb) {
          cb({
            substring: "<<<<",
          });
        },
      }),
  };

  return mountObjectInspector({
    client,
    props: generateDefaults(props),
    initialState: {
      objectInspector: {
        ...initialState,
        evaluations: new Map(),
      },
    },
  });
}

describe("ObjectInspector - state", () => {
  it("has the expected expandedPaths state when clicking nodes", async () => {
    const { wrapper, store } = mount(
      {},
      {
        initialState: {
          loadedProperties: new Map([
            ["root-1", gripPropertiesStubs.get("proto-properties-symbols")],
          ]),
        },
      }
    );

    expect(formatObjectInspector(wrapper)).toMatchSnapshot();
    let nodes = wrapper.find(".node");

    // Clicking on the root node adds it path to "expandedPaths".
    const root1 = nodes.at(0);
    const root2 = nodes.at(1);

    root1.simulate("click");

    expect(storeHasExactExpandedPaths(store, ["root-1"])).toBeTruthy();
    expect(formatObjectInspector(wrapper)).toMatchSnapshot();
    //
    // Clicking on the root node removes it path from "expandedPaths".
    root1.simulate("click");
    expect(storeHasExactExpandedPaths(store, [])).toBeTruthy();
    expect(formatObjectInspector(wrapper)).toMatchSnapshot();

    const onPropertiesLoaded = waitForDispatch(store, "NODE_PROPERTIES_LOADED");
    root2.simulate("click");
    await onPropertiesLoaded;
    expect(storeHasExactExpandedPaths(store, ["root-2"])).toBeTruthy();

    wrapper.update();
    expect(formatObjectInspector(wrapper)).toMatchSnapshot();

    root1.simulate("click");
    expect(
      storeHasExactExpandedPaths(store, ["root-1", "root-2"])
    ).toBeTruthy();
    expect(formatObjectInspector(wrapper)).toMatchSnapshot();

    nodes = wrapper.find(".node");
    const propNode = nodes.at(1);
    const symbolNode = nodes.at(2);
    const protoNode = nodes.at(3);

    propNode.simulate("click");
    symbolNode.simulate("click");
    protoNode.simulate("click");

    expect(
      storeHasExactExpandedPaths(store, [
        "root-1",
        "root-2",
        "root-1◦<prototype>",
      ])
    ).toBeTruthy();

    // The property and symbols have primitive values, and can't be expanded.
    expect(getExpandedPaths(store.getState()).size).toBe(3);
  });

  it("has the expected state when expanding a node", async () => {
    const { wrapper, store } = mount({}, {});

    expect(formatObjectInspector(wrapper)).toMatchSnapshot();
    let nodes = wrapper.find(".node");
    const root1 = nodes.at(0);

    let onPropertiesLoad = waitForDispatch(store, "NODE_PROPERTIES_LOADED");
    root1.simulate("click");
    await onPropertiesLoad;
    wrapper.update();

    expect(storeHasLoadedProperty(store, "root-1")).toBeTruthy();
    // We don't want to track root actors.
    expect(
      getActors(store.getState()).has(
        gripRepStubs.get("testMoreThanMaxProps").actor
      )
    ).toBeFalsy();
    expect(formatObjectInspector(wrapper)).toMatchSnapshot();

    nodes = wrapper.find(".node");
    const protoNode = nodes.at(1);

    onPropertiesLoad = waitForDispatch(store, "NODE_PROPERTIES_LOADED");
    protoNode.simulate("click");
    await onPropertiesLoad;
    wrapper.update();

    // Once all the loading promises are resolved, actors and loadedProperties
    // should have the expected values.
    expect(formatObjectInspector(wrapper)).toMatchSnapshot();
    expect(storeHasLoadedProperty(store, "root-1◦<prototype>")).toBeTruthy();

    expect(
      getActors(store.getState()).has(protoStub.prototype.actor)
    ).toBeTruthy();
  });

  it.skip("has the expected state when expanding a proxy node", async () => {
    const { wrapper, store } = mount({});

    expect(formatObjectInspector(wrapper)).toMatchSnapshot();
    let nodes = wrapper.find(".node");

    const proxyNode = nodes.at(1);

    let onLoadProperties = waitForDispatch(store, "NODE_PROPERTIES_LOADED");
    proxyNode.simulate("click");
    await onLoadProperties;
    wrapper.update();

    // Once the properties are loaded, actors and loadedProperties should have
    // the expected values.
    expect(formatObjectInspector(wrapper)).toMatchSnapshot();

    // We don't want to track root actors.
    expect(
      getActors(store.getState()).has(gripRepStubs.get("testProxy").actor)
    ).toBeFalsy();

    nodes = wrapper.find(".node");
    const handlerNode = nodes.at(3);
    onLoadProperties = waitForDispatch(store, "NODE_PROPERTIES_LOADED");
    handlerNode.simulate("click");
    await onLoadProperties;
    wrapper.update();

    expect(formatObjectInspector(wrapper)).toMatchSnapshot();
    expect(storeHasLoadedProperty(store, "root-2◦<handler>")).toBeTruthy();
  });

  it("does not expand if the user selected some text", async () => {
    const { wrapper, store } = mount(
      {},
      {
        initialSate: {
          loadedProperties: new Map([
            ["root-1", gripPropertiesStubs.get("proto-properties-symbols")],
          ]),
        },
      }
    );

    expect(formatObjectInspector(wrapper)).toMatchSnapshot();
    const nodes = wrapper.find(".node");

    // Set a selection using the mock.
    getSelection().setMockSelection("test");

    const root1 = nodes.at(0);
    root1.simulate("click");
    expect(storeHasExpandedPath(store, "root-1")).toBeFalsy();
    expect(formatObjectInspector(wrapper)).toMatchSnapshot();

    // Clear the selection for other tests.
    getSelection().setMockSelection();
  });

  it("expands if user selected some text and clicked the arrow", async () => {
    const { wrapper, store } = mount(
      {},
      {
        initialState: {
          loadedProperties: new Map([
            ["root-1", gripPropertiesStubs.get("proto-properties-symbols")],
          ]),
        },
      }
    );

    expect(formatObjectInspector(wrapper)).toMatchSnapshot();
    const nodes = wrapper.find(".node");

    // Set a selection using the mock.
    getSelection().setMockSelection("test");

    const root1 = nodes.at(0);
    root1.find(".arrow").simulate("click");
    expect(getExpandedPaths(store.getState()).has("root-1")).toBeTruthy();
    expect(formatObjectInspector(wrapper)).toMatchSnapshot();

    // Clear the selection for other tests.
    getSelection().setMockSelection();
  });

  it("does not throw when expanding a block node", async () => {
    const blockNode = createNode({
      name: "Block",
      contents: [
        {
          name: "a",
          contents: {
            value: 30,
          },
        },
        {
          name: "b",
          contents: {
            value: 32,
          },
        },
      ],
      type: NODE_TYPES.BLOCK,
    });

    const proxyNode = createNode({
      name: "Proxy",
      contents: {
        value: gripRepStubs.get("testProxy"),
      },
    });

    const { wrapper, store } = mount({
      roots: [blockNode, proxyNode],
    });

    expect(formatObjectInspector(wrapper)).toMatchSnapshot();

    const nodes = wrapper.find(".node");
    const root = nodes.at(0);
    const onPropertiesLoaded = waitForDispatch(store, "NODE_PROPERTIES_LOADED");
    root.simulate("click");
    await onPropertiesLoaded;
    wrapper.update();

    expect(formatObjectInspector(wrapper)).toMatchSnapshot();
  });

  it("calls recordTelemetryEvent when expanding a node", async () => {
    const recordTelemetryEvent = jest.fn();
    const { wrapper, store } = mount(
      {
        recordTelemetryEvent,
      },
      {
        initialState: {
          loadedProperties: new Map([
            ["root-1", gripPropertiesStubs.get("proto-properties-symbols")],
          ]),
        },
      }
    );

    let nodes = wrapper.find(".node");
    const root1 = nodes.at(0);
    const root2 = nodes.at(1);

    // Expanding a node calls recordTelemetryEvent.
    root1.simulate("click");
    expect(recordTelemetryEvent.mock.calls).toHaveLength(1);
    expect(recordTelemetryEvent.mock.calls[0][0]).toEqual("object_expanded");

    // Collapsing a node does not call recordTelemetryEvent.
    root1.simulate("click");
    expect(recordTelemetryEvent.mock.calls).toHaveLength(1);

    // Expanding another node calls recordTelemetryEvent.
    const onPropertiesLoaded = waitForDispatch(store, "NODE_PROPERTIES_LOADED");
    root2.simulate("click");
    await onPropertiesLoaded;
    expect(recordTelemetryEvent.mock.calls).toHaveLength(2);
    expect(recordTelemetryEvent.mock.calls[1][0]).toEqual("object_expanded");

    wrapper.update();

    // Re-expanding a node calls recordTelemetryEvent.
    root1.simulate("click");
    expect(recordTelemetryEvent.mock.calls).toHaveLength(3);
    expect(recordTelemetryEvent.mock.calls[2][0]).toEqual("object_expanded");

    nodes = wrapper.find(".node");
    const propNode = nodes.at(1);
    const symbolNode = nodes.at(2);
    const protoNode = nodes.at(3);

    propNode.simulate("click");
    symbolNode.simulate("click");
    protoNode.simulate("click");

    // The property and symbols have primitive values, and can't be expanded.
    expect(recordTelemetryEvent.mock.calls).toHaveLength(4);
    expect(recordTelemetryEvent.mock.calls[3][0]).toEqual("object_expanded");
  });

  it("expanding a getter returning a longString does not throw", async () => {
    const { wrapper, store } = mount(
      {
        focusable: false,
      },
      {
        initialState: {
          loadedProperties: new Map([
            ["root-1", gripPropertiesStubs.get("longs-string-safe-getter")],
          ]),
        },
      }
    );

    wrapper
      .find(".node")
      .at(0)
      .simulate("click");
    wrapper.update();

    const onPropertiesLoaded = waitForDispatch(store, "NODE_PROPERTIES_LOADED");
    wrapper
      .find(".node")
      .at(1)
      .simulate("click");
    await onPropertiesLoaded;

    wrapper.update();
    expect(formatObjectInspector(wrapper)).toMatchSnapshot();
  });
});
