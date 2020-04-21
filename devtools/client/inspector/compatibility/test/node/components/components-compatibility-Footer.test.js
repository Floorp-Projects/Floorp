/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Unit tests for the Footer component.
 */

const { shallow } = require("enzyme");
const React = require("react");

const Footer = React.createFactory(
  require("devtools/client/inspector/compatibility/components/Footer")
);

describe("Footer component", () => {
  it("renders", () => {
    const pane = shallow(Footer());
    expect(pane).toMatchSnapshot();
  });
});
