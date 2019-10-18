/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const { createFactory } = require("react");

const {
  setupStore,
} = require("devtools/client/application/test/components/helpers/helpers");

const { PAGE_TYPES } = require("devtools/client/application/src/constants");

const Sidebar = createFactory(
  require("devtools/client/application/src/components/routing/Sidebar")
);

/**
 * Test for Sidebar.js component
 */

describe("Sidebar", () => {
  function buildStoreWithSelectedPage(selectedPage) {
    return setupStore({
      ui: {
        selectedPage,
      },
    });
  }
  it("renders the expected snapshot when the manifest page is selected", () => {
    const store = buildStoreWithSelectedPage(PAGE_TYPES.MANIFEST);
    const wrapper = shallow(Sidebar({ store })).dive();
    expect(wrapper).toMatchSnapshot();
  });

  it("renders the expected snapshot when the service workers page is selected", () => {
    const store = buildStoreWithSelectedPage(PAGE_TYPES.SERVICE_WORKERS);
    const wrapper = shallow(Sidebar({ store })).dive();
    expect(wrapper).toMatchSnapshot();
  });

  it("renders the expected snapshot when no page is selected", () => {
    const store = buildStoreWithSelectedPage();
    const wrapper = shallow(Sidebar({ store })).dive();
    expect(wrapper).toMatchSnapshot();
  });
});
