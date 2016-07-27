/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const {
  SEVERITY_ERROR,
} = require("devtools/client/webconsole/new-console-output/constants");
const { MessageIcon } = require("devtools/client/webconsole/new-console-output/components/message-icon");

const jsdom = require("mocha-jsdom");
const expect = require("expect");

const {
  renderComponent
} = require("devtools/client/webconsole/new-console-output/test/helpers");

describe("MessageIcon component:", () => {
  jsdom();

  it("renders icon based on severity", () => {
    const rendered = renderComponent(MessageIcon, { severity: SEVERITY_ERROR });

    expect(rendered.classList.contains("icon")).toBe(true);
    expect(rendered.getAttribute("title")).toBe("Error");
  });
});
