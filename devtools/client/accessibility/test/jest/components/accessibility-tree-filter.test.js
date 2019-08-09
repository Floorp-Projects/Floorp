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
  setupStore,
} = require("devtools/client/accessibility/test/jest/helpers");

const {
  AUDIT,
  AUDITING,
  FILTERS,
  FILTER_TOGGLE,
} = require("devtools/client/accessibility/constants");

function checkFilterCheckbox(filter, checked, expectedText, disabled) {
  expect(filter.tagName).toBe("BUTTON");
  expect(filter.getAttribute("role")).toBe("menuitemcheckbox");
  expect(filter.hasAttribute("aria-checked")).toBe(checked);
  if (checked) {
    expect(filter.getAttribute("aria-checked")).toBe("true");
  }

  if (disabled) {
    expect(filter.hasAttribute("disabled")).toBe(true);
  }

  if (expectedText) {
    expect(filter.textContent).toBe(expectedText);
  }
}

function checkToggleFilterCheckbox(wrapper, filter) {
  const filterInstance = wrapper.find(AccessibilityTreeFilterClass).instance();
  filterInstance.toggleFilter = jest.fn();
  filter instanceof Node ? filter.click() : filter.simulate("click");
  expect(filterInstance.toggleFilter.mock.calls.length).toBe(1);
}

function getFilters(wrapper) {
  const menuButton = wrapper.find(MenuButton);
  // Focusing on the menu button will trigger rendering of the HTMLTooltip with
  // the menu list.
  menuButton
    .childAt(0)
    .getDOMNode()
    .focus();

  return menuButton
    .instance()
    .tooltip.panel.querySelectorAll(".menuitem .filter");
}

function checkToolbarState(wrapper, expected) {
  const filters = getFilters(wrapper);
  for (let i = 0; i < filters.length; i++) {
    checkFilterCheckbox(
      filters[i],
      expected[i].active,
      expected[i].text,
      expected[i].disabled
    );
  }
}

function renderAccTreeFilterToolbar(store) {
  const wrapper = mount(Provider({ store }, AccessibilityTreeFilter()));
  const filters = getFilters(wrapper);
  return { filters, wrapper };
}

