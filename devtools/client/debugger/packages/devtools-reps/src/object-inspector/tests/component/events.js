/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* global jest */
const { mountObjectInspector } = require("../test-utils");

const gripRepStubs = require("../../../reps/stubs/grip");
const ObjectClient = require("../__mocks__/object-client");

function generateDefaults(overrides) {
  return {
    autoExpandDepth: 0,
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

describe("ObjectInspector - properties", () => {
  it("calls the onFocus prop when provided one and given focus", () => {
    const stub = gripRepStubs.get("testMaxProps");
    const onFocus = jest.fn();

    const { wrapper } = mount({
      roots: [
        {
          path: "root",
          contents: {
            value: stub,
          },
        },
      ],
      onFocus,
    });

    const node = wrapper.find(".node").first();
    node.simulate("focus");

    expect(onFocus.mock.calls).toHaveLength(1);
  });

  it("doesn't call the onFocus when given focus but focusable is false", () => {
    const stub = gripRepStubs.get("testMaxProps");
    const onFocus = jest.fn();

    const { wrapper } = mount({
      focusable: false,
      roots: [
        {
          path: "root",
          contents: {
            value: stub,
          },
        },
      ],
      onFocus,
    });

    const node = wrapper.find(".node").first();
    node.simulate("focus");

    expect(onFocus.mock.calls).toHaveLength(0);
  });

  it("calls onDoubleClick prop when provided one and double clicked", () => {
    const stub = gripRepStubs.get("testMaxProps");
    const onDoubleClick = jest.fn();

    const { wrapper } = mount({
      roots: [
        {
          path: "root",
          contents: {
            value: stub,
          },
        },
      ],
      onDoubleClick,
    });

    const node = wrapper.find(".node").first();
    node.simulate("doubleclick");

    expect(onDoubleClick.mock.calls).toHaveLength(1);
  });

  it("calls the onCmdCtrlClick prop when provided and cmd/ctrl-clicked", () => {
    const stub = gripRepStubs.get("testMaxProps");
    const onCmdCtrlClick = jest.fn();

    const { wrapper } = mount({
      roots: [
        {
          path: "root",
          contents: {
            value: stub,
          },
        },
      ],
      onCmdCtrlClick,
    });

    const node = wrapper.find(".node").first();
    node.simulate("click", { ctrlKey: true });

    expect(onCmdCtrlClick.mock.calls).toHaveLength(1);
  });

  it("calls the onLabel prop when provided one and label clicked", () => {
    const stub = gripRepStubs.get("testMaxProps");
    const onLabelClick = jest.fn();

    const { wrapper } = mount({
      roots: [
        {
          path: "root",
          name: "Label",
          contents: {
            value: stub,
          },
        },
      ],
      onLabelClick,
    });

    const label = wrapper.find(".object-label").first();
    label.simulate("click");

    expect(onLabelClick.mock.calls).toHaveLength(1);
  });

  it("does not call the onLabel prop when the user selected text", () => {
    const stub = gripRepStubs.get("testMaxProps");
    const onLabelClick = jest.fn();

    const { wrapper } = mount({
      roots: [
        {
          path: "root",
          name: "Label",
          contents: {
            value: stub,
          },
        },
      ],
      onLabelClick,
    });

    const label = wrapper.find(".object-label").first();

    // Set a selection using the mock.
    getSelection().setMockSelection("test");

    label.simulate("click");

    expect(onLabelClick.mock.calls).toHaveLength(0);

    // Clear the selection for other tests.
    getSelection().setMockSelection();
  });
});
