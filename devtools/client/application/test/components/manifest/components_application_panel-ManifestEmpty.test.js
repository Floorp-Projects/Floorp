/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const { createFactory } = require("react");

const ManifestEmpty = createFactory(
  require("devtools/client/application/src/components/manifest/ManifestEmpty")
);

/**
 * Test for ManifestEmpty component
 */

describe("ManifestEmpty", () => {
  it("renders the expected snapshot", () => {
    const wrapper = shallow(ManifestEmpty({}));
    expect(wrapper).toMatchSnapshot();
  });
});
