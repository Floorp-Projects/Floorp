/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const { createFactory } = require("react");

// Import & init localization
const FluentReact = require("resource://devtools/client/shared/vendor/fluent-react.js");
const LocalizationProvider = createFactory(FluentReact.LocalizationProvider);

// Import component
const App = createFactory(
  require("resource://devtools/client/application/src/components/App.js")
);

describe("App", () => {
  it("renders the expected snapshot", () => {
    const wrapper = shallow(
      LocalizationProvider({ bundles: [] }, App({}))
    ).dive(); // dive to bypass the LocalizationProvider wrapper
    expect(wrapper).toMatchSnapshot();
  });
});
