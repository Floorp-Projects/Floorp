/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Unit tests for the Settings component.
 */

const { shallow } = require("enzyme");
const { createFactory } = require("devtools/client/shared/vendor/react");
const { thunk } = require("devtools/client/shared/redux/middleware/thunk.js");
const configureStore = require("redux-mock-store").default;

const Settings = createFactory(
  require("devtools/client/inspector/compatibility/components/Settings")
);

const DEFAULT_BROWSERS = [
  { id: "firefox", name: "Firefox", status: "nightly", version: "78" },
  { id: "firefox", name: "Firefox", status: "beta", version: "77" },
  { id: "firefox", name: "Firefox", status: "current", version: "76" },
];

describe("Settings component", () => {
  it("renders default browsers with no selected browsers", () => {
    const mockStore = configureStore([thunk]);
    const store = mockStore({
      compatibility: {
        defaultTargetBrowsers: DEFAULT_BROWSERS,
        targetBrowsers: [],
      },
    });
    const wrapper = shallow(Settings({ store })).dive();
    expect(wrapper).toMatchSnapshot();
  });

  it("renders default browsers with a selected browsers", () => {
    const mockStore = configureStore([thunk]);
    const store = mockStore({
      compatibility: {
        defaultTargetBrowsers: DEFAULT_BROWSERS,
        targetBrowsers: [DEFAULT_BROWSERS[1]],
      },
    });
    const wrapper = shallow(Settings({ store })).dive();
    expect(wrapper).toMatchSnapshot();
  });

  it("renders default browsers with full selected browsers", () => {
    const mockStore = configureStore([thunk]);
    const store = mockStore({
      compatibility: {
        defaultTargetBrowsers: DEFAULT_BROWSERS,
        targetBrowsers: DEFAULT_BROWSERS,
      },
    });
    const wrapper = shallow(Settings({ store })).dive();
    expect(wrapper).toMatchSnapshot();
  });
});
