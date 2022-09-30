/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Unit tests for the IssueItem component.
 */

const { shallow } = require("enzyme");
const React = require("react");

const {
  COMPATIBILITY_ISSUE_TYPE,
} = require("resource://devtools/shared/constants.js");
const IssueItem = React.createFactory(
  require("resource://devtools/client/inspector/compatibility/components/IssueItem.js")
);

describe("IssueItem component", () => {
  it("renders an unsupported issue of CSS property", () => {
    const targetComponent = shallow(
      IssueItem({
        type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY,
        property: "test-property",
        url: "test-url",
        deprecated: false,
        experimental: false,
        prefixNeeded: false,
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
        type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY,
        property: "test-property",
        url: "test-url",
        deprecated: true,
        experimental: false,
        prefixNeeded: false,
        unsupportedBrowsers: [],
      })
    );
    expect(targetComponent).toMatchSnapshot();
  });

  it("renders an experimental issue of CSS property", () => {
    const targetComponent = shallow(
      IssueItem({
        type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY,
        property: "test-property",
        url: "test-url",
        deprecated: false,
        experimental: true,
        prefixNeeded: false,
        unsupportedBrowsers: [],
      })
    );
    expect(targetComponent).toMatchSnapshot();
  });

  it("renders a prefixNeeded issue of CSS property", () => {
    const targetComponent = shallow(
      IssueItem({
        type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY,
        property: "test-property",
        url: "test-url",
        aliases: ["test-alias-1", "test-alias-2"],
        deprecated: false,
        experimental: false,
        prefixNeeded: true,
        unsupportedBrowsers: [],
      })
    );
    expect(targetComponent).toMatchSnapshot();
  });

  it("renders an issue which has deprecated and experimental", () => {
    const targetComponent = shallow(
      IssueItem({
        type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY,
        property: "test-property",
        url: "test-url",
        aliases: ["test-alias-1", "test-alias-2"],
        deprecated: true,
        experimental: true,
        prefixNeeded: false,
        unsupportedBrowsers: [],
      })
    );
    expect(targetComponent).toMatchSnapshot();
  });

  it("renders an issue which has deprecated and prefixNeeded", () => {
    const targetComponent = shallow(
      IssueItem({
        type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY,
        property: "test-property",
        url: "test-url",
        aliases: ["test-alias-1", "test-alias-2"],
        deprecated: true,
        experimental: false,
        prefixNeeded: true,
        unsupportedBrowsers: [],
      })
    );
    expect(targetComponent).toMatchSnapshot();
  });

  it("renders an issue which has experimental and prefixNeeded", () => {
    const targetComponent = shallow(
      IssueItem({
        type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY,
        property: "test-property",
        url: "test-url",
        aliases: ["test-alias-1", "test-alias-2"],
        deprecated: false,
        experimental: true,
        prefixNeeded: true,
        unsupportedBrowsers: [],
      })
    );
    expect(targetComponent).toMatchSnapshot();
  });

  it("renders an issue which has deprecated, experimental and prefixNeeded", () => {
    const targetComponent = shallow(
      IssueItem({
        type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY,
        property: "test-property",
        url: "test-url",
        aliases: ["test-alias-1", "test-alias-2"],
        deprecated: true,
        experimental: true,
        prefixNeeded: true,
        unsupportedBrowsers: [],
      })
    );
    expect(targetComponent).toMatchSnapshot();
  });

  it("renders an issue which has nodes that caused this issue", () => {
    const targetComponent = shallow(
      IssueItem({
        type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY,
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
