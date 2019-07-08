/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { mount } = require("enzyme");

const { createFactory } = require("devtools/client/shared/vendor/react");
const Provider = createFactory(
  require("devtools/client/shared/vendor/react-redux").Provider
);

const {
  ToggleButton,
} = require("devtools/client/accessibility/components/Button");
const ConnectedAccessibilityTreeFilterClass = require("devtools/client/accessibility/components/AccessibilityTreeFilter");
const AccessibilityTreeFilterClass =
  ConnectedAccessibilityTreeFilterClass.WrappedComponent;
const AccessibilityTreeFilter = createFactory(
  ConnectedAccessibilityTreeFilterClass
);
const {
  setupStore,
} = require("devtools/client/accessibility/test/jest/helpers");

const {
  AUDIT,
  AUDITING,
  FILTERS,
  FILTER_TOGGLE,
} = require("devtools/client/accessibility/constants");
const {
  accessibility: { AUDIT_TYPE },
} = require("devtools/shared/constants");

function checkFilter(button, expectedText) {
  expect(button.is("button")).toBe(true);
  expect(button.hasClass("badge")).toBe(true);
  expect(button.hasClass("toggle-button")).toBe(true);
  expect(button.hasClass("audit-badge")).toBe(false);
  expect(button.prop("aria-pressed")).toBe(false);
  expect(button.text()).toBe(expectedText);
}

function checkToggleFilter(wrapper, filter) {
  const filterInstance = wrapper.find(AccessibilityTreeFilterClass).instance();
  filterInstance.toggleFilter = jest.fn();
  filter.simulate("keydown", { key: " " });
  expect(filterInstance.toggleFilter.mock.calls.length).toBe(1);

  filter.simulate("keydown", { key: "Enter" });
  expect(filterInstance.toggleFilter.mock.calls.length).toBe(2);

  filter.simulate("click", { clientX: 1 });
  expect(filterInstance.toggleFilter.mock.calls.length).toBe(3);
}

function checkFiltersState(wrapper, expected) {
  const filters = wrapper.find(AccessibilityTreeFilterClass);
  const filterButtons = filters.find(ToggleButton);

  for (let i = 0; i < filterButtons.length; i++) {
    const filter = filterButtons.at(i).childAt(0);
    expect(filter.prop("aria-pressed")).toBe(expected[i].active);
    expect(filter.prop("aria-busy")).toBe(expected[i].busy);
  }
}

