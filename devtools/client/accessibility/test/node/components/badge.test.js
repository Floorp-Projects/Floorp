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
  setupStore,
} = require("resource://devtools/client/accessibility/test/node/helpers.js");

const BadgeClass = require("resource://devtools/client/accessibility/components/Badge.js");
const Badge = createFactory(BadgeClass);

describe("Badge component:", () => {
  const label = "Contrast";
  const tooltip = "Does not meet WCAG standards for accessible text.";
  const props = { label, tooltip };

  it("basic render", () => {
    const store = setupStore();
    const wrapper = mount(Provider({ store }, Badge(props)));
    expect(wrapper.html()).toMatchSnapshot();

    const badge = wrapper.find(BadgeClass);
    expect(badge.children().length).toBe(1);
    expect(
      badge.find(`span[aria-label="${label}"][title="${tooltip}"]`)
    ).toHaveLength(1);

    const badgeText = badge.childAt(0);
    expect(badgeText.hasClass("audit-badge")).toBe(true);
    expect(badgeText.hasClass("badge")).toBe(true);
    expect(badgeText.text()).toBe(label);
  });
});
