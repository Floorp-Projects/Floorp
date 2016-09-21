/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const {
  getAllMessages,
  getAllMessagesUiById,
} = require("devtools/client/webconsole/new-console-output/selectors/messages");
const {
  setupActions,
  setupStore
} = require("devtools/client/webconsole/new-console-output/test/helpers");
const { stubPackets, stubPreparedMessages } = require("devtools/client/webconsole/new-console-output/test/fixtures/stubs/index");

const expect = require("expect");

describe("Message reducer:", () => {
  let actions;

  before(() => {
    actions = setupActions();
  });

  describe("messagesById", () => {
    it("adds a message to an empty store", () => {
      const { dispatch, getState } = setupStore([]);

      const packet = stubPackets.get("console.log('foobar', 'test')");
      const message = stubPreparedMessages.get("console.log('foobar', 'test')");
      dispatch(actions.messageAdd(packet));

      const messages = getAllMessages(getState());

      expect(messages.first()).toEqual(message);
    });

    it("increments repeat on a repeating message", () => {
      const { dispatch, getState } = setupStore([
        "console.log('foobar', 'test')",
        "console.log('foobar', 'test')"
      ]);

      const packet = stubPackets.get("console.log('foobar', 'test')");
      dispatch(actions.messageAdd(packet));
      dispatch(actions.messageAdd(packet));

      const messages = getAllMessages(getState());

      expect(messages.size).toBe(1);
      expect(messages.first().repeat).toBe(4);
    });

    it("does not clobber a unique message", () => {
      const { dispatch, getState } = setupStore([
        "console.log('foobar', 'test')",
        "console.log('foobar', 'test')"
      ]);

      const packet = stubPackets.get("console.log('foobar', 'test')");
      dispatch(actions.messageAdd(packet));

      const packet2 = stubPackets.get("console.log(undefined)");
      dispatch(actions.messageAdd(packet2));

      const messages = getAllMessages(getState());

      expect(messages.size).toBe(2);
      expect(messages.first().repeat).toBe(3);
      expect(messages.last().repeat).toBe(1);
    });

    it("adds a message in response to console.clear()", () => {
      const { dispatch, getState } = setupStore([]);

      dispatch(actions.messageAdd(stubPackets.get("console.clear()")));

      const messages = getAllMessages(getState());

      expect(messages.size).toBe(1);
      expect(messages.first().parameters[0]).toBe("Console was cleared.");
    });

    it("clears the messages list in response to MESSAGES_CLEAR action", () => {
      const { dispatch, getState } = setupStore([
        "console.log('foobar', 'test')",
        "console.log(undefined)"
      ]);

      dispatch(actions.messagesClear());

      const messages = getAllMessages(getState());
      expect(messages.size).toBe(0);
    });

    it("limits the number of messages displayed", () => {
      const { dispatch, getState } = setupStore([]);

      const logLimit = 1000;
      const packet = stubPackets.get("console.log(undefined)");
      for (let i = 1; i <= logLimit + 1; i++) {
        packet.message.arguments = [`message num ${i}`];
        dispatch(actions.messageAdd(packet));
      }

      const messages = getAllMessages(getState());
      expect(messages.count()).toBe(logLimit);
      expect(messages.first().parameters[0]).toBe(`message num 2`);
      expect(messages.last().parameters[0]).toBe(`message num ${logLimit + 1}`);
    });

    it("does not add null messages to the store", () => {
      const { dispatch, getState } = setupStore([]);

      const message = stubPackets.get("console.time('bar')");
      dispatch(actions.messageAdd(message));

      const messages = getAllMessages(getState());
      expect(messages.size).toBe(0);
    });
  });

  describe("messagesUiById", () => {
    it("opens console.trace messages when they are added", () => {
      const { dispatch, getState } = setupStore([]);

      const message = stubPackets.get("console.trace()");
      dispatch(actions.messageAdd(message));

      const messages = getAllMessages(getState());
      const messagesUi = getAllMessagesUiById(getState());
      expect(messagesUi.size).toBe(1);
      expect(messagesUi.first()).toBe(messages.first().id);
    });

    it("clears the messages UI list in response to MESSAGES_CLEAR action", () => {
      const { dispatch, getState } = setupStore([
        "console.log('foobar', 'test')",
        "console.log(undefined)"
      ]);

      const traceMessage = stubPackets.get("console.trace()");
      dispatch(actions.messageAdd(traceMessage));

      dispatch(actions.messagesClear());

      const messagesUi = getAllMessagesUiById(getState());
      expect(messagesUi.size).toBe(0);
    });
  });
});
