/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test utils.
const expect = require("expect");
const { render } = require("enzyme");
const { createFactory } = require("devtools/client/shared/vendor/react");

// Components under test.
const ConsoleApiCall = createFactory(require("devtools/client/webconsole/components/message-types/ConsoleApiCall"));
const ConsoleCmd = createFactory(require("devtools/client/webconsole/components/message-types/ConsoleCommand"));
const EvaluationResult = createFactory(require("devtools/client/webconsole/components/message-types/EvaluationResult"));

const { ConsoleCommand } = require("devtools/client/webconsole/types");

// Test fakes.
const { stubPreparedMessages } = require("devtools/client/webconsole/test/fixtures/stubs/index");
const serviceContainer = require("devtools/client/webconsole/test/fixtures/serviceContainer");

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
      const wrapper = render(EvaluationResult({message, serviceContainer}));
      expect(wrapper.attr("aria-live")).toBe("polite");
    });
  });

  describe("ConsoleCommand", () => {
    it("sets aria-live to off", () => {
      const message = new ConsoleCommand({
        messageText: `"simple"`,
      });
      const wrapper = render(ConsoleCmd({message, serviceContainer}));
      expect(wrapper.attr("aria-live")).toBe("off");
    });
  });
});
