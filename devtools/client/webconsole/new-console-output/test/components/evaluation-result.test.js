/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { stubConsoleMessages } = require("devtools/client/webconsole/new-console-output/test/fixtures/stubs");
const { EvaluationResult } = require("devtools/client/webconsole/new-console-output/components/message-types/evaluation-result");

const jsdom = require("mocha-jsdom");
const expect = require("expect");

const {
  renderComponent
} = require("devtools/client/webconsole/new-console-output/test/helpers");

describe("EvaluationResult component:", () => {
  jsdom();
  it("renders a grip result", () => {
    const message = stubConsoleMessages.get("new Date(0)");
    const props = {
      message
    };
    const rendered = renderComponent(EvaluationResult, props);

    const messageBody = getMessageBody(rendered);
    expect(messageBody.textContent).toBe("Date1970-01-01T00:00:00.000Z");
  });
});

function getMessageBody(rendered) {
  const queryPath = "div.message.cm-s-mozilla span.message-body-wrapper.message-body.devtools-monospace";
  return rendered.querySelector(queryPath);
}
