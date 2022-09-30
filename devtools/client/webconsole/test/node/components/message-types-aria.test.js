/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test utils.
const expect = require("expect");
const { render } = require("enzyme");
const {
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");

// Components under test.
const ConsoleApiCall = createFactory(
  require("resource://devtools/client/webconsole/components/Output/message-types/ConsoleApiCall.js")
);
const ConsoleCmd = createFactory(
  require("resource://devtools/client/webconsole/components/Output/message-types/ConsoleCommand.js")
);
const EvaluationResult = createFactory(
  require("resource://devtools/client/webconsole/components/Output/message-types/EvaluationResult.js")
);

const {
  ConsoleCommand,
} = require("resource://devtools/client/webconsole/types.js");

// Test fakes.
const {
  stubPreparedMessages,
} = require("resource://devtools/client/webconsole/test/node/fixtures/stubs/index.js");
const serviceContainer = require("resource://devtools/client/webconsole/test/node/fixtures/serviceContainer.js");

describe("message types component ARIA:", () => {
  describe("ConsoleAPICall", () => {
    it("sets aria-live to polite", () => {
      const message = stubPreparedMessages.get("console.log('foobar', 'test')");
      const wrapper = render(ConsoleApiCall({ message, serviceContainer }));
      expect(wrapper.attr("aria-live")).toBe("polite");
    });
  });

  describe("EvaluationResult", () => {
    it("sets aria-live to polite", () => {
      const message = stubPreparedMessages.get("asdf()");
      const wrapper = render(EvaluationResult({ message, serviceContainer }));
      expect(wrapper.attr("aria-live")).toBe("polite");
    });
  });

  describe("ConsoleCommand", () => {
    it("sets aria-live to off", () => {
      const message = new ConsoleCommand({
        messageText: `"simple"`,
      });
      const wrapper = render(ConsoleCmd({ message, serviceContainer }));
      expect(wrapper.attr("aria-live")).toBe("off");
    });
  });
});
