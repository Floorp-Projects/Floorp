/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const { createFactory } = require("react");

const ManifestIconItem = createFactory(
  require("resource://devtools/client/application/src/components/manifest/ManifestIconItem.js")
);

/*
 * Unit tests for the ManifestIconItem component
 */

describe("ManifestIconItem", () => {
  it("renders the expected snapshot for a fully populated icon item", () => {
    const wrapper = shallow(
      ManifestIconItem({
        label: { sizes: "128x128", contentType: "image/png" },
        value: { src: "icon.png", purpose: "any" },
      })
    );
    expect(wrapper).toMatchSnapshot();
  });

  it("renders the expected snapshop when a label member is missing", () => {
    const wrapper = shallow(
      ManifestIconItem({
        label: { sizes: undefined, contentType: "image/png" },
        value: { src: "icon.png", purpose: "any" },
      })
    );
    expect(wrapper).toMatchSnapshot();
  });

  it("renders the expected snapshop when all label members are missing", () => {
    const wrapper = shallow(
      ManifestIconItem({
        label: { sizes: undefined, contentType: undefined },
        value: { src: "icon.png", purpose: "any" },
      })
    );
    expect(wrapper).toMatchSnapshot();
  });
});
