/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test utils.
const expect = require("expect");
const { render } = require("enzyme");

// React
const { createFactory } = require("devtools/client/shared/vendor/react");

// Components under test.
const EvaluationResult = createFactory(require("devtools/client/webconsole/new-console-output/components/message-types/evaluation-result").EvaluationResult);

// Test fakes.
const { stubPreparedMessages } = require("devtools/client/webconsole/new-console-output/test/fixtures/stubs/index");

describe("EvaluationResult component:", () => {
  it("renders a grip result", () => {
    const message = stubPreparedMessages.get("new Date(0)");
    const wrapper = render(EvaluationResult({ message }));

    expect(wrapper.find(".message-body").text()).toBe("Date 1970-01-01T00:00:00.000Z");
  });
});
