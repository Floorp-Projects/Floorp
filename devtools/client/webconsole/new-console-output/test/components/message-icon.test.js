/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const {
  MESSAGE_LEVEL,
} = require("devtools/client/webconsole/new-console-output/constants");
const MessageIcon = require("devtools/client/webconsole/new-console-output/components/message-icon");

const expect = require("expect");

const {
  renderComponent
} = require("devtools/client/webconsole/new-console-output/test/helpers");

describe("MessageIcon component:", () => {
  it("renders icon based on level", () => {
    const rendered = renderComponent(MessageIcon, { level: MESSAGE_LEVEL.ERROR });

    expect(rendered.classList.contains("icon")).toBe(true);
    expect(rendered.getAttribute("title")).toBe("Error");
  });
});
