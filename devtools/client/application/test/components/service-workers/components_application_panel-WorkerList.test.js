/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const React = require("react");

// Import constants
const {
  SINGLE_WORKER_DEFAULT_DOMAIN_LIST,
  MULTIPLE_WORKER_LIST,
} = require("devtools/client/application/test/components/fixtures/data/constants");

const WorkerList = React.createFactory(
  require("devtools/client/application/src/components/service-workers/WorkerList")
);

/**
 * Test for workerList.js component
 */
describe("WorkerList", () => {
  it("renders the expected snapshot for a list with a single worker", () => {
    const wrapper = shallow(
      WorkerList({
        client: {},
        workers: SINGLE_WORKER_DEFAULT_DOMAIN_LIST,
        canDebugWorkers: true,
      })
    );
    expect(wrapper).toMatchSnapshot();
  });

  it("renders the expected snapshot for a multiple workers list", () => {
    const wrapper = shallow(
      WorkerList({
        client: {},
        workers: MULTIPLE_WORKER_LIST,
        canDebugWorkers: true,
      })
    );
    expect(wrapper).toMatchSnapshot();
  });
});
