/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const { createFactory } = require("react");
// Import test helpers
const { setupStore } = require("devtools/client/application/test/node/helpers");

const {
  WORKER_RUNNING,
  WORKER_STOPPED,
  WORKER_WAITING,
} = require("devtools/client/application/test/node/fixtures/data/constants");

const Worker = createFactory(
  require("devtools/client/application/src/components/service-workers/Worker")
);

describe("Worker", () => {
  it("Renders the expected snapshot for a running worker", () => {
    const store = setupStore({});

    const wrapper = shallow(
      Worker({
        isDebugEnabled: true,
        worker: WORKER_RUNNING,
        store,
      })
    ).dive();

    // ensure proper status
    expect(wrapper.find(".js-worker-status").text()).toBe(
      "serviceworker-worker-status-running"
    );
    // check that Start button is not available
    expect(wrapper.find(".js-start-button")).toHaveLength(0);

    expect(wrapper).toMatchSnapshot();
  });

  it("Renders the expected snapshot for a stopped worker", () => {
    const store = setupStore({});

    const wrapper = shallow(
      Worker({
        isDebugEnabled: true,
        worker: WORKER_STOPPED,
        store,
      })
    ).dive();

    // ensure proper status
    expect(wrapper.find(".js-worker-status").text()).toBe(
      "serviceworker-worker-status-stopped"
    );
    // check that Start button is not available
    expect(wrapper.find(".js-start-button")).toHaveLength(1);

    expect(wrapper).toMatchSnapshot();
  });

  it("Renders the expected snapshot for a non-active worker", () => {
    const store = setupStore({});

    const wrapper = shallow(
      Worker({
        isDebugEnabled: true,
        worker: WORKER_WAITING,
        store,
      })
    ).dive();

    // ensure proper status
    // NOTE: since non-active status are localized directly in the front, not
    //       in the panel, we don't expect a localization ID here
    expect(wrapper.find(".js-worker-status").text()).toBe("installed");
    // check that Start button is not available
    expect(wrapper.find(".js-start-button")).toHaveLength(0);

    expect(wrapper).toMatchSnapshot();
  });

  it("Enables/disabled the debug button depending of debugging being available", () => {
    const store = setupStore({});

    // check disabled debugging
    let wrapper = shallow(
      Worker({
        isDebugEnabled: false,
        worker: WORKER_RUNNING,
        store,
      })
    ).dive();

    expect(wrapper.find(".js-debug-button[disabled=true]")).toHaveLength(1);

    // check enabled debugging
    wrapper = shallow(
      Worker({
        isDebugEnabled: true,
        worker: WORKER_RUNNING,
        store,
      })
    ).dive();

    expect(wrapper.find(".js-debug-button[disabled=false]")).toHaveLength(1);
  });
});
