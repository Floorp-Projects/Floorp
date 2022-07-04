/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const {
  getFirstMessage,
  setupActions,
  setupStore,
} = require("devtools/client/webconsole/test/node/helpers");

const {
  stubPackets,
} = require("devtools/client/webconsole/test/node/fixtures/stubs/index");
const expect = require("expect");

describe("Release actor enhancer:", () => {
  let actions;

  before(() => {
    actions = setupActions();
  });

  describe("release", () => {
    it("releases backend actors when limit reached adding a single message", () => {
      const logLimit = 100;
      const releasedActors = [];
      const mockFrontRelease = function() {
        releasedActors.push(this.actorID);
      };

      const { dispatch, getState } = setupStore([], {
        storeOptions: { logLimit },
      });

      // Add a log message.
      const packet = stubPackets.get(
        "console.log('myarray', ['red', 'green', 'blue'])"
      );
      packet.message.arguments[1].release = mockFrontRelease;
      dispatch(actions.messagesAdd([packet]));

      const firstMessage = getFirstMessage(getState());
      const firstMessageActor = firstMessage.parameters[1].actorID;

      // Add an evaluation result message (see Bug 1408321).
      const evaluationResultPacket = stubPackets.get("new Date(0)");
      evaluationResultPacket.result.release = mockFrontRelease;
      dispatch(actions.messagesAdd([evaluationResultPacket]));
      const secondMessageActor = evaluationResultPacket.result.actorID;

      const logCount = logLimit + 1;
      const assertPacket = stubPackets.get(
        "console.assert(false, {message: 'foobar'})"
      );
      assertPacket.message.arguments[0].release = mockFrontRelease;
      const thirdMessageActor = assertPacket.message.arguments[0].actorID;

      for (let i = 1; i <= logCount; i++) {
        assertPacket.message.arguments.push(`message num ${i}`);
        dispatch(actions.messagesAdd([assertPacket]));
      }

      expect(releasedActors.length).toBe(3);
      expect(releasedActors).toInclude(firstMessageActor);
      expect(releasedActors).toInclude(secondMessageActor);
      expect(releasedActors).toInclude(thirdMessageActor);
    });

    it("releases backend actors when limit reached adding multiple messages", () => {
      const logLimit = 100;
      const releasedActors = [];
      const { dispatch, getState } = setupStore([], {
        storeOptions: { logLimit },
      });

      const mockFrontRelease = function() {
        releasedActors.push(this.actorID);
      };

      // Add a log message.
      const logPacket = stubPackets.get(
        "console.log('myarray', ['red', 'green', 'blue'])"
      );
      logPacket.message.arguments[1].release = mockFrontRelease;
      dispatch(actions.messagesAdd([logPacket]));

      const firstMessage = getFirstMessage(getState());
      const firstMessageActor = firstMessage.parameters[1].actorID;

      // Add an evaluation result message (see Bug 1408321).
      const evaluationResultPacket = stubPackets.get("new Date(0)");
      evaluationResultPacket.result.release = mockFrontRelease;
      dispatch(actions.messagesAdd([evaluationResultPacket]));
      const secondMessageActor = evaluationResultPacket.result.actorID;

      // Add an assertion message.
      const assertPacket = stubPackets.get(
        "console.assert(false, {message: 'foobar'})"
      );
      assertPacket.message.arguments[0].release = mockFrontRelease;
      dispatch(actions.messagesAdd([assertPacket]));
      const thirdMessageActor = assertPacket.message.arguments[0].actorID;

      // Add ${logLimit} messages so we prune the ones we added before.
      const packets = [];
      // Alternate between 2 packets so we don't trigger the repeat message mechanism.
      const oddPacket = stubPackets.get("console.log(undefined)");
      const evenPacket = stubPackets.get("console.log('foobar', 'test')");
      for (let i = 0; i < logLimit; i++) {
        const packet = i % 2 === 0 ? evenPacket : oddPacket;
        packets.push(packet);
      }

      // Add all the packets at once. This will prune the first 3 messages.
      dispatch(actions.messagesAdd(packets));

      expect(releasedActors.length).toBe(3);
      expect(releasedActors).toInclude(firstMessageActor);
      expect(releasedActors).toInclude(secondMessageActor);
      expect(releasedActors).toInclude(thirdMessageActor);
    });

    it("properly releases backend actors after clear", () => {
      const releasedActors = [];
      const { dispatch, getState } = setupStore([]);

      const mockFrontRelease = function() {
        releasedActors.push(this.actorID);
      };

      // Add a log message.
      const logPacket = stubPackets.get(
        "console.log('myarray', ['red', 'green', 'blue'])"
      );
      logPacket.message.arguments[1].release = mockFrontRelease;
      dispatch(actions.messagesAdd([logPacket]));

      const firstMessage = getFirstMessage(getState());
      const firstMessageActor = firstMessage.parameters[1].actorID;

      // Add an assertion message.
      const assertPacket = stubPackets.get(
        "console.assert(false, {message: 'foobar'})"
      );
      assertPacket.message.arguments[0].release = mockFrontRelease;
      dispatch(actions.messagesAdd([assertPacket]));
      const secondMessageActor = assertPacket.message.arguments[0].actorID;

      // Add an evaluation result message (see Bug 1408321).
      const evaluationResultPacket = stubPackets.get("new Date(0)");
      evaluationResultPacket.result.release = mockFrontRelease;
      dispatch(actions.messagesAdd([evaluationResultPacket]));
      const thirdMessageActor = evaluationResultPacket.result.actorID;

      // Add a message with a long string messageText property.
      const longStringPacket = stubPackets.get("TypeError longString message");
      longStringPacket.pageError.errorMessage.release = mockFrontRelease;
      dispatch(actions.messagesAdd([longStringPacket]));
      const fourthMessageActor =
        longStringPacket.pageError.errorMessage.actorID;

      // Kick-off the actor release.
      dispatch(actions.messagesClear());

      expect(releasedActors.length).toBe(4);
      expect(releasedActors).toInclude(firstMessageActor);
      expect(releasedActors).toInclude(secondMessageActor);
      expect(releasedActors).toInclude(thirdMessageActor);
      expect(releasedActors).toInclude(fourthMessageActor);
    });
  });
});
