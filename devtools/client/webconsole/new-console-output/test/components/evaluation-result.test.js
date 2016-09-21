/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { stubPreparedMessages } = require("devtools/client/webconsole/new-console-output/test/fixtures/stubs/index");
const { EvaluationResult } = require("devtools/client/webconsole/new-console-output/components/message-types/evaluation-result");

const expect = require("expect");

const {
  renderComponent
} = require("devtools/client/webconsole/new-console-output/test/helpers");

describe("EvaluationResult component:", () => {
  it("renders a grip result", () => {
    const message = stubPreparedMessages.get("new Date(0)");
    const props = {
      message
    };
    const rendered = renderComponent(EvaluationResult, props);

    const messageBody = getMessageBody(rendered);
    expect(messageBody.textContent).toBe("Date 1970-01-01T00:00:00.000Z");
  });
});

function getMessageBody(rendered) {
  const queryPath = "div.message span.message-body-wrapper span.message-body";
  return rendered.querySelector(queryPath);
}