describe("AccessibilityTreeFilter component:", () => {
  it("audit filter not filtered", () => {
    const store = setupStore();
    const { wrapper } = renderAccTreeFilterToolbar(store);
    const accTreeFilter = wrapper.find(AccessibilityTreeFilterClass);
    const toolbar = accTreeFilter.childAt(0);

    expect(wrapper.html()).toMatchSnapshot();
    expect(accTreeFilter.children().length).toBe(1);
    expect(toolbar.is("div")).toBe(true);
    expect(toolbar.prop("role")).toBe("group");

    checkToolbarState(wrapper, [
      { active: true, disabled: false, text: "accessibility.filter.none" },
      { active: false, disabled: false, text: "accessibility.filter.all2" },
      { active: false, disabled: false, text: "accessibility.filter.contrast" },
      {
        active: false,
        disabled: false,
        text: "accessibility.filter.textLabel",
      },
    ]);
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
    const { wrapper } = renderAccTreeFilterToolbar(store);
    expect(wrapper.html()).toMatchSnapshot();
    checkToolbarState(wrapper, [
      { active: false, disabled: false },
      { active: true, disabled: false },
      { active: true, disabled: false },
      { active: true, disabled: false },
    ]);
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
    const { wrapper } = renderAccTreeFilterToolbar(store);
    expect(wrapper.html()).toMatchSnapshot();
    checkToolbarState(wrapper, [
      { active: true, disabled: false, text: "accessibility.filter.none" },
      { active: false, disabled: true, text: "accessibility.filter.all2" },
    ]);
  });

  it("audit other filter not filtered auditing", () => {
    const store = setupStore({
      preloadedState: {
        audit: {
          filters: {
            [FILTERS.ALL]: false,
            [FILTERS.CONTRAST]: false,
            [FILTERS.TEXT_LABEL]: false,
          },
          auditing: [FILTERS.CONTRAST],
        },
      },
    });
    const { wrapper } = renderAccTreeFilterToolbar(store);
    expect(wrapper.html()).toMatchSnapshot();
    checkToolbarState(wrapper, [
      { active: true, disabled: true },
      { active: false, disabled: false },
      { active: false, disabled: true },
      { active: false, disabled: false },
    ]);
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
    const { filters, wrapper } = renderAccTreeFilterToolbar(store);
    expect(wrapper.html()).toMatchSnapshot();
    checkFilterCheckbox(filters[1], true, null, true);
  });

  it("audit other filter filtered auditing", () => {
    const store = setupStore({
      preloadedState: {
        audit: {
          filters: {
            [FILTERS.ALL]: false,
            [FILTERS.CONTRAST]: true,
            [FILTERS.TEXT_LABEL]: false,
          },
          auditing: [FILTERS.CONTRAST],
        },
      },
    });
    const { wrapper } = renderAccTreeFilterToolbar(store);
    expect(wrapper.html()).toMatchSnapshot();
    checkToolbarState(wrapper, [
      { active: false, disabled: true },
      { active: false, disabled: false },
      { active: true, disabled: true },
      { active: false, disabled: false },
    ]);
  });

  it("toggle filter", () => {
    const store = setupStore();
    const { filters, wrapper } = renderAccTreeFilterToolbar(store);

    expect(wrapper.html()).toMatchSnapshot();
    for (const filter of filters) {
      checkToggleFilterCheckbox(wrapper, filter);
    }
  });

  it("render filters after state changes", () => {
    const store = setupStore();
    const { wrapper } = renderAccTreeFilterToolbar(store);
    const tests = [
      {
        expected: [
          { active: true, disabled: false },
          { active: false, disabled: false },
          { active: false, disabled: false },
          { active: false, disabled: false },
        ],
      },
      {
        action: {
          type: AUDITING,
          auditing: Object.values(FILTERS),
        },
        expected: [
          { active: true, disabled: true },
          { active: false, disabled: true },
          { active: false, disabled: true },
          { active: false, disabled: true },
        ],
      },
      {
        action: {
          type: AUDIT,
          response: [],
        },
        expected: [
          { active: true, disabled: false },
          { active: false, disabled: false },
          { active: false, disabled: false },
          { active: false, disabled: false },
        ],
      },
      {
        action: {
          type: FILTER_TOGGLE,
          filter: FILTERS.ALL,
        },
        expected: [
          { active: false, disabled: false },
          { active: true, disabled: false },
          { active: true, disabled: false },
          { active: true, disabled: false },
        ],
      },
      {
        action: {
          type: FILTER_TOGGLE,
          filter: FILTERS.CONTRAST,
        },
        expected: [
          { active: false, disabled: false },
          { active: false, disabled: false },
          { active: false, disabled: false },
          { active: true, disabled: false },
        ],
      },
      {
        action: {
          type: AUDITING,
          auditing: [FILTERS.CONTRAST],
        },
        expected: [
          { active: false, disabled: true },
          { active: false, disabled: false },
          { active: false, disabled: true },
          { active: true, disabled: false },
        ],
      },
      {
        action: {
          type: AUDIT,
          response: [],
        },
        expected: [
          { active: false, disabled: false },
          { active: false, disabled: false },
          { active: false, disabled: false },
          { active: true, disabled: false },
        ],
      },
      {
        action: {
          type: FILTER_TOGGLE,
          filter: FILTERS.CONTRAST,
        },
        expected: [
          { active: false, disabled: false },
          { active: true, disabled: false },
          { active: true, disabled: false },
          { active: true, disabled: false },
        ],
      },
      {
        action: {
          type: FILTER_TOGGLE,
          filter: FILTERS.NONE,
        },
        expected: [
          { active: true, disabled: false },
          { active: false, disabled: false },
          { active: false, disabled: false },
          { active: false, disabled: false },
        ],
      },
      {
        action: {
          type: AUDITING,
          auditing: [FILTERS.TEXT_LABEL],
        },
        expected: [
          { active: true, disabled: true },
          { active: false, disabled: false },
          { active: false, disabled: false },
          { active: false, disabled: true },
        ],
      },
      {
        action: {
          type: AUDIT,
          response: [],
        },
        expected: [
          { active: true, disabled: false },
          { active: false, disabled: false },
          { active: false, disabled: false },
          { active: false, disabled: false },
        ],
      },
      {
        action: {
          type: FILTER_TOGGLE,
          filter: FILTERS.TEXT_LABEL,
        },
        expected: [
          { active: false, disabled: false },
          { active: false, disabled: false },
          { active: false, disabled: false },
          { active: true, disabled: false },
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
      checkToolbarState(wrapper, expected);
    }
  });
});
