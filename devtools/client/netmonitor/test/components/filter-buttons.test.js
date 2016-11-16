/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-env node, mocha */
"use strict";

const expect = require("expect");
const { mount } = require("enzyme");
const { createFactory } = require("devtools/client/shared/vendor/react");
const { configureStore } = require("devtools/client/netmonitor/store");
const Provider = createFactory(require("devtools/client/shared/vendor/react-redux").Provider);
const Actions = require("devtools/client/netmonitor/actions/index");
const FilterButtons = createFactory(require("devtools/client/netmonitor/components/filter-buttons"));

const expectDefaultTypes = {
  all: true,
  html: false,
  css: false,
  js: false,
  xhr: false,
  fonts: false,
  images: false,
  media: false,
  flash: false,
  ws: false,
  other: false,
};

// unit test
describe("FilterButtons component:", () => {
  const store = configureStore();
  const wrapper = mount(FilterButtons({ store }));

  asExpected(wrapper, expectDefaultTypes, "by default");
});

// integration test with redux store, action, reducer
describe("FilterButtons::enableFilterOnly:", () => {
  const expectXHRTypes = {
    all: false,
    html: false,
    css: false,
    js: false,
    xhr: true,
    fonts: false,
    images: false,
    media: false,
    flash: false,
    ws: false,
    other: false,
  };

  const store = configureStore();
  const wrapper = mount(Provider(
    { store },
    FilterButtons()
  ));

  store.dispatch(Actions.enableFilterTypeOnly("xhr"));
  asExpected(wrapper, expectXHRTypes, `when enableFilterOnly("xhr") is called`);
});

// integration test with redux store, action, reducer
describe("FilterButtons::toggleFilter:", () => {
  const expectXHRJSTypes = {
    all: false,
    html: false,
    css: false,
    js: true,
    xhr: true,
    fonts: false,
    images: false,
    media: false,
    flash: false,
    ws: false,
    other: false,
  };

  const store = configureStore();
  const wrapper = mount(Provider(
    { store },
    FilterButtons()
  ));

  store.dispatch(Actions.toggleFilterType("xhr"));
  store.dispatch(Actions.toggleFilterType("js"));
  asExpected(wrapper, expectXHRJSTypes, `when xhr, js is toggled`);
});

function asExpected(wrapper, expectTypes, description) {
  for (let type of Object.keys(expectTypes)) {
    let checked = expectTypes[type] ? "checked" : "not checked";
    let className = expectTypes[type] ?
        "menu-filter-button checked": "menu-filter-button";
    it(`'${type}' button is ${checked} ${description}`, () => {
      expect(wrapper.find(`#requests-menu-filter-${type}-button`).html())
      .toBe(`<button id="requests-menu-filter-${type}-button" class="` + className +
            `" data-key="${type}">netmonitor.toolbar.filter.${type}</button>`);
    });
  }
}
