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

const PageSwitcher = createFactory(
  require("devtools/client/application/src/components/routing/PageSwitcher")
);

const { PAGE_TYPES } = require("devtools/client/application/src/constants");

/**
 * Test for workerListEmpty.js component
 */

describe("PageSwitcher", () => {
  function buildStoreWithSelectedPage(selectedPage) {
    return setupStore({
      preloadedState: {
        ui: {
          selectedPage,
        },
      },
    });
  }

  const consoleErrorSpy = jest
    .spyOn(console, "error")
    .mockImplementation(() => {});

  beforeEach(() => {
    console.error.mockClear();
  });

  afterAll(() => {
    consoleErrorSpy.mockRestore();
  });

  it("renders the ManifestPage component when manifest page is selected", () => {
    const store = buildStoreWithSelectedPage(PAGE_TYPES.MANIFEST);
    const wrapper = shallow(PageSwitcher({ store })).dive();
    expect(wrapper).toMatchSnapshot();
  });

  it("renders the WorkersPage component when workers page is selected", () => {
    const store = buildStoreWithSelectedPage(PAGE_TYPES.SERVICE_WORKERS);
    const wrapper = shallow(PageSwitcher({ store })).dive();
    expect(wrapper).toMatchSnapshot();
  });

  it("renders nothing when no page is selected", () => {
    const store = buildStoreWithSelectedPage(null);
    const wrapper = shallow(PageSwitcher({ store })).dive();
    expect(console.error).toHaveBeenCalledTimes(1);
    expect(wrapper).toMatchSnapshot();
  });

  it("renders nothing when an invalid page is selected", () => {
    const store = buildStoreWithSelectedPage("foo");
    const wrapper = shallow(PageSwitcher({ store })).dive();
    expect(console.error).toHaveBeenCalledTimes(1);
    expect(wrapper).toMatchSnapshot();
  });
});
