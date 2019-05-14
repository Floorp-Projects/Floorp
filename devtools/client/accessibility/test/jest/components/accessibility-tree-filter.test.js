/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { mount } = require("enzyme");

const { createFactory } = require("devtools/client/shared/vendor/react");
const Provider = createFactory(require("devtools/client/shared/vendor/react-redux").Provider);

const { ToggleButton } = require("devtools/client/accessibility/components/Button");
const ConnectedAccessibilityTreeFilterClass =
  require("devtools/client/accessibility/components/AccessibilityTreeFilter");
const AccessibilityTreeFilterClass =
  ConnectedAccessibilityTreeFilterClass.WrappedComponent;
const AccessibilityTreeFilter = createFactory(ConnectedAccessibilityTreeFilterClass);
const {
  setupStore,
} = require("devtools/client/accessibility/test/jest/helpers");
const { FILTERS } = require("devtools/client/accessibility/constants");

describe("AccessibilityTreeFilter component:", () => {
  it("audit filter not filtered", () => {
    const store = setupStore();

    const wrapper = mount(Provider({store}, AccessibilityTreeFilter()));
    expect(wrapper.html()).toMatchSnapshot();

    const filters = wrapper.find(AccessibilityTreeFilterClass);
    expect(filters.children().length).toBe(1);

    const toolbar = filters.childAt(0);
    expect(toolbar.is("div")).toBe(true);
    expect(toolbar.prop("role")).toBe("toolbar");

    const filterButtons = filters.find(ToggleButton);
    expect(filterButtons.length).toBe(1);

    const button = filterButtons.at(0).childAt(0);
    expect(button.is("button")).toBe(true);
    expect(button.hasClass("badge")).toBe(true);
    expect(button.hasClass("toggle-button")).toBe(true);
    expect(button.hasClass("audit-badge")).toBe(false);
    expect(button.prop("aria-pressed")).toBe(false);
    expect(button.text()).toBe("accessibility.badge.contrast");
  });

  it("audit filter filtered", () => {
    const store = setupStore({
      preloadedState: { audit: { filters: { [FILTERS.CONTRAST]: true }}},
    });

    const wrapper = mount(Provider({store}, AccessibilityTreeFilter()));
    expect(wrapper.html()).toMatchSnapshot();

    const button = wrapper.find("button");
    expect(button.prop("aria-pressed")).toBe(true);
    expect(button.hasClass("checked")).toBe(true);
  });

  it("audit filter not filtered auditing", () => {
    const store = setupStore({
      preloadedState: { audit: {
        filters: {
          [FILTERS.CONTRAST]: false,
        },
        auditing: FILTERS.CONTRAST,
      }},
    });

    const wrapper = mount(Provider({store}, AccessibilityTreeFilter()));
    expect(wrapper.html()).toMatchSnapshot();

    const button = wrapper.find("button");
    expect(button.prop("aria-pressed")).toBe(false);
    expect(button.hasClass("checked")).toBe(false);
    expect(button.prop("aria-busy")).toBe(true);
    expect(button.hasClass("devtools-throbber")).toBe(true);
  });

  it("audit filter filtered auditing", () => {
    const store = setupStore({
      preloadedState: { audit: {
        filters: {
          [FILTERS.CONTRAST]: true,
        },
        auditing: FILTERS.CONTRAST,
      }},
    });

    const wrapper = mount(Provider({store}, AccessibilityTreeFilter()));
    expect(wrapper.html()).toMatchSnapshot();

    const button = wrapper.find("button");
    expect(button.prop("aria-pressed")).toBe(true);
    expect(button.hasClass("checked")).toBe(true);
    expect(button.prop("aria-busy")).toBe(true);
    expect(button.hasClass("devtools-throbber")).toBe(true);
  });

  it("toggle filter", () => {
    const store = setupStore();
    const wrapper = mount(Provider({store}, AccessibilityTreeFilter()));
    expect(wrapper.html()).toMatchSnapshot();

    const filterInstance = wrapper.find(AccessibilityTreeFilterClass).instance();
    filterInstance.toggleFilter = jest.fn();
    wrapper.find("button.toggle-button.badge").simulate("keydown", { key: " " });
    expect(filterInstance.toggleFilter.mock.calls.length).toBe(1);

    wrapper.find("button.toggle-button.badge").simulate("keydown", { key: "Enter" });
    expect(filterInstance.toggleFilter.mock.calls.length).toBe(2);

    wrapper.find("button.toggle-button.badge").simulate("click", { clientX: 1 });
    expect(filterInstance.toggleFilter.mock.calls.length).toBe(3);
  });
});
