/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { mount } = require("enzyme");

const { createFactory } = require("devtools/client/shared/vendor/react");
const Provider = createFactory(require("devtools/client/shared/vendor/react-redux").Provider);
const {
  mockAccessible,
  setupStore,
} = require("devtools/client/accessibility/test/jest/helpers");

const Badges = require("devtools/client/accessibility/components/Badges");
const { REPS: {Rep} } = require("devtools/client/shared/components/reps/reps");
const AuditController = require("devtools/client/accessibility/components/AuditController");

const ConnectedAccessibilityRowValueClass =
  require("devtools/client/accessibility/components/AccessibilityRowValue");
const AccessibilityRowValueClass = ConnectedAccessibilityRowValueClass.WrappedComponent;
const AccessibilityRowValue = createFactory(ConnectedAccessibilityRowValueClass);

describe("AccessibilityRowValue component:", () => {
  it("audit not supported", () => {
    const store = setupStore({
      preloadedState: { ui: { supports: {}}},
    });
    const wrapper = mount(Provider({ store }, AccessibilityRowValue({
      member: { object: mockAccessible() },
    })));

    expect(wrapper.html()).toMatchSnapshot();
    const rowValue = wrapper.find(AccessibilityRowValueClass);
    expect(rowValue.children().length).toBe(1);
    const container = rowValue.childAt(0);
    expect(container.type()).toBe("span");
    expect(container.prop("role")).toBe("presentation");
    expect(container.children().length).toBe(1);
    expect(container.childAt(0).type()).toBe(Rep);
  });

  it("basic render", () => {
    const store = setupStore({
      preloadedState: { ui: { supports: { audit: true }}},
    });
    const wrapper = mount(Provider({ store }, AccessibilityRowValue({
      member: { object: mockAccessible() },
    })));

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
