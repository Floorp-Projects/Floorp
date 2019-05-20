/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { mountObjectInspector } = require("../test-utils");
const { MODE } = require("../../../reps/constants");
const { createNode } = require("../../utils/node");

const functionStubs = require("../../../reps/stubs/function");
const ObjectClient = require("../__mocks__/object-client");

function generateDefaults(overrides) {
  return {
    autoExpandDepth: 1,
    ...overrides,
  };
}

function mount(props) {
  const client = { createObjectClient: grip => ObjectClient(grip) };

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
