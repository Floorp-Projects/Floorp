/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const { createFactory } = require("react");

const ManifestPage = createFactory(
  require("devtools/client/application/src/components/manifest/ManifestPage")
);

/**
 * Test for ManifestPage.js component
 */

describe("ManifestPage", () => {
  it("renders the expected snapshot", () => {
    const wrapper = shallow(ManifestPage({}));
    expect(wrapper).toMatchSnapshot();
  });
});
