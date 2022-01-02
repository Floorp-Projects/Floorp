/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const {
  mountObjectInspector,
} = require("devtools/client/shared/components/test/node/components/object-inspector/test-utils");
const { MODE } = require("devtools/client/shared/components/reps/index");

const {
  formatObjectInspector,
  waitForDispatch,
} = require("devtools/client/shared/components/test/node/components/object-inspector/test-utils");
const ObjectFront = require("devtools/client/shared/components/test/node/__mocks__/object-front");
const gripRepStubs = require(`devtools/client/shared/components/test/node/stubs/reps/grip`);

function generateDefaults(overrides) {
  return {
    autoExpandDepth: 0,
    mode: MODE.LONG,
    ...overrides,
  };
}

function mount(props) {
  const client = {
    createObjectFront: grip => ObjectFront(grip),
    getFrontByID: _id => null,
  };

  return mountObjectInspector({
    client,
    props: generateDefaults(props),
  });
}

describe("ObjectInspector - keyboard navigation", () => {
  it("works as expected", async () => {
    const stub = gripRepStubs.get("testMaxProps");

    const { wrapper, store } = mount({
      roots: [{ path: "root", contents: { value: stub } }],
    });

    expect(formatObjectInspector(wrapper)).toMatchSnapshot();

    wrapper.simulate("focus");
    expect(formatObjectInspector(wrapper)).toMatchSnapshot();

    // Pressing right arrow key should expand the node and lod its properties.
    const onPropertiesLoaded = waitForDispatch(store, "NODE_PROPERTIES_LOADED");
    simulateKeyDown(wrapper, "ArrowRight");
    await onPropertiesLoaded;
    wrapper.update();
    expect(formatObjectInspector(wrapper)).toMatchSnapshot();

    // The child node should be focused.
    keyNavigate(wrapper, store, "ArrowDown");
    expect(formatObjectInspector(wrapper)).toMatchSnapshot();

    // The root node should be focused again.
    keyNavigate(wrapper, store, "ArrowLeft");
    expect(formatObjectInspector(wrapper)).toMatchSnapshot();

    // The child node should be focused again.
    keyNavigate(wrapper, store, "ArrowRight");
    expect(formatObjectInspector(wrapper)).toMatchSnapshot();

    // The root node should be focused again.
    keyNavigate(wrapper, store, "ArrowUp");
    expect(formatObjectInspector(wrapper)).toMatchSnapshot();

    wrapper.simulate("blur");
    expect(formatObjectInspector(wrapper)).toMatchSnapshot();
  });
});

function keyNavigate(wrapper, store, key) {
  simulateKeyDown(wrapper, key);
  wrapper.update();
}

function simulateKeyDown(wrapper, key) {
  wrapper.simulate("keydown", {
    key,
    preventDefault: () => {},
    stopPropagation: () => {},
  });
}
