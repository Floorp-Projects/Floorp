/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const {
  createNode,
} = require("resource://devtools/client/shared/components/object-inspector/utils/node.js");
const {
  waitForDispatch,
  mountObjectInspector,
} = require("resource://devtools/client/shared/components/test/node/components/object-inspector/test-utils.js");

const gripWindowStubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/window.js");
const ObjectFront = require("resource://devtools/client/shared/components/test/node/__mocks__/object-front.js");
const windowNode = createNode({
  name: "window",
  contents: { value: gripWindowStubs.get("Window")._grip },
});

const client = {
  createObjectFront: grip => ObjectFront(grip),
  getFrontByID: _id => null,
};

function generateDefaults(overrides) {
  return {
    autoExpandDepth: 0,
    roots: [windowNode],
    ...overrides,
  };
}

describe("ObjectInspector - dimTopLevelWindow", () => {
  it("renders window as expected when dimTopLevelWindow is true", async () => {
    const props = generateDefaults({
      dimTopLevelWindow: true,
    });

    const { wrapper, store } = mountObjectInspector({ client, props });
    let nodes = wrapper.find(".node");
    const node = nodes.at(0);

    expect(nodes.at(0).hasClass("lessen")).toBeTruthy();
    expect(wrapper).toMatchSnapshot();

    const onPropertiesLoaded = waitForDispatch(store, "NODE_PROPERTIES_LOADED");
    node.simulate("click");
    await onPropertiesLoaded;
    wrapper.update();

    nodes = wrapper.find(".node");
    expect(nodes.at(0).hasClass("lessen")).toBeFalsy();
    expect(wrapper).toMatchSnapshot();
  });

  it("renders collapsed top-level window when dimTopLevelWindow =false", () => {
    // The window node should not have the "lessen" class when
    // dimTopLevelWindow is falsy.
    const props = generateDefaults();
    const { wrapper } = mountObjectInspector({ client, props });

    expect(wrapper.find(".node.lessen").exists()).toBeFalsy();
    expect(wrapper).toMatchSnapshot();
  });

  it("renders sub-level window", async () => {
    // The window node should not have the "lessen" class when it is not at
    // top level.
    const root = createNode({
      name: "root",
      contents: [windowNode],
    });

    const props = generateDefaults({
      roots: [root],
      dimTopLevelWindow: true,
      injectWaitService: true,
    });
    const { wrapper, store } = mountObjectInspector({ client, props });

    let nodes = wrapper.find(".node");
    const node = nodes.at(0);
    const onPropertiesLoaded = waitForDispatch(store, "NODE_PROPERTIES_LOADED");
    node.simulate("click");
    await onPropertiesLoaded;
    wrapper.update();

    nodes = wrapper.find(".node");
    const win = nodes.at(1);

    // Make sure we target the window object.
    expect(win.find(".objectBox-Window").exists()).toBeTruthy();
    expect(win.hasClass("lessen")).toBeFalsy();
    expect(wrapper).toMatchSnapshot();
  });
});
