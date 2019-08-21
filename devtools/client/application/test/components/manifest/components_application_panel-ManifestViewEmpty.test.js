/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const { createFactory } = require("react");

const ManifestViewEmpty = createFactory(
  require("devtools/client/application/src/components/manifest/ManifestViewEmpty")
);

/**
 * Test for ManifestPage.js component
 */

describe("ManifestViewEmpty", () => {
  it("renders the expected snapshot", () => {
    const wrapper = shallow(ManifestViewEmpty({}));
    expect(wrapper).toMatchSnapshot();
  });
});
