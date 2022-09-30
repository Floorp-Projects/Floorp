/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Unit tests for the Footer component.
 */

const { shallow } = require("enzyme");
const React = require("react");
const {
  thunk,
} = require("resource://devtools/client/shared/redux/middleware/thunk.js");
const configureStore = require("redux-mock-store").default;

const Footer = React.createFactory(
  require("resource://devtools/client/inspector/compatibility/components/Footer.js")
);

describe("Footer component", () => {
  it("renders", () => {
    const mockStore = configureStore([thunk()]);
    const store = mockStore({});
    const connectWrapper = shallow(Footer({ store }));
    const targetComponent = connectWrapper.dive();
    expect(targetComponent).toMatchSnapshot();
  });
});
