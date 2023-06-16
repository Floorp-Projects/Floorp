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

const MenuButton = require("resource://devtools/client/shared/components/menu/MenuButton.js");
const ConnectedAccessibilityPrefsClass = require("resource://devtools/client/accessibility/components/AccessibilityPrefs.js");
const AccessibilityPrefsClass =
  ConnectedAccessibilityPrefsClass.WrappedComponent;
const AccessibilityPrefs = createFactory(ConnectedAccessibilityPrefsClass);
const {
  checkMenuItem,
  setupStore,
} = require("resource://devtools/client/accessibility/test/node/helpers.js");

const {
  PREFS,
} = require("resource://devtools/client/accessibility/constants.js");

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
  menuButton.childAt(0).getDOMNode().focus();

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
    label: "Documentation…",
  });
}

describe("AccessibilityPrefs component:", () => {
  it("prefs not set by default", () => {
    const store = setupStore();
    const wrapper = mount(
      Provider({ store }, AccessibilityPrefs({ toolboxDoc: document }))
    );
    const accPrefs = wrapper.find(AccessibilityPrefsClass);
    const menuButton = accPrefs.childAt(0);

    expect(wrapper.html()).toMatchSnapshot();
    expect(accPrefs.children().length).toBe(1);
    expect(menuButton.is(MenuButton)).toBe(true);

    checkPrefsState(wrapper, {
      prefs: [{ active: false, text: "Scroll into view" }],
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
    const wrapper = mount(
      Provider({ store }, AccessibilityPrefs({ toolboxDoc: document }))
    );
    expect(wrapper.html()).toMatchSnapshot();
    checkPrefsState(wrapper, {
      prefs: [{ active: true, text: "Scroll into view" }],
    });
  });

  it("toggle pref", () => {
    const store = setupStore();
    const wrapper = mount(
      Provider({ store }, AccessibilityPrefs({ toolboxDoc: document }))
    );

    expect(wrapper.html()).toMatchSnapshot();
    for (const pref of getMenuItems(wrapper, ".pref")) {
      checkTogglePrefCheckbox(wrapper, pref);
    }
  });
});
