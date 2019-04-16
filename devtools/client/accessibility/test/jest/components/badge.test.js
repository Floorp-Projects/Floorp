/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { mount } = require("enzyme");

const { createFactory } = require("devtools/client/shared/vendor/react");

const { ToggleButton } = require("devtools/client/accessibility/components/Button");
const Badge = createFactory(require("devtools/client/accessibility/components/Badge"));

describe("Badge component:", () => {
  const props = {
    label: "Contrast",
  };

  it("basic render", () => {
    const wrapper = mount(Badge(props));
    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.children().length).toBe(1);

    const toggleButton = wrapper.childAt(0);
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
});
