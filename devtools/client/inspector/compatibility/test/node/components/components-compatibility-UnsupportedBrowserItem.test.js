/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Unit tests for the UnsupportedBrowserItem component.
 */

const { shallow } = require("enzyme");
const React = require("react");

const BrowserItem = React.createFactory(
  require("devtools/client/inspector/compatibility/components/UnsupportedBrowserItem")
);

describe("UnsupportedBrowserItem component", () => {
  it("renders the browser", () => {
    const item = shallow(
      BrowserItem({
        id: "firefox",
        name: "Firefox",
        versions: [
          {
            version: "1",
          },
          {
            alias: "beta",
            version: "69",
          },
          {
            alias: "nightly",
            version: "70",
          },
        ],
      })
    );
    expect(item).toMatchSnapshot();
  });
});
