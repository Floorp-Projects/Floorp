/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const expect = require("expect");
const { render } = require("enzyme");
const { createFactory } = require("devtools/client/shared/vendor/react");
const MessageRepeat = createFactory(
  require("devtools/client/webconsole/components/Output/MessageRepeat")
);

describe("MessageRepeat component:", () => {
  it("renders repeated value correctly", () => {
    const rendered = render(MessageRepeat({ repeat: 99 }));
    expect(rendered.hasClass("message-repeats")).toBe(true);
    expect(rendered.text()).toBe("99");
  });
});
