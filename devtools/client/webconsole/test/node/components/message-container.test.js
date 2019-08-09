/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test utils.
const expect = require("expect");
const { render } = require("enzyme");
const { createFactory } = require("devtools/client/shared/vendor/react");

// Components under test.
let {
  MessageContainer,
  getMessageComponent,
} = require("devtools/client/webconsole/components/Output/MessageContainer");
MessageContainer = createFactory(MessageContainer);
const ConsoleApiCall = require("devtools/client/webconsole/components/Output/message-types/ConsoleApiCall");
const CSSWarning = require("devtools/client/webconsole/components/Output/message-types/CSSWarning");
const EvaluationResult = require("devtools/client/webconsole/components/Output/message-types/EvaluationResult");
const PageError = require("devtools/client/webconsole/components/Output/message-types/PageError");

// Test fakes.
const {
  stubPreparedMessages,
} = require("devtools/client/webconsole/test/node/fixtures/stubs/index");
const serviceContainer = require("devtools/client/webconsole/test/node/fixtures/serviceContainer");

describe("MessageContainer component:", () => {
  it("pipes data to children as expected", () => {
    const message = stubPreparedMessages.get("console.log('foobar', 'test')");
    const rendered = render(
      MessageContainer({
        getMessage: () => message,
        serviceContainer,
      })
    );

    expect(rendered.text().includes("foobar")).toBe(true);
  });
  it("picks correct child component", () => {
    const messageTypes = [
      {
        component: ConsoleApiCall,
        message: stubPreparedMessages.get("console.log('foobar', 'test')"),
      },
      {
        component: EvaluationResult,
        message: stubPreparedMessages.get("new Date(0)"),
      },
      {
        component: PageError,
        message: stubPreparedMessages.get(
          "ReferenceError: asdf is not defined"
        ),
      },
      {
        component: CSSWarning,
        message: stubPreparedMessages.get(
          "Unknown property ‘such-unknown-property’.  Declaration dropped."
        ),
      },
    ];

    messageTypes.forEach(info => {
      const { component, message } = info;
      expect(getMessageComponent(message)).toBe(component);
    });
  });
});
