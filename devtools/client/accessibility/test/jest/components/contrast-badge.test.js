/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { shallow, mount } = require("enzyme");

const { createFactory } = require("devtools/client/shared/vendor/react");

const Badge = require("devtools/client/accessibility/components/Badge");
const ContrastBadge = createFactory(require("devtools/client/accessibility/components/ContrastBadge"));

describe("ContrastBadge component:", () => {
  it("error render", () => {
    const wrapper = shallow(ContrastBadge({ error: true }));
    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.isEmptyRender()).toBe(true);
  });

  it("success render", () => {
    const wrapper = shallow(ContrastBadge({
      value: 5.11,
      isLargeText: false,
    }));
    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.isEmptyRender()).toBe(true);
  });

  it("success range render", () => {
    const wrapper = shallow(ContrastBadge({
      min: 5.11,
      max: 6.25,
      isLargeText: false,
    }));
    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.isEmptyRender()).toBe(true);
  });

  it("success large text render", () => {
    const wrapper = shallow(ContrastBadge({
      value: 3.77,
      isLargeText: true,
    }));
    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.isEmptyRender()).toBe(true);
  });

  it("fail render", () => {
    const wrapper = mount(ContrastBadge({
      value: 3.77,
      isLargeText: false,
    }));

    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.children().length).toBe(1);
    const badge = wrapper.childAt(0);
    expect(badge.type()).toBe(Badge);
    expect(badge.props()).toMatchObject({
      label: "accessibility.badge.contrast",
    });
  });
});
