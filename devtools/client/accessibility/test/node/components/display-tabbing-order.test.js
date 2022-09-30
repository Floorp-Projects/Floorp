/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { mount } = require("enzyme");

const {
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const Provider = createFactory(
  require("resource://devtools/client/shared/vendor/react-redux.js").Provider
);
const {
  setupStore,
} = require("resource://devtools/client/accessibility/test/node/helpers.js");
const {
  UPDATE_DISPLAY_TABBING_ORDER,
} = require("resource://devtools/client/accessibility/constants.js");

const ConnectedDisplayTabbingOrderClass = require("resource://devtools/client/accessibility/components/DisplayTabbingOrder.js");
const DisplayTabbingOrderClass =
  ConnectedDisplayTabbingOrderClass.WrappedComponent;
const DisplayTabbingOrder = createFactory(ConnectedDisplayTabbingOrderClass);

function testCheckbox(wrapper, expected) {
  expect(wrapper.html()).toMatchSnapshot();
  const displayTabbingOrder = wrapper.find(DisplayTabbingOrderClass);
  expect(displayTabbingOrder.children().length).toBe(1);

  // Label checks
  const label = displayTabbingOrder.childAt(0);
  expect(label.hasClass("accessibility-tabbing-order")).toBe(true);
  expect(label.hasClass("devtools-checkbox-label")).toBe(true);
  expect(label.prop("title")).toBe(
    "Show tabbing order of elements and their tabbing index."
  );
  expect(label.text()).toBe("Show Tabbing Order");
  expect(label.children().length).toBe(1);

  // Checkbox checks
  const checkbox = label.childAt(0);
  expect(checkbox.prop("checked")).toBe(expected.checked);
  expect(checkbox.prop("disabled")).toBe(!!expected.disabled);
}

describe("DisplayTabbingOrder component:", () => {
  it("default render", () => {
    const store = setupStore();
    const wrapper = mount(Provider({ store }, DisplayTabbingOrder()));

    testCheckbox(wrapper, { checked: false });
  });

  it("toggle tabbing order overlay", () => {
    const store = setupStore();
    const wrapper = mount(Provider({ store }, DisplayTabbingOrder()));

    expect(wrapper.html()).toMatchSnapshot();
    const displayTabbingOrderInstance = wrapper
      .find(DisplayTabbingOrderClass)
      .instance();
    displayTabbingOrderInstance.onChange = jest.fn();
    displayTabbingOrderInstance.forceUpdate();
    const checkbox = wrapper.find("input");
    checkbox.simulate("change");
    expect(displayTabbingOrderInstance.onChange.mock.calls.length).toBe(1);
  });

  it("displaying tabbing order render/update", () => {
    const store = setupStore({
      preloadedState: {
        ui: {
          tabbingOrderDisplayed: true,
        },
      },
    });
    const wrapper = mount(Provider({ store }, DisplayTabbingOrder()));

    testCheckbox(wrapper, { checked: true });

    store.dispatch({
      type: UPDATE_DISPLAY_TABBING_ORDER,
      tabbingOrderDisplayed: false,
    });
    wrapper.update();

    testCheckbox(wrapper, { checked: false });
  });
});
