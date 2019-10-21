/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const { createFactory } = require("react");
const { td, tr } = require("devtools/client/shared/vendor/react-dom-factories");

const ManifestSection = createFactory(
  require("devtools/client/application/src/components/manifest/ManifestSection")
);

/*
 * Unit tests for the ManifestSection component
 */

describe("ManifestSection", () => {
  it("renders the expected snapshot for a populated section", () => {
    const content = tr({}, td({}, "foo"));
    const wrapper = shallow(ManifestSection({ title: "Lorem ipsum" }, content));
    expect(wrapper).toMatchSnapshot();
  });

  it("renders the expected snapshot for a section with no children", () => {
    const wrapper = shallow(ManifestSection({ title: "Lorem ipsum" }));
    expect(wrapper).toMatchSnapshot();
    expect(wrapper.find(".manifest-section--empty"));
  });
});
