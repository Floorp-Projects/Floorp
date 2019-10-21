/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const { createFactory } = require("react");

const ManifestIssueList = createFactory(
  require("devtools/client/application/src/components/manifest/ManifestIssueList")
);

/*
 * Tests for the ManifestIssue component
 */

describe("ManifestIssueList", () => {
  it("renders the expected snapshot for a populated list", () => {
    const issues = [
      { level: "error", message: "Foo" },
      { level: "warning", message: "Foo" },
      { level: "warning", message: "Bar" },
    ];
    const wrapper = shallow(ManifestIssueList({ issues }));
    expect(wrapper).toMatchSnapshot();
  });

  it("groups issues by level and shows errors first", () => {
    const issues = [
      { level: "warning", message: "A warning" },
      { level: "error", message: "An error" },
      { level: "warning", message: "Another warning" },
    ];
    const wrapper = shallow(ManifestIssueList({ issues }));
    expect(wrapper).toMatchSnapshot();

    expect(wrapper.find("ManifestIssue").get(0).props.level).toBe("error");
    expect(wrapper.find("ManifestIssue").get(1).props.level).toBe("warning");
    expect(wrapper.find("ManifestIssue").get(2).props.level).toBe("warning");
  });

  it("skips rendering empty level groups", () => {
    const issues = [
      { level: "warning", message: "A warning" },
      { level: "warning", message: "Another warning" },
    ];
    const wrapper = shallow(ManifestIssueList({ issues }));
    expect(wrapper).toMatchSnapshot();

    const lists = wrapper.find(".js-manifest-issues");
    expect(lists).toHaveLength(1);
  });

  it("renders nothing for empty issues", () => {
    const wrapper = shallow(ManifestIssueList({ issues: [] }));
    expect(wrapper).toMatchSnapshot();
  });
});
