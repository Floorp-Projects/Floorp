/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { stubConsoleMessages } = require("devtools/client/webconsole/new-console-output/test/fixtures/stubs");

const { PageError } = require("devtools/client/webconsole/new-console-output/components/message-types/page-error");

const expect = require("expect");

const {
  renderComponent
} = require("devtools/client/webconsole/new-console-output/test/helpers");

describe("PageError component:", () => {
  it("renders a page error", () => {
    const message = stubConsoleMessages.get("ReferenceError");
    const rendered = renderComponent(PageError, {message});

    const messageBody = getMessageBody(rendered);
    expect(messageBody.textContent).toBe("ReferenceError: asdf is not defined");
  });
});

function getMessageBody(rendered) {
  const queryPath = "div.message span.message-body-wrapper.message-body.devtools-monospace";
  return rendered.querySelector(queryPath);
}
