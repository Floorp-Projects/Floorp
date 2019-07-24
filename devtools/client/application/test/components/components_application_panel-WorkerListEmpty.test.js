/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const React = require("react");

// Import constants
const {
  CLIENT,
  EMPTY_WORKER_LIST,
} = require("devtools/client/application/test/components/fixtures/data/constants");

const WorkerListEmpty = React.createFactory(
  require("devtools/client/application/src/components/WorkerListEmpty")
);

/**
 * Test for workerListEmpty.js component
 */

describe("Empty Service Worker List:", () => {
  it("renders the expected snapshot for a workerListEmpty with empty worker list", () => {
    const wrapper = shallow(
      WorkerListEmpty({
        client: CLIENT,
        workers: EMPTY_WORKER_LIST,
        isDebugEnabled: true,
      })
    );
    expect(wrapper).toMatchSnapshot();
  });
});
