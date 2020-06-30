/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const { createFactory } = require("react");

const ManifestColorItem = createFactory(
  require("devtools/client/application/src/components/manifest/ManifestColorItem")
);

/*
 * Unit tests for the ManifestItem component
 */

describe("ManifestColorItem", () => {
  it("renders the expected snapshot for a populated color item", () => {
    const wrapper = shallow(
      ManifestColorItem({ label: "foo", value: "#ff0000" })
    );
    expect(wrapper).toMatchSnapshot();
  });

  it("renders the expected snapshot for an empty color item", () => {
    const wrapper = shallow(ManifestColorItem({ label: "foo" }));
    expect(wrapper).toMatchSnapshot();
  });
});
