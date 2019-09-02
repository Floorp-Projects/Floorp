/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { reducers } = require("devtools/client/accessibility/reducers/index");
const CheckClass = require("devtools/client/accessibility/components/Check");
const FluentReact = require("devtools/client/shared/vendor/fluent-react");

const {
  createStore,
  combineReducers,
} = require("devtools/client/shared/vendor/redux");

/**
 * Prepare the store for use in testing.
 */
function setupStore({ preloadedState } = {}) {
  const store = createStore(combineReducers(reducers), preloadedState);
  return store;
}

/**
 * Build a mock accessible object.
 * @param {Object} form
 *        Data similar to what accessible actor passes to accessible front.
 */
function mockAccessible(form) {
  return {
    on: jest.fn(),
    off: jest.fn(),
    audit: jest.fn().mockReturnValue(Promise.resolve()),
    ...form,
  };
}

/**
 *
 * @param {DOMNode}
 *        DOMNode that corresponds to a menu item in a menu list
 * @param {Object}
 *        Expected properties:
 *          - role:     optional ARIA role for the menu item
 *          - checked:  optional checked state for the menu item
 *          - disabled: optional disabled state for the menu item
 *          - label:    optional text label for the menu item
 */
function checkMenuItem(menuItem, expected) {
  expect(menuItem.tagName).toBe("BUTTON");
  if (expected.role) {
    expect(menuItem.getAttribute("role")).toBe(expected.role);
  } else if (typeof expected.checked !== "undefined") {
    expect(menuItem.getAttribute("role")).toBe("menuitemcheckbox");
  } else {
    expect(menuItem.getAttribute("role")).toBe("menuitem");
  }

  if (typeof expected.checked !== "undefined") {
    expect(menuItem.hasAttribute("aria-checked")).toBe(expected.checked);
  }

  if (expected.checked) {
    expect(menuItem.getAttribute("aria-checked")).toBe("true");
  }

  if (expected.disabled) {
    expect(menuItem.hasAttribute("disabled")).toBe(true);
  }

  if (expected.label) {
    expect(menuItem.textContent).toBe(expected.label);
  }
}

/**
 *
 * @param {ReactWrapper}
 *        React wrapper for the top level check component.
 * @param {Object}
 *        Expected audit properties:
 *          - score: audit score
 *          - issue: audit issue type
 */
function testCustomCheck(wrapper, props) {
  expect(wrapper.html()).toMatchSnapshot();
  expect(wrapper.children().length).toBe(1);
  const check = wrapper.childAt(0);
  expect(wrapper.find(CheckClass)).toStrictEqual(check);
  testCheck(check, props);
}

/**
 *
 * @param {ReactWrapper}
 *        React wrapper for the check component.
 * @param {Object}
 *        Expected audit properties:
 *          - score: audit score
 *          - issue: audit issue type
 */
function testCheck(wrapper, props) {
  expect(wrapper.html()).toMatchSnapshot();
  const container = wrapper.childAt(0);
  expect(container.hasClass("accessibility-check")).toBe(true);
  expect(container.prop("role")).toBe("presentation");
  expect(wrapper.props()).toMatchObject(props);

  const localized = wrapper.find(FluentReact.Localized);
  expect(localized.length).toBe(3);

  const heading = localized.at(0).childAt(0);
  expect(heading.type()).toBe("h3");
  expect(heading.hasClass("accessibility-check-header")).toBe(true);

  const icon = localized.at(1).childAt(0);
  expect(icon.type()).toBe("img");
  expect(icon.prop("data-score")).toEqual(props.score);

  const annotation = localized.at(2).childAt(0);
  expect(annotation.type()).toBe("p");
  expect(annotation.hasClass("accessibility-check-annotation")).toBe(true);
}

module.exports = {
  checkMenuItem,
  mockAccessible,
  setupStore,
  testCheck,
  testCustomCheck,
};
