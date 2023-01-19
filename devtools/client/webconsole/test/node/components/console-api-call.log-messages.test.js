/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test utils.
const expect = require("expect");
const { render } = require("enzyme");

// React
const {
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const {
  setupStore,
} = require("resource://devtools/client/webconsole/test/node/helpers.js");
const Provider = createFactory(
  require("resource://devtools/client/shared/vendor/react-redux.js").Provider
);

// Components under test.
const ConsoleApiCall = createFactory(
  require("resource://devtools/client/webconsole/components/Output/message-types/ConsoleApiCall.js")
);

const {
  stubPreparedMessages,
} = require("resource://devtools/client/webconsole/test/node/fixtures/stubs/index.js");

const serviceContainer = require("resource://devtools/client/webconsole/test/node/fixtures/serviceContainer.js");

describe("ConsoleAPICall component for platform message", () => {
  describe("Services.console.logStringMessage", () => {
    it("renders logMessage grips", () => {
      const message = stubPreparedMessages.get("platform-simple-message");
      const wrapper = render(ConsoleApiCall({ message, serviceContainer }));

      expect(wrapper.find(".message-body").text()).toBe("foobar test");

      // There should not be the location
      expect(wrapper.find(".message-location").text()).toBe("");
    });

    it("renders longString logMessage grips", () => {
      const message = stubPreparedMessages.get("platform-longString-message");

      // We need to wrap the ConsoleApiElement in a Provider in order for the
      // ObjectInspector to work.
      const wrapper = render(
        Provider(
          { store: setupStore() },
          ConsoleApiCall({ message, serviceContainer })
        )
      );

      expect(wrapper.find(".message-body").text()).toInclude(
        `a\n${"a".repeat(100)}`
      );
    });
  });
});
