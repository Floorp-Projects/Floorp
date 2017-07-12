/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const {
  setupActions,
  setupStore,
  clonePacket
} = require("devtools/client/webconsole/new-console-output/test/helpers");
const { stubPackets } = require("devtools/client/webconsole/new-console-output/test/fixtures/stubs/index");

const {
  getAllMessagesById,
} = require("devtools/client/webconsole/new-console-output/selectors/messages");

const expect = require("expect");

describe("Release actor enhancer:", () => {
  let actions;

  before(() => {
    actions = setupActions();
  });

  describe("Client proxy", () => {
    it("properly releases backend actors when limit is reached", () => {
      const logLimit = 100;
      let releasedActors = [];
      const { dispatch, getState } = setupStore([], {
        proxy: {
          releaseActor: (actor) => {
            releasedActors.push(actor);
          }
        }
      }, { logLimit });

      // Add a log message with loaded object properties.
      dispatch(actions.messageAdd(
        stubPackets.get("console.log('myarray', ['red', 'green', 'blue'])")));

      let messages = getAllMessagesById(getState());
      const firstMessage = messages.first();
      const firstMessageActor = firstMessage.parameters[1].actor;
      const arrayProperties = Symbol();
      const arraySubProperties = Symbol();
      const [id] = [...messages.keys()];
      dispatch(actions.messageObjectPropertiesReceive(
        id, "fakeActor1", arrayProperties));
      dispatch(actions.messageObjectPropertiesReceive(
        id, "fakeActor2", arraySubProperties));

      const logCount = logLimit + 1;
      const packet = clonePacket(stubPackets.get(
        "console.assert(false, {message: 'foobar'})"));
      const secondMessageActor = packet.message.arguments[0].actor;

      for (let i = 1; i <= logCount; i++) {
        packet.message.arguments.push(`message num ${i}`);
        dispatch(actions.messageAdd(packet));
      }

      expect(releasedActors.length).toBe(4);
      expect(releasedActors).toInclude(firstMessageActor);
      expect(releasedActors).toInclude("fakeActor1");
      expect(releasedActors).toInclude("fakeActor2");
      expect(releasedActors).toInclude(secondMessageActor);
    });

    it("properly releases backend actors after clear", () => {
      let releasedActors = [];
      const { dispatch, getState } = setupStore([], {
        proxy: {
          releaseActor: (actor) => {
            releasedActors.push(actor);
          }
        }
      });

      // Add a log message with loaded object properties.
      dispatch(actions.messageAdd(
        stubPackets.get("console.log('myarray', ['red', 'green', 'blue'])")));

      let messages = getAllMessagesById(getState());
      const firstMessage = messages.first();
      const firstMessageActor = firstMessage.parameters[1].actor;
      const arrayProperties = Symbol();
      const arraySubProperties = Symbol();
      const [id] = [...messages.keys()];
      dispatch(actions.messageObjectPropertiesReceive(
        id, "fakeActor1", arrayProperties));
      dispatch(actions.messageObjectPropertiesReceive(
        id, "fakeActor2", arraySubProperties));

      const packet = clonePacket(stubPackets.get(
        "console.assert(false, {message: 'foobar'})"));
      const secondMessageActor = packet.message.arguments[0].actor;
      dispatch(actions.messageAdd(packet));

      dispatch(actions.messagesClear());

      expect(releasedActors.length).toBe(4);
      expect(releasedActors).toInclude(firstMessageActor);
      expect(releasedActors).toInclude("fakeActor1");
      expect(releasedActors).toInclude("fakeActor2");
      expect(releasedActors).toInclude(secondMessageActor);
    });
  });
});
