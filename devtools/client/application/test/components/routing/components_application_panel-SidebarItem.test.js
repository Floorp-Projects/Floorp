/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const { createFactory } = require("react");

const SidebarItem = createFactory(
  require("devtools/client/application/src/components/routing/SidebarItem")
);

/**
 * Test for SidebarItem.js component
 */

describe("SidebarItem", () => {
  it("renders the expected snapshot with manifest page", () => {
    const wrapper = shallow(
      SidebarItem({
        page: "manifest",
      })
    );
    expect(wrapper).toMatchSnapshot();
  });

  it("renders the expected snapshot with service-workers page", () => {
    const wrapper = shallow(
      SidebarItem({
        page: "service-workers",
      })
    );
    expect(wrapper).toMatchSnapshot();
  });
});