describe("AccessibilityTreeFilter component:", () => {
  it("audit filter not filtered", () => {
    const store = setupStore();

    const wrapper = mount(Provider({ store }, AccessibilityTreeFilter()));
    expect(wrapper.html()).toMatchSnapshot();

    const filters = wrapper.find(AccessibilityTreeFilterClass);
    expect(filters.children().length).toBe(1);

    const toolbar = filters.childAt(0);
    expect(toolbar.is("div")).toBe(true);
    expect(toolbar.prop("role")).toBe("toolbar");

    const filterButtons = filters.find(ToggleButton);
    expect(filterButtons.length).toBe(3);

    const expectedText = [
      "accessibility.filter.all",
      "accessibility.badge.contrast",
      "accessibility.badge.textLabel",
    ];

    for (let i = 0; i < filterButtons.length; i++) {
      checkFilter(filterButtons.at(i).childAt(0), expectedText[i]);
    }
  });

  it("audit filters filtered", () => {
    const store = setupStore({
      preloadedState: {
        audit: {
          filters: {
            [FILTERS.ALL]: true,
            [FILTERS.CONTRAST]: true,
            [FILTERS.TEXT_LABEL]: true,
          },
          auditing: [],
        },
      },
    });

    const wrapper = mount(Provider({ store }, AccessibilityTreeFilter()));
    expect(wrapper.html()).toMatchSnapshot();

    const buttons = wrapper.find("button");
    for (let i = 0; i < buttons.length; i++) {
      const button = buttons.at(i);
      expect(button.prop("aria-pressed")).toBe(true);
      expect(button.hasClass("checked")).toBe(true);
    }
  });

  it("audit filter not filtered auditing", () => {
    const store = setupStore({
      preloadedState: {
        audit: {
          filters: {
            [FILTERS.CONTRAST]: false,
          },
          auditing: [AUDIT_TYPE.CONTRAST],
        },
      },
    });

    const wrapper = mount(Provider({ store }, AccessibilityTreeFilter()));
    expect(wrapper.html()).toMatchSnapshot();

    const button = wrapper.find("button");
    expect(button.prop("aria-pressed")).toBe(false);
    expect(button.hasClass("checked")).toBe(false);
    expect(button.prop("aria-busy")).toBe(true);
    expect(button.hasClass("devtools-throbber")).toBe(true);
  });

  it("audit filter filtered auditing", () => {
    const store = setupStore({
      preloadedState: {
        audit: {
          filters: {
            [FILTERS.CONTRAST]: true,
          },
          auditing: [AUDIT_TYPE.CONTRAST],
        },
      },
    });

    const wrapper = mount(Provider({ store }, AccessibilityTreeFilter()));
    expect(wrapper.html()).toMatchSnapshot();

    const button = wrapper.find("button");
    expect(button.prop("aria-pressed")).toBe(true);
    expect(button.hasClass("checked")).toBe(true);
    expect(button.prop("aria-busy")).toBe(true);
    expect(button.hasClass("devtools-throbber")).toBe(true);
  });

  it("toggle filter", () => {
    const store = setupStore();
    const wrapper = mount(Provider({ store }, AccessibilityTreeFilter()));
    expect(wrapper.html()).toMatchSnapshot();

    const filters = wrapper.find("button.toggle-button.badge");
    for (let i = 0; i < filters.length; i++) {
      const filter = filters.at(i);
      checkToggleFilter(wrapper, filter);
    }
  });

  it("render filters after state changes", () => {
    const store = setupStore();
    const wrapper = mount(Provider({ store }, AccessibilityTreeFilter()));

    const tests = [
      {
        expected: [
          { active: false, busy: false },
          { active: false, busy: false },
          { active: false, busy: false },
        ],
      },
      {
        action: {
          type: AUDITING,
          auditing: Object.values(FILTERS),
        },
        expected: [
          { active: false, busy: true },
          { active: false, busy: true },
          { active: false, busy: true },
        ],
      },
      {
        action: {
          type: AUDIT,
          response: [],
        },
        expected: [
          { active: false, busy: false },
          { active: false, busy: false },
          { active: false, busy: false },
        ],
      },
      {
        action: {
          type: FILTER_TOGGLE,
          filter: FILTERS.ALL,
        },
        expected: [
          { active: true, busy: false },
          { active: true, busy: false },
          { active: true, busy: false },
        ],
      },
      {
        action: {
          type: FILTER_TOGGLE,
          filter: FILTERS.CONTRAST,
        },
        expected: [
          { active: false, busy: false },
          { active: false, busy: false },
          { active: true, busy: false },
        ],
      },
      {
        action: {
          type: AUDITING,
          auditing: [FILTERS.CONTRAST],
        },
        expected: [
          { active: false, busy: false },
          { active: false, busy: true },
          { active: true, busy: false },
        ],
      },
      {
        action: {
          type: AUDIT,
          response: [],
        },
        expected: [
          { active: false, busy: false },
          { active: false, busy: false },
          { active: true, busy: false },
        ],
      },
      {
        action: {
          type: FILTER_TOGGLE,
          filter: FILTERS.CONTRAST,
        },
        expected: [
          { active: true, busy: false },
          { active: true, busy: false },
          { active: true, busy: false },
        ],
      },
      {
        action: {
          type: FILTER_TOGGLE,
          filter: FILTERS.ALL,
        },
        expected: [
          { active: false, busy: false },
          { active: false, busy: false },
          { active: false, busy: false },
        ],
      },
      {
        action: {
          type: AUDITING,
          auditing: [FILTERS.TEXT_LABEL],
        },
        expected: [
          { active: false, busy: false },
          { active: false, busy: false },
          { active: false, busy: true },
        ],
      },
      {
        action: {
          type: AUDIT,
          response: [],
        },
        expected: [
          { active: false, busy: false },
          { active: false, busy: false },
          { active: false, busy: false },
        ],
      },
      {
        action: {
          type: FILTER_TOGGLE,
          filter: FILTERS.TEXT_LABEL,
        },
        expected: [
          { active: false, busy: false },
          { active: false, busy: false },
          { active: true, busy: false },
        ],
      },
    ];

    for (const test of tests) {
      const { action, expected } = test;
      if (action) {
        store.dispatch(action);
        wrapper.update();
      }

      expect(wrapper.html()).toMatchSnapshot();
      checkFiltersState(wrapper, expected);
    }
  });
});
