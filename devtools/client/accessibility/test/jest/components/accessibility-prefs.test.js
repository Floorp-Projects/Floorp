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
const ConnectedAccessibilityPrefsClass = require("devtools/client/accessibility/components/AccessibilityPrefs");
const AccessibilityPrefsClass =
  ConnectedAccessibilityPrefsClass.WrappedComponent;
const AccessibilityPrefs = createFactory(ConnectedAccessibilityPrefsClass);
const {
  checkMenuItem,
  setupStore,
} = require("devtools/client/accessibility/test/jest/helpers");

const { PREFS } = require("devtools/client/accessibility/constants");

function checkTogglePrefCheckbox(wrapper, pref) {
  const prefsInstance = wrapper.find(AccessibilityPrefsClass).instance();
  prefsInstance.togglePref = jest.fn();
  pref instanceof Node ? pref.click() : pref.simulate("click");
  expect(prefsInstance.togglePref.mock.calls.length).toBe(1);
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

function checkPrefsState(wrapper, expected) {
  const prefs = getMenuItems(wrapper, ".pref");
  for (let i = 0; i < prefs.length; i++) {
    checkMenuItem(prefs[i], {
      checked: expected.prefs[i].active,
      label: expected.prefs[i].text,
    });
  }
  checkMenuItem(getMenuItems(wrapper, ".help")[0], {
    role: "link",
    label: "accessibility.documentation.label",
  });
}

describe("AccessibilityPrefs component:", () => {
  it("prefs not set by default", () => {
    const store = setupStore();
    const wrapper = mount(Provider({ store }, AccessibilityPrefs()));
    const accPrefs = wrapper.find(AccessibilityPrefsClass);
    const menuButton = accPrefs.childAt(0);

    expect(wrapper.html()).toMatchSnapshot();
    expect(accPrefs.children().length).toBe(1);
    expect(menuButton.is(MenuButton)).toBe(true);

    checkPrefsState(wrapper, {
      prefs: [
        { active: false, text: "accessibility.pref.scroll.into.view.label" },
      ],
    });
  });

  it("prefs checked", () => {
    const store = setupStore({
      preloadedState: {
        ui: {
          [PREFS.SCROLL_INTO_VIEW]: true,
        },
      },
    });
    const wrapper = mount(Provider({ store }, AccessibilityPrefs()));
    expect(wrapper.html()).toMatchSnapshot();
    checkPrefsState(wrapper, {
      prefs: [
        { active: true, text: "accessibility.pref.scroll.into.view.label" },
      ],
    });
  });

  it("toggle pref", () => {
    const store = setupStore();
    const wrapper = mount(Provider({ store }, AccessibilityPrefs()));

    expect(wrapper.html()).toMatchSnapshot();
    for (const pref of getMenuItems(wrapper, ".pref")) {
      checkTogglePrefCheckbox(wrapper, pref);
    }
  });
});
