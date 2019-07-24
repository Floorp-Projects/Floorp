/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { mount } = require("enzyme");
const React = require("react");
const Provider = React.createFactory(
  require("devtools/client/shared/vendor/react-redux").Provider
);

// Import & init localization
const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const LocalizationProvider = React.createFactory(
  FluentReact.LocalizationProvider
);

// Import constants
const {
  INITSTATE,
  CLIENT,
  EMPTY_WORKER_LIST,
  SINGLE_WORKER_LIST,
  MULTIPLE_WORKER_LIST,
} = require("devtools/client/application/test/components/fixtures/data/constants");

// Import setupStore with imported & combined reducers
const {
  setupStore,
} = require("devtools/client/application/test/components/helpers/helpers");

// Import component
const App = React.createFactory(
  require("devtools/client/application/src/components/App")
);

/**
 * Test for App.js component
 */
describe("App:", () => {
  const store = setupStore(INITSTATE, {
    client: CLIENT,
    worker: SINGLE_WORKER_LIST,
  });
  it("renders the expected snapshot for an App with empty worker list", () => {
    const wrapper = mount(
      Provider(
        { store },
        LocalizationProvider(
          { messages: [] },
          App(INITSTATE, { client: CLIENT, worker: EMPTY_WORKER_LIST })
        )
      )
    );
    expect(wrapper).toMatchSnapshot();
  });
  it("renders the expected snapshot for an App with single worker list", () => {
    const wrapper = mount(
      Provider(
        { store },
        LocalizationProvider(
          { messages: [] },
          App(INITSTATE, { client: CLIENT, worker: SINGLE_WORKER_LIST })
        )
      )
    );
    expect(wrapper).toMatchSnapshot();
  });
  it("renders the expected snapshot for an App with multiple workers list", () => {
    const wrapper = mount(
      Provider(
        { store },
        LocalizationProvider(
          { messages: [] },
          App(INITSTATE, { client: CLIENT, worker: MULTIPLE_WORKER_LIST })
        )
      )
    );
    expect(wrapper).toMatchSnapshot();
  });
});
