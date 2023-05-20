/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Unit tests for the IssuePane component.
 */

const { shallow } = require("enzyme");
const React = require("react");

const {
  COMPATIBILITY_ISSUE_TYPE,
} = require("resource://devtools/shared/constants.js");
const IssuePane = React.createFactory(
  require("resource://devtools/client/inspector/compatibility/components/IssuePane.js")
);

describe("IssuePane component", () => {
  it("renders no issues", () => {
    const targetComponent = shallow(IssuePane({ issues: [] }));
    expect(targetComponent).toMatchSnapshot();
  });

  it("renders some issues", () => {
    const targetComponent = shallow(
      IssuePane({
        issues: [
          {
            type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY,
            property: "border-block-color",
            url: "https://developer.mozilla.org/docs/Web/CSS/border-block-color",
            deprecated: false,
            experimental: true,
            unsupportedBrowsers: [],
          },
          {
            type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY_ALIASES,
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
    expect(targetComponent).toMatchSnapshot();
  });
});
