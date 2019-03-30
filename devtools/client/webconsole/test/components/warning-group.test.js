/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test utils.
const expect = require("expect");
const { render } = require("enzyme");

// Components under test.
const WarningGroup = require("devtools/client/webconsole/components/message-types/WarningGroup");
const { MESSAGE_SOURCE } = require("devtools/client/webconsole/constants");
const { ConsoleMessage } = require("devtools/client/webconsole/types");

// Test fakes.
const serviceContainer = require("devtools/client/webconsole/test/fixtures/serviceContainer");
const message = ConsoleMessage({
  messageText: "this is a warning group",
  source: MESSAGE_SOURCE.CONSOLE_FRONTEND,
  timeStamp: Date.now(),
});

describe("WarningGroup component:", () => {
  it("renders", () => {
    const wrapper = render(WarningGroup({
      message,
      serviceContainer,
      timestampsVisible: true,
      badge: 42,
    }));

    const { timestampString } = require("devtools/client/webconsole/webconsole-l10n");
    expect(wrapper.find(".timestamp").text()).toBe(timestampString(message.timeStamp));
    expect(wrapper.find(".message-body").text()).toBe("this is a warning group 42");
    expect(wrapper.find(".arrow[aria-expanded=false]")).toExist();
  });

  it("does have an expanded arrow when `open` prop is true", () => {
    const wrapper = render(WarningGroup({
      message,
      serviceContainer,
      open: true,
    }));

    expect(wrapper.find(".arrow[aria-expanded=true]")).toExist();
  });

  it("does not have a timestamp when timestampsVisible prop is falsy", () => {
    const wrapper = render(WarningGroup({
      message,
      serviceContainer,
      timestampsVisible: false,
    }));

    expect(wrapper.find(".timestamp").length).toBe(0);
  });
});
