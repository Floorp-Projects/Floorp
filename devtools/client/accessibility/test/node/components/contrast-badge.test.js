/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { shallow, mount } = require("enzyme");

const {
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");

const Provider = createFactory(
  require("resource://devtools/client/shared/vendor/react-redux.js").Provider
);
const {
  setupStore,
} = require("resource://devtools/client/accessibility/test/node/helpers.js");

const Badge = require("resource://devtools/client/accessibility/components/Badge.js");
const ContrastBadgeClass = require("resource://devtools/client/accessibility/components/ContrastBadge.js");
const ContrastBadge = createFactory(ContrastBadgeClass);

const {
  accessibility: { SCORES },
} = require("resource://devtools/shared/constants.js");

describe("ContrastBadge component:", () => {
  const store = setupStore();

  it("error render", () => {
    const wrapper = shallow(ContrastBadge({ error: true }));
    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.isEmptyRender()).toBe(true);
  });

  it("success render", () => {
    const wrapper = shallow(
      ContrastBadge({
        value: 5.11,
        isLargeText: false,
        score: SCORES.AA,
      })
    );
    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.isEmptyRender()).toBe(true);
  });

  it("success range render", () => {
    const wrapper = shallow(
      ContrastBadge({
        min: 5.11,
        max: 6.25,
        isLargeText: false,
        score: SCORES.AA,
      })
    );
    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.isEmptyRender()).toBe(true);
  });

  it("success large text render", () => {
    const wrapper = shallow(
      ContrastBadge({
        value: 3.77,
        isLargeText: true,
        score: SCORES.AA,
      })
    );
    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.isEmptyRender()).toBe(true);
  });

  it("fail render", () => {
    const wrapper = mount(
      Provider(
        { store },
        ContrastBadge({
          value: 3.77,
          isLargeText: false,
          score: SCORES.FAIL,
        })
      )
    );

    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.children().length).toBe(1);
    const contrastBadge = wrapper.find(ContrastBadgeClass);
    const badge = contrastBadge.childAt(0);
    expect(badge.type()).toBe(Badge);
    expect(badge.props()).toMatchObject({
      label: "contrast",
      tooltip: "Does not meet WCAG standards for accessible text.",
    });
  });
});
