/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test utils.
const expect = require("expect");

const {
  renderComponent
} = require("devtools/client/webconsole/new-console-output/test/helpers");

// Components under test.
const ConsoleApiCall = require("devtools/client/webconsole/new-console-output/components/message-types/ConsoleApiCall");
const ConsoleCmd = require("devtools/client/webconsole/new-console-output/components/message-types/ConsoleCommand");
const EvaluationResult = require("devtools/client/webconsole/new-console-output/components/message-types/EvaluationResult");

const { ConsoleCommand } = require("devtools/client/webconsole/new-console-output/types");

// Test fakes.
const { stubPreparedMessages } = require("devtools/client/webconsole/new-console-output/test/fixtures/stubs/index");
const serviceContainer = require("devtools/client/webconsole/new-console-output/test/fixtures/serviceContainer");

describe("message types component ARIA:", () => {
  describe("ConsoleAPICall", () => {
    it("sets aria-live to polite", () => {
      const message = stubPreparedMessages.get("console.log('foobar', 'test')");
      const wrapper = renderComponent(ConsoleApiCall,
        { message, serviceContainer });
      expect(wrapper.getAttribute("aria-live")).toBe("polite");
    });
  });

  describe("EvaluationResult", () => {
    it("sets aria-live to polite", () => {
      const message = stubPreparedMessages.get("asdf()");
      const wrapper = renderComponent(EvaluationResult, { message, serviceContainer });
      expect(wrapper.getAttribute("aria-live")).toBe("polite");
    });
  });

  describe("ConsoleCommand", () => {
    it("sets aria-live to off", () => {
      let message = new ConsoleCommand({
        messageText: `"simple"`,
      });
      const wrapper = renderComponent(ConsoleCmd, { message, serviceContainer});
      expect(wrapper.getAttribute("aria-live")).toBe("off");
    });
  });
});
