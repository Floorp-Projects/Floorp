/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Unit tests for the IssueItem component.
 */

const { shallow } = require("enzyme");
const React = require("react");

const MDNCompatibility = require("devtools/client/inspector/compatibility/lib/MDNCompatibility");
const IssueItem = React.createFactory(
  require("devtools/client/inspector/compatibility/components/IssueItem")
);

describe("IssueItem component", () => {
  it("renders an issue of CSS property", () => {
    const item = shallow(
      IssueItem({
        type: MDNCompatibility.ISSUE_TYPE.CSS_PROPERTY,
        property: "test-property",
        url: "test-url",
        deprecated: false,
        experimental: false,
        unsupportedBrowsers: [],
      })
    );
    expect(item).toMatchSnapshot();
  });
});
