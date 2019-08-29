/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const { createFactory } = require("react");

const Manifest = createFactory(
  require("devtools/client/application/src/components/manifest/Manifest")
);

const {
  MANIFEST_NO_ISSUES,
  MANIFEST_SIMPLE,
} = require("../fixtures/data/constants");

/*
 * Test for Manifest component
 */

describe("Manifest", () => {
  it("renders the expected snapshot for a simple manifest", () => {
    const wrapper = shallow(Manifest(MANIFEST_SIMPLE));
    expect(wrapper).toMatchSnapshot();
  });

  it("does not render the issues section when the manifest is valid", () => {
    const wrapper = shallow(Manifest(MANIFEST_NO_ISSUES));
    expect(wrapper).toMatchSnapshot();

    const sections = wrapper.find("ManifestSection");
    expect(sections).toHaveLength(3);
    expect(sections.get(0).props.title).not.toBe("manifest-item-warnings");
    expect(sections.contains("ManifestIssueList")).toBe(false);
  });
});
