/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { stubConsoleMessages } = require("devtools/client/webconsole/new-console-output/test/fixtures/stubs");
const { ConsoleApiCall } = require("devtools/client/webconsole/new-console-output/components/message-types/console-api-call");
const expect = require("expect");

const {
  renderComponent
} = require("devtools/client/webconsole/new-console-output/test/helpers");

describe("ConsoleAPICall component:", () => {
  describe("console.log", () => {
    it("renders string grips", () => {
      const message = stubConsoleMessages.get("console.log('foobar', 'test')");
      const rendered = renderComponent(ConsoleApiCall, {message});

      const messageBody = getMessageBody(rendered);
      // @TODO should output: foobar test
      expect(messageBody.textContent).toBe("\"foobar\"\"test\"");

      const consoleStringNodes = messageBody.querySelectorAll(".objectBox-string");
      expect(consoleStringNodes.length).toBe(2);
    });
    it("renders repeat node", () => {
      const message =
        stubConsoleMessages.get("console.log('foobar', 'test')")
        .set("repeat", 107);
      const rendered = renderComponent(ConsoleApiCall, {message});

      const repeatNode = getRepeatNode(rendered);
      expect(repeatNode[0].textContent).toBe("107");
    });
  });

  describe("console.count", () => {
    it("renders", () => {
      const message = stubConsoleMessages.get("console.count('bar')");
      const rendered = renderComponent(ConsoleApiCall, {message});

      const messageBody = getMessageBody(rendered);
      expect(messageBody.textContent).toBe(message.messageText);
    });
  });
});

function getMessageBody(rendered) {
  const queryPath = "div.message.cm-s-mozilla span span.message-flex-body span.message-body.devtools-monospace";
  return rendered.querySelector(queryPath);
}

function getRepeatNode(rendered) {
  const repeatPath = "span > span.message-flex-body > span.message-body.devtools-monospace + span.message-repeats";
  return rendered.querySelectorAll(repeatPath);
}