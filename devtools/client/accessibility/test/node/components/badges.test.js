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

const Badge = require("resource://devtools/client/accessibility/components/Badge.js");
const Badges = createFactory(
  require("resource://devtools/client/accessibility/components/Badges.js")
);
const ContrastBadge = require("resource://devtools/client/accessibility/components/ContrastBadge.js");

const {
  accessibility: { SCORES },
} = require("resource://devtools/shared/constants.js");

describe("Badges component:", () => {
  const store = setupStore();

  it("no props render", () => {
    const wrapper = mount(Provider({ store }, Badges()));
    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.isEmptyRender()).toBe(true);
  });

  it("null checks render", () => {
    const wrapper = mount(Provider({ store }, Badges({ checks: null })));
    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.isEmptyRender()).toBe(true);
  });

  it("empty checks render", () => {
    const wrapper = mount(Provider({ store }, Badges({ checks: {} })));
    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.isEmptyRender()).toBe(true);
  });

  it("unsupported checks render", () => {
    const wrapper = mount(Provider({ store }, Badges({ checks: { tbd: {} } })));
    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.isEmptyRender()).toBe(true);
  });

  it("contrast ratio success render", () => {
    const wrapper = mount(
      Provider(
        { store },
        Badges({
          checks: {
            CONTRAST: {
              value: 5.11,
              color: [255, 0, 0, 1],
              backgroundColor: [255, 255, 255, 1],
              isLargeText: false,
              score: SCORES.AA,
            },
          },
        })
      )
    );
    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.isEmptyRender()).toBe(true);
  });

  it("contrast ratio fail render", () => {
    const CONTRAST = {
      value: 3.1,
      color: [255, 0, 0, 1],
      backgroundColor: [255, 255, 255, 1],
      isLargeText: false,
      score: SCORES.FAIL,
    };
    const wrapper = mount(
      Provider({ store }, Badges({ checks: { CONTRAST } }))
    );

    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.find(Badge).length).toBe(1);
    expect(wrapper.find(ContrastBadge).length).toBe(1);
    expect(wrapper.find(ContrastBadge).first().props()).toMatchObject(CONTRAST);
  });

  it("contrast ratio fail range render", () => {
    const CONTRAST = {
      min: 1.19,
      max: 1.39,
      color: [128, 128, 128, 1],
      backgroundColorMin: [219, 106, 116, 1],
      backgroundColorMax: [156, 145, 211, 1],
      isLargeText: false,
      score: SCORES.FAIL,
    };
    const wrapper = mount(
      Provider({ store }, Badges({ checks: { CONTRAST } }))
    );

    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.find(Badge).length).toBe(1);
    expect(wrapper.find(ContrastBadge).length).toBe(1);
    expect(wrapper.find(ContrastBadge).first().props()).toMatchObject(CONTRAST);
  });
});
