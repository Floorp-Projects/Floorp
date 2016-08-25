/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { getRepeatId } = require("devtools/client/webconsole/new-console-output/utils/messages");
const { stubPreparedMessages } = require("devtools/client/webconsole/new-console-output/test/fixtures/stubs/index");
const { setupActions } = require("devtools/client/webconsole/new-console-output/test/helpers");
const constants = require("devtools/client/webconsole/new-console-output/constants");

const expect = require("expect");

let actions;

describe("Message actions:", () => {
  before(()=>{
    actions = setupActions();
  });

  describe("messageAdd", () => {
    it("creates expected action given a packet", () => {
      const packet = {
        "from": "server1.conn4.child1/consoleActor2",
        "type": "consoleAPICall",
        "message": {
          "arguments": [
            "foobar",
            "test"
          ],
          "columnNumber": 27,
          "counter": null,
          "filename": "http://example.com/browser/devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/test-tempfile.js",
          "functionName": "",
          "groupName": "",
          "level": "log",
          "lineNumber": 1,
          "private": false,
          "styles": [],
          "timeStamp": 1455064271115,
          "timer": null,
          "workerType": "none",
          "category": "webdev"
        }
      };
      const action = actions.messageAdd(packet);
      const expected = {
        type: constants.MESSAGE_ADD,
        message: stubPreparedMessages.get("console.log('foobar', 'test')")
      };

      expect(action.message.toJS()).toEqual(expected.message.toJS());
    });
  });

  describe("messagesClear", () => {
    it("creates expected action", () => {
      const action = actions.messagesClear();
      const expected = {
        type: constants.MESSAGES_CLEAR,
      };
      expect(action).toEqual(expected);
    });
  });
});
