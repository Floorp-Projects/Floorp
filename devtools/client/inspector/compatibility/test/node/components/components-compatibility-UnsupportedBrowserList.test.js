/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Unit tests for the BrowserList component.
 */

const { shallow } = require("enzyme");
const React = require("react");

const UnsupportedBrowserList = React.createFactory(
  require("devtools/client/inspector/compatibility/components/UnsupportedBrowserList")
);

describe("UnsupportedBrowserList component", () => {
  it("renders the browsers", () => {
    const list = shallow(
      UnsupportedBrowserList({
        browsers: [
          { id: "firefox", name: "Firefox", version: "69", status: "beta" },
          { id: "firefox", name: "Firefox", version: "70", status: "nightly" },
          { id: "test-browser", name: "Test Browser", version: "1" },
          { id: "test-browser", name: "Test Browser", version: "2" },
          { id: "sample-browser", name: "Sample Browser", version: "100" },
        ],
      })
    );
    expect(list).toMatchSnapshot();
  });
});
