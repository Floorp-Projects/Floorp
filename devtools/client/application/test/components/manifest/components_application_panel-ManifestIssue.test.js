/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const { createFactory } = require("react");

const ManifestIssue = createFactory(
  require("devtools/client/application/src/components/manifest/ManifestIssue")
);

/*
 * Tests for the ManifestIssue component
 */

describe("ManifestIssue", () => {
  it("renders the expected snapshot for a warning", () => {
    const issue = { level: "warning", message: "Lorem ipsum" };
    const wrapper = shallow(ManifestIssue(issue));
    expect(wrapper).toMatchSnapshot();
  });

  it("renders the expected snapshot for an error", () => {
    const issue = { level: "error", message: "Lorem ipsum" };
    const wrapper = shallow(ManifestIssue(issue));
    expect(wrapper).toMatchSnapshot();
  });
});
