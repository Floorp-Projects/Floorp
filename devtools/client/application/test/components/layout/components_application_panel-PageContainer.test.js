/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const { createFactory } = require("react");

// Import setupStore with imported & combined reducers
const {
  setupStore,
} = require("devtools/client/application/test/components/helpers/helpers");

const PageContainer = createFactory(
  require("devtools/client/application/src/components/layout/PageContainer")
);

const { PAGE_TYPES } = require("devtools/client/application/src/constants");

/**
 * Test for workerListEmpty.js component
 */

describe("PageContainer", () => {
  function buildStoreWithSelectedPage(selectedPage) {
    return setupStore({
      preloadedState: {
        ui: {
          selectedPage,
        },
      },
    });
  }

  it("renders the WorkersPage component when workers page is selected", () => {
    const store = buildStoreWithSelectedPage(PAGE_TYPES.SERVICE_WORKERS);
    const wrapper = shallow(PageContainer({ store })).dive();
    expect(wrapper).toMatchSnapshot();
  });

  it("renders nothing when no page is selected", () => {
    const store = buildStoreWithSelectedPage(null);
    const wrapper = shallow(PageContainer({ store })).dive();
    expect(wrapper).toMatchSnapshot();
  });

  it("renders nothing when an invalid page is selected", () => {
    const store = buildStoreWithSelectedPage("foo");
    const wrapper = shallow(PageContainer({ store })).dive();
    expect(wrapper).toMatchSnapshot();
  });
});
