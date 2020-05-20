/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Unit tests for the IssueList component.
 */

const { shallow } = require("enzyme");
const React = require("react");

const MDNCompatibility = require("devtools/shared/compatibility/MDNCompatibility");
const IssueList = React.createFactory(
  require("devtools/client/inspector/compatibility/components/IssueList")
);

describe("IssueList component", () => {
  it("renders some issues", () => {
    const list = shallow(
      IssueList({
        issues: [
          {
            type: MDNCompatibility.ISSUE_TYPE.CSS_PROPERTY,
            property: "border-block-color",
            url:
              "https://developer.mozilla.org/docs/Web/CSS/border-block-color",
            deprecated: false,
            experimental: true,
            unsupportedBrowsers: [],
          },
          {
            type: MDNCompatibility.ISSUE_TYPE.CSS_PROPERTY_ALIASES,
            property: "user-modify",
            url: "https://developer.mozilla.org/docs/Web/CSS/user-modify",
            aliases: ["user-modify"],
            deprecated: true,
            experimental: false,
            unsupportedBrowsers: [],
          },
        ],
      })
    );
    expect(list).toMatchSnapshot();
  });
});
