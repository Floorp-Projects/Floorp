/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { mount } = require("enzyme");

const { createFactory } = require("devtools/client/shared/vendor/react");
const Provider = createFactory(
  require("devtools/client/shared/vendor/react-redux").Provider
);

const MenuButton = require("devtools/client/shared/components/menu/MenuButton");
const ConnectedAccessibilityTreeFilterClass = require("devtools/client/accessibility/components/AccessibilityTreeFilter");
const AccessibilityTreeFilterClass =
  ConnectedAccessibilityTreeFilterClass.WrappedComponent;
const AccessibilityTreeFilter = createFactory(
  ConnectedAccessibilityTreeFilterClass
);
const {
  checkMenuItem,
  setupStore,
} = require("devtools/client/accessibility/test/jest/helpers");

const {
  AUDIT,
  AUDITING,
  FILTERS,
  FILTER_TOGGLE,
} = require("devtools/client/accessibility/constants");

function checkToggleFilterCheckbox(wrapper, filter) {
  const filterInstance = wrapper.find(AccessibilityTreeFilterClass).instance();
  filterInstance.toggleFilter = jest.fn();
  filter.click();
  expect(filterInstance.toggleFilter.mock.calls.length).toBe(1);
}

function getMenuItems(wrapper, selector) {
  const menuButton = wrapper.find(MenuButton);
  // Focusing on the menu button will trigger rendering of the HTMLTooltip with
  // the menu list.
  menuButton
    .childAt(0)
    .getDOMNode()
    .focus();

  return menuButton
    .instance()
    .tooltip.panel.querySelectorAll(`.menuitem ${selector}`);
}

function checkFiltersState(wrapper, expected) {
  const filters = getMenuItems(wrapper, ".filter");
  for (let i = 0; i < filters.length; i++) {
    checkMenuItem(filters[i], {
      checked: expected.filters[i].active,
      label: expected.filters[i].text,
      disabled: expected.filters[i].disabled,
    });
  }
}

