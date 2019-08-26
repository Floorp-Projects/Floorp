/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const { createFactory } = require("react");

const ManifestSection = createFactory(
  require("devtools/client/application/src/components/manifest/ManifestItem")
);

/*
 * Unit tests for the ManifestItem component
 */

describe("ManifestItem", () => {
  it("renders the expected snapshot for a populated item", () => {
    const wrapper = shallow(ManifestSection({ label: "foo" }, "bar"));
    expect(wrapper).toMatchSnapshot();
  });

  it("renders the expected snapshot for an empty item", () => {
    const wrapper = shallow(ManifestSection({ label: "foo" }));
    expect(wrapper).toMatchSnapshot();
  });
});
