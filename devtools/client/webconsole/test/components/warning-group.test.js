/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test utils.
const expect = require("expect");
const { render } = require("enzyme");

// Components under test.
const WarningGroup = require("devtools/client/webconsole/components/message-types/WarningGroup");
const { MESSAGE_SOURCE, MESSAGE_TYPE } = require("devtools/client/webconsole/constants");
const { ConsoleMessage } = require("devtools/client/webconsole/types");
const { createWarningGroupMessage } = require("devtools/client/webconsole/utils/messages");

// Test fakes.
const { stubPreparedMessages } = require("devtools/client/webconsole/test/fixtures/stubs/index");
const serviceContainer = require("devtools/client/webconsole/test/fixtures/serviceContainer");
const mockMessage = ConsoleMessage({
  messageText: "this is a warning group",
  source: MESSAGE_SOURCE.CONSOLE_FRONTEND,
  timeStamp: Date.now(),
});

describe("WarningGroup component:", () => {
  it("renders", () => {
    const wrapper = render(WarningGroup({
      message: mockMessage,
      serviceContainer,
      timestampsVisible: true,
      badge: 42,
    }));

    const { timestampString } = require("devtools/client/webconsole/webconsole-l10n");
    expect(wrapper.find(".timestamp").text())
      .toBe(timestampString(mockMessage.timeStamp));
    expect(wrapper.find(".message-body").text()).toBe("this is a warning group 42");
    expect(wrapper.find(".arrow[aria-expanded=false]")).toExist();
  });

  it("does have an expanded arrow when `open` prop is true", () => {
    const wrapper = render(WarningGroup({
      message: mockMessage,
      serviceContainer,
      open: true,
    }));

    expect(wrapper.find(".arrow[aria-expanded=true]")).toExist();
  });

  it("does not have a timestamp when timestampsVisible prop is falsy", () => {
    const wrapper = render(WarningGroup({
      message: mockMessage,
      serviceContainer,
      timestampsVisible: false,
    }));

    expect(wrapper.find(".timestamp").length).toBe(0);
  });

  it("renders Content Blocking Group message", () => {
    const firstMessage = stubPreparedMessages.get("ReferenceError: asdf is not defined");
    const type = MESSAGE_TYPE.CONTENT_BLOCKING_GROUP;
    const message = createWarningGroupMessage(`${type}-${firstMessage.innerWindowID}`,
      type, firstMessage);

    const wrapper = render(WarningGroup({
      message,
      serviceContainer,
      badge: 24,
    }));

    expect(wrapper.find(".message-body").text()).toBe("Content blocked messages 24");
    expect(wrapper.find(".arrow[aria-expanded=false]")).toExist();
  });
});
