/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { mount } = require("enzyme");

const { createFactory } = require("devtools/client/shared/vendor/react");
const Provider = createFactory(require("devtools/client/shared/vendor/react-redux").Provider);
const { setupStore } = require("devtools/client/accessibility/test/jest/helpers");

const { ToggleButton } = require("devtools/client/accessibility/components/Button");
const ConnectedBadgeClass = require("devtools/client/accessibility/components/Badge");
const BadgeClass = ConnectedBadgeClass.WrappedComponent;
const Badge = createFactory(ConnectedBadgeClass);

const { FILTERS } = require("devtools/client/accessibility/constants");

describe("Badge component:", () => {
  const props = {
    label: "Contrast",
    filterKey: FILTERS.CONTRAST,
  };

  it("basic render inactive", () => {
    const store = setupStore();
    const wrapper = mount(Provider({ store }, Badge(props)));
    expect(wrapper.html()).toMatchSnapshot();

    const badge = wrapper.find(BadgeClass);
    expect(badge.prop("active")).toBe(false);
    expect(badge.children().length).toBe(1);

    const toggleButton = badge.childAt(0);
    expect(toggleButton.type()).toBe(ToggleButton);
    expect(toggleButton.children().length).toBe(1);

    const button = toggleButton.childAt(0);
    expect(button.is("button")).toBe(true);
    expect(button.hasClass("audit-badge")).toBe(true);
    expect(button.hasClass("badge")).toBe(true);
    expect(button.hasClass("toggle-button")).toBe(true);
    expect(button.prop("aria-pressed")).toBe(false);
    expect(button.text()).toBe("Contrast");
  });

  it("basic render active", () => {
    const store = setupStore({
      preloadedState: { audit: { filters: { [FILTERS.CONTRAST]: true }}},
    });
    const wrapper = mount(Provider({ store }, Badge(props)));
    expect(wrapper.html()).toMatchSnapshot();
    const badge = wrapper.find(BadgeClass);
    expect(badge.prop("active")).toBe(true);

    const button = wrapper.find("button");
    expect(button.prop("aria-pressed")).toBe(true);
  });

  it("toggle filter", () => {
    const store = setupStore();
    const wrapper = mount(Provider({ store }, Badge(props)));
    expect(wrapper.html()).toMatchSnapshot();

    const badgeInstance = wrapper.find(BadgeClass).instance();
    badgeInstance.toggleFilter = jest.fn();
    wrapper.find("button.audit-badge.badge").simulate("keydown", { key: " " });
    expect(badgeInstance.toggleFilter.mock.calls.length).toBe(1);

    wrapper.find("button.audit-badge.badge").simulate("keydown", { key: "Enter" });
    expect(badgeInstance.toggleFilter.mock.calls.length).toBe(2);

    wrapper.find("button.audit-badge.badge").simulate("click");
    expect(badgeInstance.toggleFilter.mock.calls.length).toBe(3);
  });
});
