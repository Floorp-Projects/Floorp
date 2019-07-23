/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const React = require("react");

// Import constants
const {
  CLIENT,
  SINGLE_WORKER_LIST,
  MULTIPLE_WORKER_LIST,
} = require("devtools/client/application/test/components/fixtures/data/constants");

const WorkerList = React.createFactory(
  require("devtools/client/application/src/components/WorkerList")
);

/**
 * Test for workerList.js component
 */
describe("Service Worker List:", () => {
  it("renders the expected snapshot for a workerList with single worker list", () => {
    const wrapper = shallow(
      WorkerList({
        client: CLIENT,
        workers: SINGLE_WORKER_LIST,
        isDebugEnabled: true,
      })
    );
    expect(wrapper).toMatchSnapshot();
  });

  it("renders the expected snapshot for a workerList with multiple workers list", () => {
    const wrapper = shallow(
      WorkerList({
        client: CLIENT,
        workers: MULTIPLE_WORKER_LIST,
        isDebugEnabled: true,
      })
    );
    expect(wrapper).toMatchSnapshot();
  });
});
