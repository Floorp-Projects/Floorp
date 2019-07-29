/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const React = require("react");

const WorkerListEmpty = React.createFactory(
  require("devtools/client/application/src/components/service-workers/WorkerListEmpty")
);

/**
 * Test for workerListEmpty.js component
 */

describe("WorkerListEmpty", () => {
  it("renders the expected snapshot", () => {
    const wrapper = shallow(
      WorkerListEmpty({
        serviceContainer: {},
      })
    );
    expect(wrapper).toMatchSnapshot();
  });
});
