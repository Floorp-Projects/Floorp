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
  mockAccessible,
  setupStore,
} = require("resource://devtools/client/accessibility/test/node/helpers.js");

const Badges = require("resource://devtools/client/accessibility/components/Badges.js");
const {
  REPS: { Rep },
} = require("resource://devtools/client/shared/components/reps/index.js");
const AuditController = require("resource://devtools/client/accessibility/components/AuditController.js");

const AccessibilityRowValueClass = require("resource://devtools/client/accessibility/components/AccessibilityRowValue.js");
const AccessibilityRowValue = createFactory(AccessibilityRowValueClass);

describe("AccessibilityRowValue component:", () => {
  it("basic render", () => {
    const store = setupStore({
      preloadedState: { ui: { supports: {} } },
    });
    const wrapper = mount(
      Provider(
        { store },
        AccessibilityRowValue({
          member: { object: mockAccessible() },
        })
      )
    );

    expect(wrapper.html()).toMatchSnapshot();
    const rowValue = wrapper.find(AccessibilityRowValueClass);
    expect(rowValue.children().length).toBe(1);
    const container = rowValue.childAt(0);
    expect(container.type()).toBe("span");
    expect(container.prop("role")).toBe("presentation");
    expect(container.children().length).toBe(2);
    expect(container.childAt(0).type()).toBe(Rep);
    const controller = container.childAt(1);
    expect(controller.type()).toBe(AuditController);
    expect(controller.children().length).toBe(1);
    expect(controller.childAt(0).type()).toBe(Badges);
  });
});
