/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const { createFactory } = require("react");

const RegistrationListEmpty = createFactory(
  require("resource://devtools/client/application/src/components/service-workers/RegistrationListEmpty.js")
);

/**
 * Test for RegistrationListEmpty.js component
 */

describe("RegistrationListEmpty", () => {
  it("renders the expected snapshot", () => {
    const wrapper = shallow(RegistrationListEmpty({}));
    expect(wrapper).toMatchSnapshot();
  });
});
