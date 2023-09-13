/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Unit tests for the CompatibilityApp component.
 */

const { shallow } = require("enzyme");
const {
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const {
  thunk,
} = require("resource://devtools/client/shared/redux/middleware/thunk.js");
const configureStore = require("redux-mock-store").default;

const CompatibilityApp = createFactory(
  require("resource://devtools/client/inspector/compatibility/components/CompatibilityApp.js")
);

describe("CompatibilityApp component", () => {
  it("renders zero issues", () => {
    const mockStore = configureStore([thunk()]);
    const store = mockStore({
      compatibility: {
        selectedNodeIssues: [],
        topLevelTargetIssues: [],
      },
    });

    const withLocalizationWrapper = shallow(CompatibilityApp({ store }));
    const connectWrapper = withLocalizationWrapper.dive();
    const targetComponent = connectWrapper.dive();
    expect(targetComponent).toMatchSnapshot();
  });

  it("renders with settings", () => {
    const mockStore = configureStore([thunk()]);
    const store = mockStore({
      compatibility: {
        isSettingsVisible: true,
        selectedNodeIssues: [],
        topLevelTargetIssues: [],
      },
    });

    const withLocalizationWrapper = shallow(CompatibilityApp({ store }));
    const connectWrapper = withLocalizationWrapper.dive();
    const targetComponent = connectWrapper.dive();
    expect(targetComponent).toMatchSnapshot();
  });
});
