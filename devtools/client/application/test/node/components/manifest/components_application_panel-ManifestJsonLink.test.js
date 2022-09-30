/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const { createFactory } = require("react");

const ManifestJsonLink = createFactory(
  require("resource://devtools/client/application/src/components/manifest/ManifestJsonLink.js")
);

/*
 * Test for the ManifestJsonLink component
 */

describe("ManifestJsonLink", () => {
  it("renders the expected snapshot when given a regular URL", () => {
    const wrapper = shallow(
      ManifestJsonLink({ url: "https://example.com/manifest.json" })
    );
    expect(wrapper).toMatchSnapshot();
  });

  it("renders the expected snapshot when given a data URL", () => {
    const wrapper = shallow(
      ManifestJsonLink({
        url: `data:application/manifest+json,{"name": "Foo"}`,
      })
    );
    expect(wrapper).toMatchSnapshot();
    // assert there's no link for data URLs
    expect(wrapper.find("a").length).toBe(0);
  });
});
