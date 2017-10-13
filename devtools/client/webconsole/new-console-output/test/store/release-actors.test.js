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

      // Add a log message.
      dispatch(actions.messageAdd(
        stubPackets.get("console.log('myarray', ['red', 'green', 'blue'])")));

      let messages = getAllMessagesById(getState());
      const firstMessage = messages.first();
      const firstMessageActor = firstMessage.parameters[1].actor;

      // Add an evaluation result message (see Bug 1408321).
      const evaluationResultPacket = stubPackets.get("new Date(0)");
      dispatch(actions.messageAdd(evaluationResultPacket));
      const secondMessageActor = evaluationResultPacket.result.actor;

      const logCount = logLimit + 1;
      const packet = clonePacket(stubPackets.get(
        "console.assert(false, {message: 'foobar'})"));
      const thirdMessageActor = packet.message.arguments[0].actor;

      for (let i = 1; i <= logCount; i++) {
        packet.message.arguments.push(`message num ${i}`);
        dispatch(actions.messageAdd(packet));
      }

      expect(releasedActors.length).toBe(3);
      expect(releasedActors).toInclude(firstMessageActor);
      expect(releasedActors).toInclude(secondMessageActor);
      expect(releasedActors).toInclude(thirdMessageActor);
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

      // Add a log message.
      dispatch(actions.messageAdd(
        stubPackets.get("console.log('myarray', ['red', 'green', 'blue'])")));

      let messages = getAllMessagesById(getState());
      const firstMessage = messages.first();
      const firstMessageActor = firstMessage.parameters[1].actor;

      const packet = clonePacket(stubPackets.get(
        "console.assert(false, {message: 'foobar'})"));
      const secondMessageActor = packet.message.arguments[0].actor;
      dispatch(actions.messageAdd(packet));

      // Add an evaluation result message (see Bug 1408321).
      const evaluationResultPacket = stubPackets.get("new Date(0)");
      dispatch(actions.messageAdd(evaluationResultPacket));
      const thirdMessageActor = evaluationResultPacket.result.actor;

      // Kick-off the actor release.
      dispatch(actions.messagesClear());

      expect(releasedActors.length).toBe(3);
      expect(releasedActors).toInclude(firstMessageActor);
      expect(releasedActors).toInclude(secondMessageActor);
      expect(releasedActors).toInclude(thirdMessageActor);
    });
  });
});
