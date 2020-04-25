/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const { createFactory } = require("react");

const {
  REGISTRATION_SINGLE_WORKER,
  REGISTRATION_MULTIPLE_WORKERS,
} = require("devtools/client/application/test/node/fixtures/data/constants");

const Registration = createFactory(
  require("devtools/client/application/src/components/service-workers/Registration")
);

describe("Registration", () => {
  it("Renders the expected snapshot for a registration with a worker", () => {
    const wrapper = shallow(
      Registration({
        isDebugEnabled: true,
        registration: REGISTRATION_SINGLE_WORKER,
      })
    );

    expect(wrapper).toMatchSnapshot();
    // ensure that we do have the proper amount of workers
    expect(wrapper.find("Worker")).toHaveLength(1);
  });

  it("Renders the expected snapshot for a registration with multiple workers", () => {
    const wrapper = shallow(
      Registration({
        isDebugEnabled: true,
        registration: REGISTRATION_MULTIPLE_WORKERS,
      })
    );

    expect(wrapper).toMatchSnapshot();
    // ensure that we do have the proper amount of workers
    expect(wrapper.find("Worker")).toHaveLength(2);
  });

  it("Renders the expected snapshot when sw debugging is disabled", () => {
    const wrapper = shallow(
      Registration({
        isDebugEnabled: false,
        registration: REGISTRATION_SINGLE_WORKER,
      })
    );

    expect(wrapper).toMatchSnapshot();
  });
});
