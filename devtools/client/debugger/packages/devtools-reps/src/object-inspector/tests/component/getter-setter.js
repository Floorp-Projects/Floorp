/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { MODE } = require("../../../reps/constants");
const {
  formatObjectInspector,
  waitForLoadedProperties,
  mountObjectInspector,
} = require("../test-utils");

const { makeNodesForProperties } = require("../../utils/node");
const accessorStubs = require("../../../reps/stubs/accessor");
const ObjectClient = require("../__mocks__/object-client");

function generateDefaults(overrides) {
  return {
    autoExpandDepth: 1,
    createObjectClient: grip => ObjectClient(grip),
    mode: MODE.LONG,
    ...overrides,
  };
}

function mount(stub, propsOverride = {}) {
  const client = { createObjectClient: grip => ObjectClient(grip) };

  const root = { path: "root", name: "root" };
  const nodes = makeNodesForProperties(
    {
      ownProperties: {
        x: stub,
      },
    },
    root
  );
  root.contents = nodes;

  return mountObjectInspector({
    client,
    props: generateDefaults({ roots: [root], ...propsOverride }),
  });
}

describe("ObjectInspector - getters & setters", () => {
  it("renders getters as expected", async () => {
    const { store, wrapper } = mount(accessorStubs.get("getter"));
    await waitForLoadedProperties(store, ["root"]);
    wrapper.update();

    expect(formatObjectInspector(wrapper)).toMatchSnapshot();
  });

  it("renders setters as expected", async () => {
    const { store, wrapper } = mount(accessorStubs.get("setter"));
    await waitForLoadedProperties(store, ["root"]);
    wrapper.update();

    expect(formatObjectInspector(wrapper)).toMatchSnapshot();
  });

  it("renders getters and setters as expected", async () => {
    const { store, wrapper } = mount(accessorStubs.get("getter setter"));
    await waitForLoadedProperties(store, ["root"]);
    wrapper.update();

    expect(formatObjectInspector(wrapper)).toMatchSnapshot();
  });

  it("onInvokeGetterButtonClick + getter", async () => {
    const onInvokeGetterButtonClick = jest.fn();
    const { store, wrapper } = mount(accessorStubs.get("getter"), {
      onInvokeGetterButtonClick,
    });
    await waitForLoadedProperties(store, ["root"]);
    wrapper.update();

    expect(formatObjectInspector(wrapper)).toMatchSnapshot();
  });

  it("onInvokeGetterButtonClick + setter", async () => {
    const onInvokeGetterButtonClick = jest.fn();
    const { store, wrapper } = mount(accessorStubs.get("setter"), {
      onInvokeGetterButtonClick,
    });
    await waitForLoadedProperties(store, ["root"]);
    wrapper.update();

    expect(formatObjectInspector(wrapper)).toMatchSnapshot();
  });

  it("onInvokeGetterButtonClick + getter & setter", async () => {
    const onInvokeGetterButtonClick = jest.fn();
    const { store, wrapper } = mount(accessorStubs.get("getter setter"), {
      onInvokeGetterButtonClick,
    });
    await waitForLoadedProperties(store, ["root"]);
    wrapper.update();

    expect(formatObjectInspector(wrapper)).toMatchSnapshot();
  });
});
