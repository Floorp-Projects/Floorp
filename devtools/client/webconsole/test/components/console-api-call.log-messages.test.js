/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test utils.
const expect = require("expect");
const { render } = require("enzyme");

// React
const { createFactory } = require("devtools/client/shared/vendor/react");

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
  "timeStamp": 1519052480060
};
