/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Unit tests for the CompatibilityApp component.
 */

const { shallow } = require("enzyme");
const { createFactory } = require("devtools/client/shared/vendor/react");
const { thunk } = require("devtools/client/shared/redux/middleware/thunk.js");
const configureStore = require("redux-mock-store").default;

const CompatibilityApp = createFactory(
  require("devtools/client/inspector/compatibility/components/CompatibilityApp")
);

describe("CompatibilityApp component", () => {
  it("renders zero issues", () => {
    const mockStore = configureStore([thunk]);
    const store = mockStore({
      compatibility: {
        selectedNodeIssues: [],
        topLevelTargetIssues: [],
      },
    });
    const wrapper = shallow(CompatibilityApp({ store })).dive();
    expect(wrapper).toMatchSnapshot();
  });

  it("renders with settings", () => {
    const mockStore = configureStore([thunk]);
    const store = mockStore({
      compatibility: {
        isSettingsVisibile: true,
        selectedNodeIssues: [],
        topLevelTargetIssues: [],
      },
    });
    const wrapper = shallow(CompatibilityApp({ store })).dive();
    expect(wrapper).toMatchSnapshot();
  });
});
