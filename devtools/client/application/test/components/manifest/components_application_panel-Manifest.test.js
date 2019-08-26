/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const { createFactory } = require("react");

const Manifest = createFactory(
  require("devtools/client/application/src/components/manifest/Manifest")
);

const { MANIFEST_SIMPLE } = require("../fixtures/data/constants");

/*
 * Test for Manifest component
 */

describe("Manifest", () => {
  it("renders the expected snapshot", () => {
    const wrapper = shallow(Manifest(MANIFEST_SIMPLE));
    expect(wrapper).toMatchSnapshot();
  });
});
