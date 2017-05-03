/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test utils.
const expect = require("expect");
const { render } = require("enzyme");

// React
const { createFactory } = require("devtools/client/shared/vendor/react");

// Components under test.
const ConsoleApiCall = createFactory(require("devtools/client/webconsole/new-console-output/components/message-types/console-api-call"));

const { ConsoleMessage } = require("devtools/client/webconsole/new-console-output/types");
const { prepareMessage } = require("devtools/client/webconsole/new-console-output/utils/messages");
const serviceContainer = require("devtools/client/webconsole/new-console-output/test/fixtures/serviceContainer");

describe("ConsoleAPICall component:", () => {
  describe("Services.console.logStringMessage", () => {
    it("renders logMessage grips", () => {
      let message = prepareMessage(logMessageStubPacket, {getNextId: () => "1"});
      message = new ConsoleMessage(message);

      const wrapper = render(ConsoleApiCall({ message, serviceContainer }));

      expect(wrapper.find(".message-body").text()).toBe("foobar test");

      // There should not be the location
      expect(wrapper.find(".message-location").text()).toBe("");
    });
  });
});

// Stub packet
const logMessageStubPacket = {
  "from": "server1.conn1.consoleActor2",
  "message": "foobar test",
  "timeStamp": "1493370184067",
  "_type": "LogMessage",
};
