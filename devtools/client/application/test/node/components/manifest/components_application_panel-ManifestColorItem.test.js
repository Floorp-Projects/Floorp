/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const { createFactory } = require("react");

const ManifestColorItem = createFactory(
  require("resource://devtools/client/application/src/components/manifest/ManifestColorItem.js")
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

  it("strips opaque alpha from the displayed color", () => {
    const wrapper = shallow(
      ManifestColorItem({ label: "foo", value: "#00FF00FF" })
    );
    expect(wrapper).toMatchSnapshot();

    expect(wrapper.find(".manifest-item__color").text()).toBe("#00FF00");
  });

  it("does not strip translucent alpha from the displayed color", () => {
    const wrapper = shallow(
      ManifestColorItem({ label: "foo", value: "#00FF00FA" })
    );
    expect(wrapper).toMatchSnapshot();

    expect(wrapper.find(".manifest-item__color").text()).toBe("#00FF00FA");
  });
});
