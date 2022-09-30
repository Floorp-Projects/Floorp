/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const { createFactory } = require("react");

const ManifestUrlItem = createFactory(
  require("resource://devtools/client/application/src/components/manifest/ManifestUrlItem.js")
);

/*
 * Unit tests for the ManifestUrlItem component
 */

describe("ManifestUrlItem", () => {
  it("renders the expected snapshot for a populated url", () => {
    const wrapper = shallow(
      ManifestUrlItem({ label: "foo" }, "https://example.com")
    );
    expect(wrapper).toMatchSnapshot();
  });

  it("renders the expected snapshot for an empty url", () => {
    const wrapper = shallow(ManifestUrlItem({ label: "foo" }));
    expect(wrapper).toMatchSnapshot();
  });
});
