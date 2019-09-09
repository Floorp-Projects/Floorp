/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const { createFactory } = require("react");

const Sidebar = createFactory(
  require("devtools/client/application/src/components/routing/Sidebar")
);

/**
 * Test for Sidebar.js component
 */

describe("Sidebar", () => {
  it("renders the expected snapshot", () => {
    const wrapper = shallow(Sidebar());
    expect(wrapper).toMatchSnapshot();
  });
});
