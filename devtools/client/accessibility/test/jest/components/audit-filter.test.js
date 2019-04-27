/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { mount } = require("enzyme");

const { createFactory } = require("devtools/client/shared/vendor/react");
const { span } = require("devtools/client/shared/vendor/react-dom-factories");
const Provider = createFactory(require("devtools/client/shared/vendor/react-redux").Provider);

const ConnectedAuditFilterClass =
  require("devtools/client/accessibility/components/AuditFilter");
const AuditFilterClass = ConnectedAuditFilterClass.WrappedComponent;
const AuditFilter = createFactory(ConnectedAuditFilterClass);
const {
  setupStore,
} = require("devtools/client/accessibility/test/jest/helpers");
const { FILTERS } = require("devtools/client/accessibility/constants");

describe("AuditController component:", () => {
  it("audit filter not filtered", () => {
    const store = setupStore();

    const wrapper = mount(Provider({store}, AuditFilter({}, span())));
    expect(wrapper.html()).toMatchSnapshot();

    const filter = wrapper.find(AuditFilterClass);
    expect(filter.children().length).toBe(1);
    expect(filter.childAt(0).is("span")).toBe(true);
  });

  it("audit filter filtered no checks", () => {
    const store = setupStore({
      preloadedState: { audit: { filters: { [FILTERS.CONTRAST]: true }}},
    });

    const wrapper = mount(Provider({store}, AuditFilter({}, span())));
    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.isEmptyRender()).toBe(true);
  });

  it("audit filter filtered unknown checks", () => {
    const store = setupStore({
      preloadedState: { audit: { filters: { tbd: true }}},
    });

    const wrapper = mount(Provider({store}, AuditFilter({}, span())));
    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.isEmptyRender()).toBe(true);
  });

  it("audit filter filtered contrast checks success", () => {
    const store = setupStore({
      preloadedState: { audit: { filters: { [FILTERS.CONTRAST]: true }}},
    });

    const wrapper = mount(Provider({store}, AuditFilter({
      checks: {
        "CONTRAST": {
          "value": 5.11,
          "color": [255, 0, 0, 1],
          "backgroundColor": [255, 255, 255, 1],
          "isLargeText": false,
          "score": "AA",
        },
      },
    }, span())));
    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.isEmptyRender()).toBe(true);
  });

  it("audit filter filtered contrast checks fail", () => {
    const store = setupStore({
      preloadedState: { audit: { filters: { [FILTERS.CONTRAST]: true }}},
    });

    const CONTRAST = {
      "value": 3.1,
      "color": [255, 0, 0, 1],
      "backgroundColor": [255, 255, 255, 1],
      "isLargeText": false,
      "score": "fail",
    };

    const wrapper = mount(Provider({store}, AuditFilter({
      checks: { CONTRAST },
    }, span())));
    expect(wrapper.html()).toMatchSnapshot();
    const filter = wrapper.find(AuditFilterClass);
    expect(filter.children().length).toBe(1);
    expect(filter.childAt(0).is("span")).toBe(true);
  });

  it("audit filter filtered contrast checks fail range", () => {
    const store = setupStore({
      preloadedState: { audit: { filters: { [FILTERS.CONTRAST]: true }}},
    });

    const CONTRAST = {
      "min": 1.19,
      "max": 1.39,
      "color": [128, 128, 128, 1],
      "backgroundColorMin": [219, 106, 116, 1],
      "backgroundColorMax": [156, 145, 211, 1],
      "isLargeText": false,
      "score": "fail",
    };

    const wrapper = mount(Provider({store}, AuditFilter({
      checks: { CONTRAST },
    }, span())));
    expect(wrapper.html()).toMatchSnapshot();
    const filter = wrapper.find(AuditFilterClass);
    expect(filter.children().length).toBe(1);
    expect(filter.childAt(0).is("span")).toBe(true);
  });
});
