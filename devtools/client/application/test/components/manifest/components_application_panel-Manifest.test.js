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
  MANIFEST_COLOR_MEMBERS,
  MANIFEST_STRING_MEMBERS,
  MANIFEST_UNKNOWN_TYPE_MEMBERS,
  MANIFEST_NO_ISSUES,
  MANIFEST_WITH_ISSUES,
} = require("../fixtures/data/constants");

/*
 * Test for Manifest component
 */

describe("Manifest", () => {
  it("renders the expected snapshot for a manifest with string members", () => {
    const wrapper = shallow(Manifest(MANIFEST_STRING_MEMBERS));
    expect(wrapper).toMatchSnapshot();
  });

  it("renders the expected snapshot for a manifest with color members", () => {
    const wrapper = shallow(Manifest(MANIFEST_COLOR_MEMBERS));
    expect(wrapper).toMatchSnapshot();
  });

  it("renders the expected snapshot for a manifest with unknown types", () => {
    const wrapper = shallow(Manifest(MANIFEST_UNKNOWN_TYPE_MEMBERS));
    expect(wrapper).toMatchSnapshot();
  });

  it("does render the issues section when the manifest is not valid", () => {
    const wrapper = shallow(Manifest(MANIFEST_WITH_ISSUES));
    expect(wrapper).toMatchSnapshot();

    const sections = wrapper.find("ManifestSection");
    expect(sections).toHaveLength(4);
    expect(sections.get(0).props.title).toBe("manifest-item-warnings");
    expect(sections.find("ManifestIssueList")).toHaveLength(1);
  });

  it("does not render the issues section when the manifest is valid", () => {
    const wrapper = shallow(Manifest(MANIFEST_NO_ISSUES));
    expect(wrapper).toMatchSnapshot();

    const sections = wrapper.find("ManifestSection");
    expect(sections).toHaveLength(3);
    expect(sections.get(0).props.title).not.toBe("manifest-item-warnings");
    expect(sections.find("ManifestIssueList")).toHaveLength(0);
  });
});
