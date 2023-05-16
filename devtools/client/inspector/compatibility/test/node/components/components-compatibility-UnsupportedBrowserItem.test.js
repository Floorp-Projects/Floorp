/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Unit tests for the UnsupportedBrowserItem component.
 */

const { shallow } = require("enzyme");
const React = require("react");

const BrowserItem = React.createFactory(
  require("resource://devtools/client/inspector/compatibility/components/UnsupportedBrowserItem.js")
);

describe("UnsupportedBrowserItem component", () => {
  it("renders the browser", () => {
    const item = shallow(
      BrowserItem({
        id: "firefox",
        name: "Firefox",
        version: "113",
        unsupportedVersions: [
          {
            status: "current",
            version: "113",
          },
          {
            status: "beta",
            version: "114",
          },
          {
            status: "release",
            version: "115",
          },
        ],
      })
    );
    expect(item).toMatchSnapshot();
  });
});
