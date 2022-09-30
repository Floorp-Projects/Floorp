/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const {
  mountObjectInspector,
} = require("resource://devtools/client/shared/components/test/node/components/object-inspector/test-utils.js");
const {
  MODE,
} = require("resource://devtools/client/shared/components/reps/reps/constants.js");
const {
  createNode,
} = require("resource://devtools/client/shared/components/object-inspector/utils/node.js");

const functionStubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/function.js");
const ObjectFront = require("resource://devtools/client/shared/components/test/node/__mocks__/object-front.js");

function generateDefaults(overrides) {
  return {
    autoExpandDepth: 1,
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

describe("ObjectInspector - functions", () => {
  it("renders named function properties as expected", () => {
    const stub = functionStubs.get("Named");
    const { wrapper } = mount({
      roots: [
        createNode({
          name: "fn",
          contents: { value: stub },
        }),
      ],
    });

    const nodes = wrapper.find(".node");
    const functionNode = nodes.first();
    expect(functionNode.text()).toBe("fn:testName()");
  });

  it("renders anon function properties as expected", () => {
    const stub = functionStubs.get("Anon");
    const { wrapper } = mount({
      roots: [
        createNode({
          name: "fn",
          contents: { value: stub },
        }),
      ],
    });

    const nodes = wrapper.find(".node");
    const functionNode = nodes.first();
    // It should have the name of the property.
    expect(functionNode.text()).toBe("fn()");
  });

  it("renders non-TINY mode functions as expected", () => {
    const stub = functionStubs.get("Named");
    const { wrapper } = mount({
      autoExpandDepth: 0,
      roots: [
        {
          path: "root",
          name: "x",
          contents: { value: stub },
        },
      ],
      mode: MODE.LONG,
    });

    const nodes = wrapper.find(".node");
    const functionNode = nodes.first();
    // It should have the name of the property.
    expect(functionNode.text()).toBe("x: function testName()");
  });
});
