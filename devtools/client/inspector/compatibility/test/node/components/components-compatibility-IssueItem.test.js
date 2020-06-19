/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Unit tests for the IssueItem component.
 */

const { shallow } = require("enzyme");
const React = require("react");

const MDNCompatibility = require("devtools/shared/compatibility/MDNCompatibility");
const IssueItem = React.createFactory(
  require("devtools/client/inspector/compatibility/components/IssueItem")
);

describe("IssueItem component", () => {
  it("renders an unsupported issue of CSS property", () => {
    const targetComponent = shallow(
      IssueItem({
        type: MDNCompatibility.ISSUE_TYPE.CSS_PROPERTY,
        property: "test-property",
        url: "test-url",
        deprecated: false,
        experimental: false,
        unsupportedBrowsers: [
          { id: "firefox", name: "Firefox", version: "70", status: "nightly" },
        ],
      })
    );
    expect(targetComponent).toMatchSnapshot();
  });

  it("renders a deprecated issue of CSS property", () => {
    const targetComponent = shallow(
      IssueItem({
        type: MDNCompatibility.ISSUE_TYPE.CSS_PROPERTY,
        property: "test-property",
        url: "test-url",
        deprecated: true,
        experimental: false,
        unsupportedBrowsers: [],
      })
    );
    expect(targetComponent).toMatchSnapshot();
  });

  it("renders an experimental issue of CSS property", () => {
    const targetComponent = shallow(
      IssueItem({
        type: MDNCompatibility.ISSUE_TYPE.CSS_PROPERTY,
        property: "test-property",
        url: "test-url",
        deprecated: false,
        experimental: true,
        unsupportedBrowsers: [],
      })
    );
    expect(targetComponent).toMatchSnapshot();
  });

  it("renders an issue which has both deprecated and experimental", () => {
    const targetComponent = shallow(
      IssueItem({
        type: MDNCompatibility.ISSUE_TYPE.CSS_PROPERTY,
        property: "test-property",
        url: "test-url",
        deprecated: true,
        experimental: true,
        unsupportedBrowsers: [],
      })
    );
    expect(targetComponent).toMatchSnapshot();
  });

  it("renders an issue which has nodes that caused this issue", () => {
    const targetComponent = shallow(
      IssueItem({
        type: MDNCompatibility.ISSUE_TYPE.CSS_PROPERTY,
        property: "test-property",
        url: "test-url",
        unsupportedBrowsers: [],
        nodes: [
          {
            actorID: "test-actor",
            attributes: [],
            nodeName: "test-element",
            nodeType: 1,
          },
        ],
      })
    );
    expect(targetComponent).toMatchSnapshot();
  });
});
