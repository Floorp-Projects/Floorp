/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global L10N */

const { mount } = require("enzyme");

const { createFactory } = require("devtools/client/shared/vendor/react");
const Provider = createFactory(require("devtools/client/shared/vendor/react-redux").Provider);
const { setupStore } = require("devtools/client/accessibility/test/jest/helpers");

const Badge = require("devtools/client/accessibility/components/Badge");
const Badges = createFactory(require("devtools/client/accessibility/components/Badges"));
const ContrastBadge = require("devtools/client/accessibility/components/ContrastBadge");

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
    const wrapper = mount(Provider({ store }, Badges({
      checks: {
        "CONTRAST": {
          "value": 5.11,
          "color": [255, 0, 0, 1],
          "backgroundColor": [255, 255, 255, 1],
          "isLargeText": false,
          "score": "AA",
        },
      },
    })));
    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.isEmptyRender()).toBe(true);
  });

  it("contrast ratio fail render", () => {
    const CONTRAST = {
      "value": 3.1,
      "color": [255, 0, 0, 1],
      "backgroundColor": [255, 255, 255, 1],
      "isLargeText": false,
      "score": "fail",
    };
    const wrapper = mount(Provider({ store }, Badges({ checks: { CONTRAST }})));

    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.find(Badge).length).toBe(1);
    expect(wrapper.find(ContrastBadge).length).toBe(1);
    expect(wrapper.find(ContrastBadge).first().props()).toMatchObject(CONTRAST);
  });

  it("contrast ratio fail range render", () => {
    const CONTRAST = {
      "min": 1.19,
      "max": 1.39,
      "color": [128, 128, 128, 1],
      "backgroundColorMin": [219, 106, 116, 1],
      "backgroundColorMax": [156, 145, 211, 1],
      "isLargeText": false,
      "score": "fail",
    };
    const wrapper = mount(Provider({ store }, Badges({ checks: { CONTRAST }})));

    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.find(Badge).length).toBe(1);
    expect(wrapper.find(ContrastBadge).length).toBe(1);
    expect(wrapper.find(ContrastBadge).first().props()).toMatchObject(CONTRAST);
  });
});
