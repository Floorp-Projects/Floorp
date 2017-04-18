/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const {
  getAllMessages,
  getAllMessagesUiById,
  getAllGroupsById,
  getCurrentGroup,
} = require("devtools/client/webconsole/new-console-output/selectors/messages");
const {
  setupActions,
  setupStore
} = require("devtools/client/webconsole/new-console-output/test/helpers");
const { stubPackets, stubPreparedMessages } = require("devtools/client/webconsole/new-console-output/test/fixtures/stubs/index");
const {
  MESSAGE_TYPE,
} = require("devtools/client/webconsole/new-console-output/constants");

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

    it("adds console.table call with unsupported type as console.log", () => {
      const { dispatch, getState } = setupStore([]);

      const packet = stubPackets.get("console.table('bar')");
      dispatch(actions.messageAdd(packet));

      const messages = getAllMessages(getState());
      const tableMessage = messages.last();
      expect(tableMessage.level).toEqual(MESSAGE_TYPE.LOG);
    });

    it("adds console.group messages to the store", () => {
      const { dispatch, getState } = setupStore([]);

      const message = stubPackets.get("console.group('bar')");
      dispatch(actions.messageAdd(message));

      const messages = getAllMessages(getState());
      expect(messages.size).toBe(1);
    });

    it("sets groupId property as expected", () => {
      const { dispatch, getState } = setupStore([]);

      dispatch(actions.messageAdd(
        stubPackets.get("console.group('bar')")));

      const packet = stubPackets.get("console.log('foobar', 'test')");
      dispatch(actions.messageAdd(packet));

      const messages = getAllMessages(getState());
      expect(messages.size).toBe(2);
      expect(messages.last().groupId).toBe(messages.first().id);
    });

    it("does not display console.groupEnd messages to the store", () => {
      const { dispatch, getState } = setupStore([]);

      const message = stubPackets.get("console.groupEnd('bar')");
      dispatch(actions.messageAdd(message));

      const messages = getAllMessages(getState());
      expect(messages.size).toBe(0);
    });

    it("filters out message added after a console.groupCollapsed message", () => {
      const { dispatch, getState } = setupStore([]);

      const message = stubPackets.get("console.groupCollapsed('foo')");
      dispatch(actions.messageAdd(message));

      dispatch(actions.messageAdd(
        stubPackets.get("console.log('foobar', 'test')")));

      const messages = getAllMessages(getState());
      expect(messages.size).toBe(1);
    });

    it("shows the group of the first displayed message when messages are pruned", () => {
      const { dispatch, getState } = setupStore([]);

      const logLimit = 1000;

      const groupMessage = stubPreparedMessages.get("console.group('bar')");
      dispatch(actions.messageAdd(
        stubPackets.get("console.group('bar')")));

      const packet = stubPackets.get("console.log(undefined)");
      for (let i = 1; i <= logLimit + 1; i++) {
        packet.message.arguments = [`message num ${i}`];
        dispatch(actions.messageAdd(packet));
      }

      const messages = getAllMessages(getState());
      expect(messages.count()).toBe(logLimit);
      expect(messages.first().messageText).toBe(groupMessage.messageText);
      expect(messages.get(1).parameters[0]).toBe(`message num 3`);
      expect(messages.last().parameters[0]).toBe(`message num ${logLimit + 1}`);
    });

    it("adds console.dirxml call as console.log", () => {
      const { dispatch, getState } = setupStore([]);

      const packet = stubPackets.get("console.dirxml(window)");
      dispatch(actions.messageAdd(packet));

      const messages = getAllMessages(getState());
      const dirxmlMessage = messages.last();
      expect(dirxmlMessage.level).toEqual(MESSAGE_TYPE.LOG);
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

    it("opens console.group messages when they are added", () => {
      const { dispatch, getState } = setupStore([]);

      const message = stubPackets.get("console.group('bar')");
      dispatch(actions.messageAdd(message));

      const messages = getAllMessages(getState());
      const messagesUi = getAllMessagesUiById(getState());
      expect(messagesUi.size).toBe(1);
      expect(messagesUi.first()).toBe(messages.first().id);
    });

    it("does not open console.groupCollapsed messages when they are added", () => {
      const { dispatch, getState } = setupStore([]);

      const message = stubPackets.get("console.groupCollapsed('foo')");
      dispatch(actions.messageAdd(message));

      const messagesUi = getAllMessagesUiById(getState());
      expect(messagesUi.size).toBe(0);
    });
  });

  describe("currentGroup", () => {
    it("sets the currentGroup when console.group message is added", () => {
      const { dispatch, getState } = setupStore([]);

      const packet = stubPackets.get("console.group('bar')");
      dispatch(actions.messageAdd(packet));

      const messages = getAllMessages(getState());
      const currentGroup = getCurrentGroup(getState());
      expect(currentGroup).toBe(messages.first().id);
    });

    it("sets currentGroup to expected value when console.groupEnd is added", () => {
      const { dispatch, getState } = setupStore([
        "console.group('bar')",
        "console.groupCollapsed('foo')"
      ]);

      let messages = getAllMessages(getState());
      let currentGroup = getCurrentGroup(getState());
      expect(currentGroup).toBe(messages.last().id);

      const endFooPacket = stubPackets.get("console.groupEnd('foo')");
      dispatch(actions.messageAdd(endFooPacket));
      messages = getAllMessages(getState());
      currentGroup = getCurrentGroup(getState());
      expect(currentGroup).toBe(messages.first().id);

      const endBarPacket = stubPackets.get("console.groupEnd('bar')");
      dispatch(actions.messageAdd(endBarPacket));
      messages = getAllMessages(getState());
      currentGroup = getCurrentGroup(getState());
      expect(currentGroup).toBe(null);
    });

    it("resets the currentGroup to null in response to MESSAGES_CLEAR action", () => {
      const { dispatch, getState } = setupStore([
        "console.group('bar')"
      ]);

      dispatch(actions.messagesClear());

      const currentGroup = getCurrentGroup(getState());
      expect(currentGroup).toBe(null);
    });
  });

  describe("groupsById", () => {
    it("adds the group with expected array when console.group message is added", () => {
      const { dispatch, getState } = setupStore([]);

      const barPacket = stubPackets.get("console.group('bar')");
      dispatch(actions.messageAdd(barPacket));

      let messages = getAllMessages(getState());
      let groupsById = getAllGroupsById(getState());
      expect(groupsById.size).toBe(1);
      expect(groupsById.has(messages.first().id)).toBe(true);
      expect(groupsById.get(messages.first().id)).toEqual([]);

      const fooPacket = stubPackets.get("console.groupCollapsed('foo')");
      dispatch(actions.messageAdd(fooPacket));
      messages = getAllMessages(getState());
      groupsById = getAllGroupsById(getState());
      expect(groupsById.size).toBe(2);
      expect(groupsById.has(messages.last().id)).toBe(true);
      expect(groupsById.get(messages.last().id)).toEqual([messages.first().id]);
    });

    it("resets groupsById in response to MESSAGES_CLEAR action", () => {
      const { dispatch, getState } = setupStore([
        "console.group('bar')",
        "console.groupCollapsed('foo')",
      ]);

      let groupsById = getAllGroupsById(getState());
      expect(groupsById.size).toBe(2);

      dispatch(actions.messagesClear());

      groupsById = getAllGroupsById(getState());
      expect(groupsById.size).toBe(0);
    });
  });
});