describe("AccessibilityTreeFilter component:", () => {
  it("audit filter not filtered", () => {
    const store = setupStore();
    const wrapper = mount(Provider({ store }, AccessibilityTreeFilter()));
    const accTreeFilter = wrapper.find(AccessibilityTreeFilterClass);
    const toolbar = accTreeFilter.childAt(0);

    expect(wrapper.html()).toMatchSnapshot();
    expect(accTreeFilter.children().length).toBe(1);
    expect(toolbar.is("div")).toBe(true);
    expect(toolbar.prop("role")).toBe("group");

    checkFiltersState(wrapper, {
      filters: [
        { active: true, disabled: false, text: "accessibility.filter.none" },
        { active: false, disabled: false, text: "accessibility.filter.all2" },
        {
          active: false,
          disabled: false,
          text: "accessibility.filter.contrast",
        },
        {
          active: false,
          disabled: false,
          text: "accessibility.filter.keyboard",
        },
        {
          active: false,
          disabled: false,
          text: "accessibility.filter.textLabel",
        },
      ],
    });
  });

  it("audit filters filtered", () => {
    const store = setupStore({
      preloadedState: {
        audit: {
          filters: {
            [FILTERS.ALL]: true,
            [FILTERS.CONTRAST]: true,
            [FILTERS.KEYBOARD]: true,
            [FILTERS.TEXT_LABEL]: true,
          },
          auditing: [],
        },
      },
    });
    const wrapper = mount(Provider({ store }, AccessibilityTreeFilter()));
    expect(wrapper.html()).toMatchSnapshot();
    checkFiltersState(wrapper, {
      filters: [
        { active: false, disabled: false },
        { active: true, disabled: false },
        { active: true, disabled: false },
        { active: true, disabled: false },
        { active: true, disabled: false },
      ],
    });
  });

  it("audit all filter not filtered auditing", () => {
    const store = setupStore({
      preloadedState: {
        audit: {
          filters: {
            [FILTERS.ALL]: false,
          },
          auditing: [FILTERS.ALL],
        },
      },
    });
    const wrapper = mount(Provider({ store }, AccessibilityTreeFilter()));
    expect(wrapper.html()).toMatchSnapshot();
    checkFiltersState(wrapper, {
      filters: [
        { active: true, disabled: false, text: "accessibility.filter.none" },
        { active: false, disabled: true, text: "accessibility.filter.all2" },
      ],
    });
  });

  it("audit other filter not filtered auditing", () => {
    const store = setupStore({
      preloadedState: {
        audit: {
          filters: {
            [FILTERS.ALL]: false,
            [FILTERS.CONTRAST]: false,
            [FILTERS.KEYBOARD]: false,
            [FILTERS.TEXT_LABEL]: false,
          },
          auditing: [FILTERS.CONTRAST],
        },
      },
    });
    const wrapper = mount(Provider({ store }, AccessibilityTreeFilter()));
    expect(wrapper.html()).toMatchSnapshot();
    checkFiltersState(wrapper, {
      filters: [
        { active: true, disabled: true },
        { active: false, disabled: false },
        { active: false, disabled: true },
        { active: false, disabled: false },
        { active: false, disabled: false },
      ],
    });
  });

  it("audit all filter filtered auditing", () => {
    const store = setupStore({
      preloadedState: {
        audit: {
          filters: {
            [FILTERS.ALL]: true,
          },
          auditing: [FILTERS.ALL],
        },
      },
    });
    const wrapper = mount(Provider({ store }, AccessibilityTreeFilter()));
    const filters = getMenuItems(wrapper, ".filter");
    expect(wrapper.html()).toMatchSnapshot();
    checkMenuItem(filters[1], { checked: true, disabled: true });
  });

  it("audit other filter filtered auditing", () => {
    const store = setupStore({
      preloadedState: {
        audit: {
          filters: {
            [FILTERS.ALL]: false,
            [FILTERS.CONTRAST]: true,
            [FILTERS.KEYBOARD]: false,
            [FILTERS.TEXT_LABEL]: false,
          },
          auditing: [FILTERS.CONTRAST],
        },
      },
    });
    const wrapper = mount(Provider({ store }, AccessibilityTreeFilter()));
    expect(wrapper.html()).toMatchSnapshot();
    checkFiltersState(wrapper, {
      filters: [
        { active: false, disabled: true },
        { active: false, disabled: false },
        { active: true, disabled: true },
        { active: false, disabled: false },
        { active: false, disabled: false },
      ],
    });
  });

  it("toggle filter", () => {
    const store = setupStore();
    const wrapper = mount(Provider({ store }, AccessibilityTreeFilter()));
    const filters = getMenuItems(wrapper, ".filter");

    expect(wrapper.html()).toMatchSnapshot();
    for (const filter of filters) {
      checkToggleFilterCheckbox(wrapper, filter);
    }
  });

  it("render filters after state changes", () => {
    const store = setupStore();
    const wrapper = mount(Provider({ store }, AccessibilityTreeFilter()));
    const tests = [
      {
        expected: {
          filters: [
            { active: true, disabled: false },
            { active: false, disabled: false },
            { active: false, disabled: false },
            { active: false, disabled: false },
            { active: false, disabled: false },
          ],
        },
      },
      {
        action: {
          type: AUDITING,
          auditing: Object.values(FILTERS),
        },
        expected: {
          filters: [
            { active: true, disabled: true },
            { active: false, disabled: true },
            { active: false, disabled: true },
            { active: false, disabled: true },
            { active: false, disabled: true },
          ],
        },
      },
      {
        action: {
          type: AUDIT,
          response: [],
        },
        expected: {
          filters: [
            { active: true, disabled: false },
            { active: false, disabled: false },
            { active: false, disabled: false },
            { active: false, disabled: false },
            { active: false, disabled: false },
          ],
        },
      },
      {
        action: {
          type: FILTER_TOGGLE,
          filter: FILTERS.ALL,
        },
        expected: {
          filters: [
            { active: false, disabled: false },
            { active: true, disabled: false },
            { active: true, disabled: false },
            { active: true, disabled: false },
            { active: true, disabled: false },
          ],
        },
      },
      {
        action: {
          type: FILTER_TOGGLE,
          filter: FILTERS.CONTRAST,
        },
        expected: {
          filters: [
            { active: false, disabled: false },
            { active: false, disabled: false },
            { active: false, disabled: false },
            { active: true, disabled: false },
            { active: true, disabled: false },
          ],
        },
      },
      {
        action: {
          type: AUDITING,
          auditing: [FILTERS.CONTRAST],
        },
        expected: {
          filters: [
            { active: false, disabled: true },
            { active: false, disabled: false },
            { active: false, disabled: true },
            { active: true, disabled: false },
            { active: true, disabled: false },
          ],
        },
      },
      {
        action: {
          type: AUDIT,
          response: [],
        },
        expected: {
          filters: [
            { active: false, disabled: false },
            { active: false, disabled: false },
            { active: false, disabled: false },
            { active: true, disabled: false },
            { active: true, disabled: false },
          ],
        },
      },
      {
        action: {
          type: FILTER_TOGGLE,
          filter: FILTERS.CONTRAST,
        },
        expected: {
          filters: [
            { active: false, disabled: false },
            { active: true, disabled: false },
            { active: true, disabled: false },
            { active: true, disabled: false },
            { active: true, disabled: false },
          ],
        },
      },
      {
        action: {
          type: FILTER_TOGGLE,
          filter: FILTERS.NONE,
        },
        expected: {
          filters: [
            { active: true, disabled: false },
            { active: false, disabled: false },
            { active: false, disabled: false },
            { active: false, disabled: false },
            { active: false, disabled: false },
          ],
        },
      },
      {
        action: {
          type: AUDITING,
          auditing: [FILTERS.TEXT_LABEL],
        },
        expected: {
          filters: [
            { active: true, disabled: true },
            { active: false, disabled: false },
            { active: false, disabled: false },
            { active: false, disabled: false },
            { active: false, disabled: true },
          ],
        },
      },
      {
        action: {
          type: AUDIT,
          response: [],
        },
        expected: {
          filters: [
            { active: true, disabled: false },
            { active: false, disabled: false },
            { active: false, disabled: false },
            { active: false, disabled: false },
            { active: false, disabled: false },
          ],
        },
      },
      {
        action: {
          type: FILTER_TOGGLE,
          filter: FILTERS.TEXT_LABEL,
        },
        expected: {
          filters: [
            { active: false, disabled: false },
            { active: false, disabled: false },
            { active: false, disabled: false },
            { active: false, disabled: false },
            { active: true, disabled: false },
          ],
        },
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
