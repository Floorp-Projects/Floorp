/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test utils.
const expect = require("expect");
const { render } = require("enzyme");

// React
const { createFactory } = require("devtools/client/shared/vendor/react");
const { setupStore } = require("devtools/client/webconsole/test/helpers");
const Provider = createFactory(require("react-redux").Provider);

// Components under test.
const ConsoleApiCall = createFactory(require("devtools/client/webconsole/components/message-types/ConsoleApiCall"));

const { prepareMessage } = require("devtools/client/webconsole/utils/messages");
const serviceContainer = require("devtools/client/webconsole/test/fixtures/serviceContainer");

describe("ConsoleAPICall component:", () => {
  describe("Services.console.logStringMessage", () => {
    it("renders cached logMessage grips", () => {
      const message = prepareMessage(cachedLogMessageStubPacket, {getNextId: () => "1"});
      const wrapper = render(ConsoleApiCall({ message, serviceContainer }));

      expect(wrapper.find(".message-body").text()).toBe("foobar test");

      // There should not be the location
      expect(wrapper.find(".message-location").text()).toBe("");
    });

    it("renders logMessage grips", () => {
      const message = prepareMessage(logMessageStubPacket, {getNextId: () => "1"});
      const wrapper = render(ConsoleApiCall({ message, serviceContainer }));

      expect(wrapper.find(".message-body").text()).toBe("foobar test");

      // There should not be the location
      expect(wrapper.find(".message-location").text()).toBe("");
    });

    it("renders longString logMessage grips", () => {
      const message =
        prepareMessage(logMessageLongStringStubPacket, {getNextId: () => "1"});

      // We need to wrap the ConsoleApiElement in a Provider in order for the
      // ObjectInspector to work.
      const wrapper = render(
        Provider({ store: setupStore() }, ConsoleApiCall({ message, serviceContainer }))
      );

      expect(wrapper.find(".message-body").text()).toInclude(initialText);
    });
  });
});

// Stub packet
const cachedLogMessageStubPacket = {
  "from": "server1.conn1.consoleActor2",
  "message": "foobar test",
  "timeStamp": "1493370184067",
  "_type": "LogMessage",
};

const logMessageStubPacket = {
  "from": "server1.conn0.consoleActor2",
  "type": "logMessage",
  "message": "foobar test",
  "timeStamp": 1519052480060,
};

const multilineFullText = `a\n${Array(20000)
  .fill("a")
  .join("")}`;
const fullTextLength = multilineFullText.length;
const initialText = multilineFullText.substring(0, 10000);
const logMessageLongStringStubPacket = {
  "from": "server1.conn0.consoleActor2",
  "type": "logMessage",
  "message": {
    type: "longString",
    initial: initialText,
    length: fullTextLength,
    actor: "server1.conn1.child1/longString58",
  },
  "timeStamp": 1519052480060,
};
