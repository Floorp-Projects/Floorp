/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const {
  getAllMessagesUiById,
  getAllMessagesPayloadById,
  getAllNetworkMessagesUpdateById,
  getAllRepeatById,
  getCurrentGroup,
  getGroupsById,
  getAllMessagesById,
  getVisibleMessages,
} = require("devtools/client/webconsole/selectors/messages");
const {
  ensureExecutionPoint,
} = require("devtools/client/webconsole/reducers/messages");

const {
  clonePacket,
  getFirstMessage,
  getLastMessage,
  getMessageAt,
  setupActions,
  setupStore,
} = require("devtools/client/webconsole/test/node/helpers");
const {
  stubPackets,
  stubPreparedMessages,
} = require("devtools/client/webconsole/test/node/fixtures/stubs/index");
const { MESSAGE_TYPE } = require("devtools/client/webconsole/constants");

const expect = require("expect");

describe("Message reducer:", () => {
  let actions;

  before(() => {
    actions = setupActions();
  });

  describe("messagesById", () => {
    it("adds a message to an empty store", () => {
      const { dispatch, getState } = setupStore();

      // Retrieve the store before adding the message to not pollute the
      // ensureExecutionPoint results.
      const state = getState();

      const packet = stubPackets.get("console.log('foobar', 'test')");
      dispatch(actions.messagesAdd([packet]));

      const message = stubPreparedMessages.get("console.log('foobar', 'test')");
      ensureExecutionPoint(state.messages, message);

      expect(getFirstMessage(getState())).toEqual(message);
    });

    it("increments repeat on a repeating log message", () => {
      const key1 = "console.log('foobar', 'test')";
      const { dispatch, getState } = setupStore([key1, key1], { actions });

      const packet = clonePacket(stubPackets.get(key1));
      const packet2 = clonePacket(packet);

      // Repeat ID must be the same even if the timestamp is different.
      packet.message.timeStamp = 1;
      packet2.message.timeStamp = 2;
      dispatch(actions.messagesAdd([packet, packet2]));

      const messages = getAllMessagesById(getState());

      expect(messages.size).toBe(1);
      const repeat = getAllRepeatById(getState());
      expect(repeat[getFirstMessage(getState()).id]).toBe(4);
    });

    it("doesn't increment repeat on same log message with different locations", () => {
      const key1 = "console.log('foobar', 'test')";
      const { dispatch, getState } = setupStore();

      const packet = clonePacket(stubPackets.get(key1));

      // Dispatch original packet.
      dispatch(actions.messagesAdd([packet]));

      // Dispatch same packet with modified column number.
      packet.message.columnNumber = packet.message.columnNumber + 1;
      dispatch(actions.messagesAdd([packet]));

      // Dispatch same packet with modified line number.
      packet.message.lineNumber = packet.message.lineNumber + 1;
      dispatch(actions.messagesAdd([packet]));

      const messages = getAllMessagesById(getState());

      expect(messages.size).toBe(3);

      const repeat = getAllRepeatById(getState());
      expect(Object.keys(repeat).length).toBe(0);
    });

    it("increments repeat on a repeating css message", () => {
      const key1 =
        "Unknown property ‘such-unknown-property’.  Declaration dropped.";
      const { dispatch, getState } = setupStore([key1, key1]);

      const packet = clonePacket(stubPackets.get(key1));

      // Repeat ID must be the same even if the timestamp is different.
      packet.pageError.timeStamp = 1;
      dispatch(actions.messagesAdd([packet]));
      packet.pageError.timeStamp = 2;
      dispatch(actions.messagesAdd([packet]));

      const messages = getAllMessagesById(getState());

      expect(messages.size).toBe(1);

      const repeat = getAllRepeatById(getState());
      expect(repeat[getFirstMessage(getState()).id]).toBe(4);
    });

    it("doesn't increment repeat on same css message with different locations", () => {
      const key1 =
        "Unknown property ‘such-unknown-property’.  Declaration dropped.";
      const { dispatch, getState } = setupStore();

      const packet = clonePacket(stubPackets.get(key1));

      // Dispatch original packet.
      dispatch(actions.messagesAdd([packet]));

      // Dispatch same packet with modified column number.
      packet.pageError.columnNumber = packet.pageError.columnNumber + 1;
      dispatch(actions.messagesAdd([packet]));

      // Dispatch same packet with modified line number.
      packet.pageError.lineNumber = packet.pageError.lineNumber + 1;
      dispatch(actions.messagesAdd([packet]));

      const messages = getAllMessagesById(getState());

      expect(messages.size).toBe(3);

      const repeat = getAllRepeatById(getState());
      expect(Object.keys(repeat).length).toBe(0);
    });

    it("increments repeat on a repeating error message", () => {
      const key1 = "ReferenceError: asdf is not defined";
      const { dispatch, getState } = setupStore([key1, key1]);

      const packet = clonePacket(stubPackets.get(key1));

      // Repeat ID must be the same even if the timestamp is different.
      packet.pageError.timeStamp = 1;
      dispatch(actions.messagesAdd([packet]));
      packet.pageError.timeStamp = 2;
      dispatch(actions.messagesAdd([packet]));

      const messages = getAllMessagesById(getState());

      expect(messages.size).toBe(1);

      const repeat = getAllRepeatById(getState());
      expect(repeat[getFirstMessage(getState()).id]).toBe(4);
    });

    it("does not increment repeat after closing a group", () => {
      const logKey = "console.log('foobar', 'test')";
      const { getState } = setupStore([
        logKey,
        logKey,
        "console.group('bar')",
        logKey,
        logKey,
        logKey,
        "console.groupEnd()",
        logKey,
      ]);

      const messages = getAllMessagesById(getState());

      expect(messages.size).toBe(4);
      const repeat = getAllRepeatById(getState());
      expect(repeat[getFirstMessage(getState()).id]).toBe(2);
      expect(repeat[getMessageAt(getState(), 2).id]).toBe(3);
      expect(repeat[getLastMessage(getState()).id]).toBe(undefined);
    });

    it("doesn't increment undefined messages coming from different places", () => {
      const { getState } = setupStore(["console.log(undefined)", "undefined"]);

      const messages = getAllMessagesById(getState());
      expect(messages.size).toBe(2);

      const repeat = getAllRepeatById(getState());
      expect(Object.keys(repeat).length).toBe(0);
    });

    it("doesn't increment successive falsy but different messages", () => {
      const { getState } = setupStore(
        ["console.log(NaN)", "console.log(undefined)", "console.log(null)"],
        { actions }
      );

      const messages = getAllMessagesById(getState());
      expect(messages.size).toBe(3);
      const repeat = getAllRepeatById(getState());
      expect(Object.keys(repeat).length).toBe(0);
    });

    it("increment falsy messages when expected", () => {
      const { dispatch, getState } = setupStore();

      const nanPacket = stubPackets.get("console.log(NaN)");
      dispatch(actions.messagesAdd([nanPacket, nanPacket]));
      let messages = getAllMessagesById(getState());
      expect(messages.size).toBe(1);
      let repeat = getAllRepeatById(getState());
      expect(repeat[getLastMessage(getState()).id]).toBe(2);

      const undefinedPacket = stubPackets.get("console.log(undefined)");
      dispatch(actions.messagesAdd([undefinedPacket, undefinedPacket]));
      messages = getAllMessagesById(getState());
      expect(messages.size).toBe(2);
      repeat = getAllRepeatById(getState());
      expect(repeat[getLastMessage(getState()).id]).toBe(2);

      const nullPacket = stubPackets.get("console.log(null)");
      dispatch(actions.messagesAdd([nullPacket, nullPacket]));
      messages = getAllMessagesById(getState());
      expect(messages.size).toBe(3);
      repeat = getAllRepeatById(getState());
      expect(repeat[getLastMessage(getState()).id]).toBe(2);
    });

    it("does not clobber a unique message", () => {
      const key1 = "console.log('foobar', 'test')";
      const { dispatch, getState } = setupStore([key1, key1]);

      const packet = stubPackets.get(key1);
      dispatch(actions.messagesAdd([packet]));

      const packet2 = stubPackets.get("console.log(undefined)");
      dispatch(actions.messagesAdd([packet2]));

      const messages = getAllMessagesById(getState());
      expect(messages.size).toBe(2);

      const repeat = getAllRepeatById(getState());
      expect(repeat[getFirstMessage(getState()).id]).toBe(3);
      expect(repeat[getLastMessage(getState()).id]).toBe(undefined);
    });

    it("adds a message in response to console.clear()", () => {
      const { dispatch, getState } = setupStore([]);

      dispatch(actions.messagesAdd([stubPackets.get("console.clear()")]));

      const messages = getAllMessagesById(getState());

      expect(messages.size).toBe(1);
      expect(getFirstMessage(getState()).parameters[0]).toBe(
        "Console was cleared."
      );
    });

    it("clears the messages list in response to MESSAGES_CLEAR action", () => {
      const { dispatch, getState } = setupStore([
        "console.log('foobar', 'test')",
        "console.log('foobar', 'test')",
        "console.log(undefined)",
        "console.table(['red', 'green', 'blue']);",
        "console.group('bar')",
      ]);

      dispatch(actions.messagesClear());

      const state = getState();
      expect(getAllMessagesById(state).size).toBe(0);
      expect(getVisibleMessages(state).length).toBe(0);
      expect(getAllMessagesUiById(state).length).toBe(0);
      expect(getGroupsById(state).size).toBe(0);
      expect(getAllMessagesPayloadById(state).size).toBe(0);
      expect(getCurrentGroup(state)).toBe(null);
      expect(getAllRepeatById(state)).toEqual({});
    });

    it("cleans the repeatsById object when messages are pruned", () => {
      const { dispatch, getState } = setupStore(
        [
          "console.log('foobar', 'test')",
          "console.log('foobar', 'test')",
          "console.log(undefined)",
          "console.log(undefined)",
        ],
        {
          actions,
          storeOptions: {
            logLimit: 2,
          },
        }
      );

      // Check that we have the expected data.
      let repeats = getAllRepeatById(getState());
      expect(Object.keys(repeats).length).toBe(2);
      const lastMessageId = getLastMessage(getState()).id;

      // This addition will prune the first message out of the store.
      let packet = stubPackets.get("console.log('foobar', 'test')");
      dispatch(actions.messagesAdd([packet]));

      repeats = getAllRepeatById(getState());

      // There should be only the data for the "undefined" message.
      expect(Object.keys(repeats)).toEqual([lastMessageId]);
      expect(Object.keys(repeats).length).toBe(1);
      expect(repeats[lastMessageId]).toBe(2);

      // This addition will prune the first message out of the store.
      packet = stubPackets.get("console.log(undefined)");
      dispatch(actions.messagesAdd([packet]));

      // repeatById should now be empty.
      expect(getAllRepeatById(getState())).toEqual({});
    });

    it("properly limits number of messages", () => {
      const logLimit = 1000;
      const { dispatch, getState } = setupStore([], {
        storeOptions: {
          logLimit,
        },
      });

      const packet = clonePacket(stubPackets.get("console.log(undefined)"));

      for (let i = 1; i <= logLimit + 2; i++) {
        packet.message.arguments = [`message num ${i}`];
        dispatch(actions.messagesAdd([packet]));
      }

      const messages = getAllMessagesById(getState());
      expect(messages.size).toBe(logLimit);
      expect(getFirstMessage(getState()).parameters[0]).toBe(`message num 3`);
      expect(getLastMessage(getState()).parameters[0]).toBe(
        `message num ${logLimit + 2}`
      );
    });

    it("properly limits number of messages when there are nested groups", () => {
      const logLimit = 1000;
      const { dispatch, getState } = setupStore([], {
        storeOptions: {
          logLimit,
        },
      });

      const packet = clonePacket(stubPackets.get("console.log(undefined)"));
      const packetGroup = clonePacket(stubPackets.get("console.group('bar')"));
      const packetGroupEnd = clonePacket(stubPackets.get("console.groupEnd()"));

      packetGroup.message.arguments = [`group-1`];
      dispatch(actions.messagesAdd([packetGroup]));
      packetGroup.message.arguments = [`group-1-1`];
      dispatch(actions.messagesAdd([packetGroup]));
      packetGroup.message.arguments = [`group-1-1-1`];
      dispatch(actions.messagesAdd([packetGroup]));
      packet.message.arguments = [`message-in-group-1`];
      dispatch(actions.messagesAdd([packet]));
      packet.message.arguments = [`message-in-group-2`];
      dispatch(actions.messagesAdd([packet]));
      // Closing group-1-1-1
      dispatch(actions.messagesAdd([packetGroupEnd]));
      // Closing group-1-1
      dispatch(actions.messagesAdd([packetGroupEnd]));
      // Closing group-1
      dispatch(actions.messagesAdd([packetGroupEnd]));

      for (let i = 0; i < logLimit; i++) {
        packet.message.arguments = [`message-${i}`];
        dispatch(actions.messagesAdd([packet]));
      }

      const visibleMessages = getVisibleMessages(getState());
      const messages = getAllMessagesById(getState());

      expect(messages.size).toBe(logLimit);
      expect(visibleMessages.length).toBe(logLimit);
      expect(messages.get(visibleMessages[0]).parameters[0]).toBe(`message-0`);
      expect(messages.get(visibleMessages[logLimit - 1]).parameters[0]).toBe(
        `message-${logLimit - 1}`
      );

      // The groups were cleaned up.
      const groups = getGroupsById(getState());
      expect(groups.size).toBe(0);
    });

    it("properly limits number of groups", () => {
      const logLimit = 100;
      const { dispatch, getState } = setupStore([], {
        storeOptions: { logLimit },
      });

      const packet = clonePacket(stubPackets.get("console.log(undefined)"));
      const packetGroup = clonePacket(stubPackets.get("console.group('bar')"));
      const packetGroupEnd = clonePacket(stubPackets.get("console.groupEnd()"));

      for (let i = 0; i < logLimit + 2; i++) {
        dispatch(actions.messagesAdd([packetGroup]));
        packet.message.arguments = [`message-${i}-a`];
        dispatch(actions.messagesAdd([packet]));
        packet.message.arguments = [`message-${i}-b`];
        dispatch(actions.messagesAdd([packet]));
        dispatch(actions.messagesAdd([packetGroupEnd]));
      }

      const visibleMessages = getVisibleMessages(getState());
      const messages = getAllMessagesById(getState());
      // We should have three times the logLimit since each group has one message inside.
      expect(messages.size).toBe(logLimit * 3);

      // We should have logLimit number of groups
      const groups = getGroupsById(getState());
      expect(groups.size).toBe(logLimit);

      expect(messages.get(visibleMessages[1]).parameters[0]).toBe(
        `message-2-a`
      );
      expect(getLastMessage(getState()).parameters[0]).toBe(
        `message-${logLimit + 1}-b`
      );
    });

    it("properly limits number of collapsed groups", () => {
      const logLimit = 100;
      const { dispatch, getState } = setupStore([], {
        storeOptions: { logLimit },
      });

      const packet = clonePacket(stubPackets.get("console.log(undefined)"));
      const packetGroupCollapsed = clonePacket(
        stubPackets.get("console.groupCollapsed('foo')")
      );
      const packetGroupEnd = clonePacket(stubPackets.get("console.groupEnd()"));

      for (let i = 0; i < logLimit + 2; i++) {
        packetGroupCollapsed.message.arguments = [`group-${i}`];
        dispatch(actions.messagesAdd([packetGroupCollapsed]));
        packet.message.arguments = [`message-${i}-a`];
        dispatch(actions.messagesAdd([packet]));
        packet.message.arguments = [`message-${i}-b`];
        dispatch(actions.messagesAdd([packet]));
        dispatch(actions.messagesAdd([packetGroupEnd]));
      }

      const messages = getAllMessagesById(getState());
      // We should have three times the logLimit since each group has two message inside.
      expect(messages.size).toBe(logLimit * 3);

      // We should have logLimit number of groups
      const groups = getGroupsById(getState());
      expect(groups.size).toBe(logLimit);

      expect(getFirstMessage(getState()).parameters[0]).toBe(`group-2`);
      expect(getLastMessage(getState()).parameters[0]).toBe(
        `message-${logLimit + 1}-b`
      );

      const visibleMessages = getVisibleMessages(getState());
      expect(visibleMessages.length).toBe(logLimit);
      const lastVisibleMessageId = visibleMessages[visibleMessages.length - 1];
      expect(messages.get(lastVisibleMessageId).parameters[0]).toBe(
        `group-${logLimit + 1}`
      );
    });

    it("does not add null messages to the store", () => {
      const { dispatch, getState } = setupStore();

      const message = stubPackets.get("console.time('bar')");
      dispatch(actions.messagesAdd([message]));

      const messages = getAllMessagesById(getState());
      expect(messages.size).toBe(0);
    });

    it("adds console.table call with unsupported type as console.log", () => {
      const { dispatch, getState } = setupStore();

      const packet = stubPackets.get("console.table('bar')");
      dispatch(actions.messagesAdd([packet]));

      const tableMessage = getLastMessage(getState());
      expect(tableMessage.level).toEqual(MESSAGE_TYPE.LOG);
    });

    it("adds console.group messages to the store", () => {
      const { dispatch, getState } = setupStore();

      const message = stubPackets.get("console.group('bar')");
      dispatch(actions.messagesAdd([message]));

      const messages = getAllMessagesById(getState());
      expect(messages.size).toBe(1);
    });

    it("adds messages in console.group to the store", () => {
      const { dispatch, getState } = setupStore();

      const groupPacket = stubPackets.get("console.group('bar')");
      const groupEndPacket = stubPackets.get("console.groupEnd('bar')");
      const logPacket = stubPackets.get("console.log('foobar', 'test')");

      const packets = [
        groupPacket,
        logPacket,
        groupPacket,
        groupPacket,
        logPacket,
        groupEndPacket,
        logPacket,
        groupEndPacket,
        logPacket,
        groupEndPacket,
        logPacket,
      ];
      dispatch(actions.messagesAdd(packets));

      // Here is what we should have (8 messages)
      // ▼ bar
      // |  foobar test
      // |  ▼ bar
      // |  |  ▼ bar
      // |  |  |  foobar test
      // |  |  foobar test
      // |  foobar test
      // foobar test

      const isNotGroupEnd = p => p !== groupEndPacket;
      const messageCount = packets.filter(isNotGroupEnd).length;

      const messages = getAllMessagesById(getState());
      const visibleMessages = getVisibleMessages(getState());
      expect(messages.size).toBe(messageCount);
      expect(visibleMessages.length).toBe(messageCount);
    });

    it("sets groupId property as expected", () => {
      const { dispatch, getState } = setupStore();

      dispatch(
        actions.messagesAdd([
          stubPackets.get("console.group('bar')"),
          stubPackets.get("console.log('foobar', 'test')"),
        ])
      );

      const messages = getAllMessagesById(getState());
      expect(messages.size).toBe(2);
      expect(getLastMessage(getState()).groupId).toBe(
        getFirstMessage(getState()).id
      );
    });

    it("does not display console.groupEnd messages to the store", () => {
      const { dispatch, getState } = setupStore();

      const message = stubPackets.get("console.groupEnd('bar')");
      dispatch(actions.messagesAdd([message]));

      const messages = getAllMessagesById(getState());
      expect(messages.size).toBe(0);
    });

    it("filters out message added after a console.groupCollapsed message", () => {
      const { dispatch, getState } = setupStore();

      dispatch(
        actions.messagesAdd([
          stubPackets.get("console.groupCollapsed('foo')"),
          stubPackets.get("console.log('foobar', 'test')"),
        ])
      );

      const messages = getVisibleMessages(getState());
      expect(messages.length).toBe(1);
    });

    it("adds console.dirxml call as console.log", () => {
      const { dispatch, getState } = setupStore();

      const packet = stubPackets.get("console.dirxml(window)");
      dispatch(actions.messagesAdd([packet]));

      const dirxmlMessage = getLastMessage(getState());
      expect(dirxmlMessage.level).toEqual(MESSAGE_TYPE.LOG);
    });

    it("does not throw when adding incomplete console.count packet", () => {
      const { dispatch, getState } = setupStore();
      const packet = clonePacket(stubPackets.get(`console.count('bar')`));

      // Remove counter information to mimick packet we receive in the browser console.
      delete packet.message.counter;

      dispatch(actions.messagesAdd([packet]));
      // The message should not be added to the state.
      expect(getAllMessagesById(getState()).size).toBe(0);
    });
  });

  describe("expandedMessageIds", () => {
    it("opens console.trace messages when they are added", () => {
      const { dispatch, getState } = setupStore();

      const message = stubPackets.get("console.trace()");
      dispatch(actions.messagesAdd([message]));

      const expanded = getAllMessagesUiById(getState());
      expect(expanded.length).toBe(1);
      expect(expanded[0]).toBe(getFirstMessage(getState()).id);
    });

    it("clears the messages UI list in response to MESSAGES_CLEAR action", () => {
      const { dispatch, getState } = setupStore([
        "console.log('foobar', 'test')",
        "console.log(undefined)",
      ]);

      const traceMessage = stubPackets.get("console.trace()");
      dispatch(actions.messagesAdd([traceMessage]));

      dispatch(actions.messagesClear());

      const expanded = getAllMessagesUiById(getState());
      expect(expanded.length).toBe(0);
    });

    it("cleans the messages UI list when messages are pruned", () => {
      const { dispatch, getState } = setupStore(
        ["console.trace()", "console.log(undefined)", "console.trace()"],
        {
          storeOptions: {
            logLimit: 3,
          },
        }
      );

      // Check that we have the expected data.
      let expanded = getAllMessagesUiById(getState());
      expect(expanded.length).toBe(2);
      expect(expanded[0]).toBe(getFirstMessage(getState()).id);
      const lastMessageId = getLastMessage(getState()).id;
      expect(expanded[expanded.length - 1]).toBe(lastMessageId);

      // This addition will prune the first message out of the store.
      let packet = stubPackets.get("console.log(undefined)");
      dispatch(actions.messagesAdd([packet]));

      expanded = getAllMessagesUiById(getState());

      // There should be only the id of the last console.trace message.
      expect(expanded.length).toBe(1);
      expect(expanded[0]).toBe(lastMessageId);

      // These additions will prune the last console.trace message out of the store.
      packet = stubPackets.get("console.log('foobar', 'test')");
      dispatch(actions.messagesAdd([packet]));
      packet = stubPackets.get("console.log(undefined)");
      dispatch(actions.messagesAdd([packet]));

      // expandedMessageIds should now be empty.
      expect(getAllMessagesUiById(getState()).length).toBe(0);
    });

    it("opens console.group messages when they are added", () => {
      const { dispatch, getState } = setupStore();

      const message = stubPackets.get("console.group('bar')");
      dispatch(actions.messagesAdd([message]));

      const expanded = getAllMessagesUiById(getState());
      expect(expanded.length).toBe(1);
      expect(expanded[0]).toBe(getFirstMessage(getState()).id);
    });

    it("does not open console.groupCollapsed messages when they are added", () => {
      const { dispatch, getState } = setupStore();

      const message = stubPackets.get("console.groupCollapsed('foo')");
      dispatch(actions.messagesAdd([message]));

      const expanded = getAllMessagesUiById(getState());
      expect(expanded.length).toBe(0);
    });

    it("reacts to messageClose/messageOpen actions on console.group", () => {
      const { dispatch, getState } = setupStore(["console.group('bar')"]);
      const firstMessageId = getFirstMessage(getState()).id;

      let expanded = getAllMessagesUiById(getState());
      expect(expanded.length).toBe(1);
      expect(expanded[0]).toBe(firstMessageId);

      dispatch(actions.messageClose(firstMessageId));

      expanded = getAllMessagesUiById(getState());
      expect(expanded.length).toBe(0);

      dispatch(actions.messageOpen(firstMessageId));

      expanded = getAllMessagesUiById(getState());
      expect(expanded.length).toBe(1);
      expect(expanded[0]).toBe(firstMessageId);
    });

    it("reacts to messageClose/messageOpen actions on exception", () => {
      const { dispatch, getState } = setupStore([
        "ReferenceError: asdf is not defined",
      ]);
      const firstMessageId = getFirstMessage(getState()).id;

      let expanded = getAllMessagesUiById(getState());
      expect(expanded.length).toBe(0);

      dispatch(actions.messageOpen(firstMessageId));

      expanded = getAllMessagesUiById(getState());
      expect(expanded.length).toBe(1);
      expect(expanded[0]).toBe(firstMessageId);

      dispatch(actions.messageClose(firstMessageId));

      expanded = getAllMessagesUiById(getState());
      expect(expanded.length).toBe(0);
    });
  });

  describe("currentGroup", () => {
    it("sets the currentGroup when console.group message is added", () => {
      const { dispatch, getState } = setupStore();

      const packet = stubPackets.get("console.group('bar')");
      dispatch(actions.messagesAdd([packet]));

      const currentGroup = getCurrentGroup(getState());
      expect(currentGroup).toBe(getFirstMessage(getState()).id);
    });

    it("sets currentGroup to expected value when console.groupEnd is added", () => {
      const { dispatch, getState } = setupStore([
        "console.group('bar')",
        "console.groupCollapsed('foo')",
        "console.group('bar')",
        "console.groupEnd('bar')",
      ]);

      let currentGroup = getCurrentGroup(getState());
      expect(currentGroup).toBe(getMessageAt(getState(), 1).id);

      const endFooPacket = stubPackets.get("console.groupEnd('foo')");
      dispatch(actions.messagesAdd([endFooPacket]));
      currentGroup = getCurrentGroup(getState());
      expect(currentGroup).toBe(getFirstMessage(getState()).id);

      const endBarPacket = stubPackets.get("console.groupEnd('bar')");
      dispatch(actions.messagesAdd([endBarPacket]));
      currentGroup = getCurrentGroup(getState());
      expect(currentGroup).toBe(null);
    });

    it("resets the currentGroup to null in response to MESSAGES_CLEAR action", () => {
      const { dispatch, getState } = setupStore(["console.group('bar')"]);

      dispatch(actions.messagesClear());

      const currentGroup = getCurrentGroup(getState());
      expect(currentGroup).toBe(null);
    });
  });

  describe("groupsById", () => {
    it("adds the group with expected array when console.group message is added", () => {
      const { dispatch, getState } = setupStore();

      const barPacket = stubPackets.get("console.group('bar')");
      dispatch(actions.messagesAdd([barPacket]));

      let groupsById = getGroupsById(getState());
      expect(groupsById.size).toBe(1);
      expect(groupsById.has(getFirstMessage(getState()).id)).toBe(true);
      expect(groupsById.get(getFirstMessage(getState()).id)).toEqual([]);

      const fooPacket = stubPackets.get("console.groupCollapsed('foo')");
      dispatch(actions.messagesAdd([fooPacket]));

      groupsById = getGroupsById(getState());
      expect(groupsById.size).toBe(2);
      expect(groupsById.has(getLastMessage(getState()).id)).toBe(true);
      expect(groupsById.get(getLastMessage(getState()).id)).toEqual([
        getFirstMessage(getState()).id,
      ]);
    });

    it("resets groupsById in response to MESSAGES_CLEAR action", () => {
      const { dispatch, getState } = setupStore([
        "console.group('bar')",
        "console.groupCollapsed('foo')",
      ]);

      let groupsById = getGroupsById(getState());
      expect(groupsById.size).toBe(2);

      dispatch(actions.messagesClear());

      groupsById = getGroupsById(getState());
      expect(groupsById.size).toBe(0);
    });

    it("cleans the groupsById property when messages are pruned", () => {
      const { dispatch, getState } = setupStore(
        [
          "console.group('bar')",
          "console.group()",
          "console.groupEnd()",
          "console.groupEnd('bar')",
          "console.group('bar')",
          "console.groupEnd('bar')",
          "console.log('foobar', 'test')",
        ],
        {
          actions,
          storeOptions: {
            logLimit: 3,
          },
        }
      );

      /*
       * Here is the initial state of the console:
       * ▶︎ bar
       *   ▶︎ noLabel
       * ▶︎ bar
       * foobar test
       */

      // Check that we have the expected data.
      let groupsById = getGroupsById(getState());
      expect(groupsById.size).toBe(3);

      // This addition will prune the first group (and its child group) out of the store.
      /*
       * ▶︎ bar
       * foobar test
       * undefined
       */
      let packet = stubPackets.get("console.log(undefined)");
      dispatch(actions.messagesAdd([packet]));

      groupsById = getGroupsById(getState());

      // There should be only the id of the last console.group message.
      expect(groupsById.size).toBe(1);

      // This additions will prune the last group message out of the store.
      /*
       * foobar test
       * undefined
       * foobar test
       */
      packet = stubPackets.get("console.log('foobar', 'test')");
      dispatch(actions.messagesAdd([packet]));

      // groupsById should now be empty.
      expect(getGroupsById(getState()).size).toBe(0);
    });
  });

  describe("networkMessagesUpdateById", () => {
    it("adds the network update message when network update action is called", () => {
      const { dispatch, getState } = setupStore();

      let packet = clonePacket(stubPackets.get("GET request"));
      let updatePacket = clonePacket(stubPackets.get("GET request update"));

      packet.actor = "message1";
      updatePacket.networkInfo.actor = "message1";
      dispatch(actions.messagesAdd([packet]));
      dispatch(
        actions.networkMessageUpdate(
          updatePacket.networkInfo,
          null,
          updatePacket
        )
      );

      let networkUpdates = getAllNetworkMessagesUpdateById(getState());
      expect(Object.keys(networkUpdates)).toEqual(["message1"]);

      packet = clonePacket(stubPackets.get("GET request"));
      updatePacket = stubPackets.get("XHR GET request update");
      packet.actor = "message2";
      updatePacket.networkInfo.actor = "message2";
      dispatch(actions.messagesAdd([packet]));
      dispatch(
        actions.networkMessageUpdate(
          updatePacket.networkInfo,
          null,
          updatePacket
        )
      );

      networkUpdates = getAllNetworkMessagesUpdateById(getState());
      expect(Object.keys(networkUpdates)).toEqual(["message1", "message2"]);
    });

    it("resets networkMessagesUpdateById in response to MESSAGES_CLEAR action", () => {
      const { dispatch, getState } = setupStore(["XHR GET request"]);

      const updatePacket = stubPackets.get("XHR GET request update");
      dispatch(
        actions.networkMessageUpdate(
          updatePacket.networkInfo,
          null,
          updatePacket
        )
      );

      let networkUpdates = getAllNetworkMessagesUpdateById(getState());
      expect(Object.keys(networkUpdates).length > 0).toBe(true);

      dispatch(actions.messagesClear());

      networkUpdates = getAllNetworkMessagesUpdateById(getState());
      expect(Object.keys(networkUpdates).length).toBe(0);
    });

    it("cleans the networkMessagesUpdateById property when messages are pruned", () => {
      const { dispatch, getState } = setupStore([], {
        storeOptions: {
          logLimit: 3,
        },
      });

      // Add 3 network messages and their updates
      let packet = clonePacket(stubPackets.get("XHR GET request"));
      const updatePacket = clonePacket(
        stubPackets.get("XHR GET request update")
      );
      packet.actor = "message1";
      updatePacket.networkInfo.actor = "message1";
      dispatch(actions.messagesAdd([packet]));
      dispatch(
        actions.networkMessageUpdate(
          updatePacket.networkInfo,
          null,
          updatePacket
        )
      );

      packet.actor = "message2";
      updatePacket.networkInfo.actor = "message2";
      dispatch(actions.messagesAdd([packet]));
      dispatch(
        actions.networkMessageUpdate(
          updatePacket.networkInfo,
          null,
          updatePacket
        )
      );

      packet.actor = "message3";
      updatePacket.networkInfo.actor = "message3";
      dispatch(actions.messagesAdd([packet]));
      dispatch(
        actions.networkMessageUpdate(
          updatePacket.networkInfo,
          null,
          updatePacket
        )
      );

      // Check that we have the expected data.
      const messages = getAllMessagesById(getState());
      const [
        firstNetworkMessageId,
        secondNetworkMessageId,
        thirdNetworkMessageId,
      ] = [...messages.keys()];

      let networkUpdates = getAllNetworkMessagesUpdateById(getState());
      expect(Object.keys(networkUpdates)).toEqual([
        firstNetworkMessageId,
        secondNetworkMessageId,
        thirdNetworkMessageId,
      ]);

      // This addition will remove the first network message.
      packet = stubPackets.get("console.log(undefined)");
      dispatch(actions.messagesAdd([packet]));

      networkUpdates = getAllNetworkMessagesUpdateById(getState());
      expect(Object.keys(networkUpdates)).toEqual([
        secondNetworkMessageId,
        thirdNetworkMessageId,
      ]);

      // This addition will remove the second network message.
      packet = stubPackets.get("console.log('foobar', 'test')");
      dispatch(actions.messagesAdd([packet]));

      networkUpdates = getAllNetworkMessagesUpdateById(getState());
      expect(Object.keys(networkUpdates)).toEqual([thirdNetworkMessageId]);

      // This addition will remove the last network message.
      packet = stubPackets.get("console.log(undefined)");
      dispatch(actions.messagesAdd([packet]));

      // networkMessageUpdateById should now be empty.
      networkUpdates = getAllNetworkMessagesUpdateById(getState());
      expect(Object.keys(networkUpdates)).toEqual([]);
    });
  });

  describe("messagesPayloadById", () => {
    it("resets messagesPayloadById in response to MESSAGES_CLEAR action", () => {
      const { dispatch, getState } = setupStore([
        "console.table(['a', 'b', 'c'])",
      ]);

      const data = Symbol("tableData");
      dispatch(
        actions.messageUpdatePayload(getFirstMessage(getState()).id, data)
      );
      const table = getAllMessagesPayloadById(getState());
      expect(table.size).toBe(1);
      expect(table.get(getFirstMessage(getState()).id)).toBe(data);

      dispatch(actions.messagesClear());

      expect(getAllMessagesPayloadById(getState()).size).toBe(0);
    });

    it("cleans the messagesPayloadById property when messages are pruned", () => {
      const { dispatch, getState } = setupStore([], {
        storeOptions: {
          logLimit: 2,
        },
      });

      // Add 2 table message and their data.
      dispatch(
        actions.messagesAdd([stubPackets.get("console.table(['a', 'b', 'c'])")])
      );
      dispatch(
        actions.messagesAdd([
          stubPackets.get("console.table(['red', 'green', 'blue']);"),
        ])
      );

      const messages = getAllMessagesById(getState());

      const tableData1 = Symbol();
      const tableData2 = Symbol();
      const [id1, id2] = [...messages.keys()];
      dispatch(actions.messageUpdatePayload(id1, tableData1));
      dispatch(actions.messageUpdatePayload(id2, tableData2));

      let table = getAllMessagesPayloadById(getState());
      expect(table.size).toBe(2);

      // This addition will remove the first table message.
      dispatch(
        actions.messagesAdd([stubPackets.get("console.log(undefined)")])
      );

      table = getAllMessagesPayloadById(getState());
      expect(table.size).toBe(1);
      expect(table.get(id2)).toBe(tableData2);

      // This addition will remove the second table message.
      dispatch(
        actions.messagesAdd([stubPackets.get("console.log('foobar', 'test')")])
      );

      expect(getAllMessagesPayloadById(getState()).size).toBe(0);
    });
  });

  describe("messagesAdd", () => {
    it("still log repeated message over logLimit, but only repeated ones", () => {
      // Log two distinct messages
      const key1 = "console.log('foobar', 'test')";
      const key2 = "console.log(undefined)";
      const { dispatch, getState } = setupStore([key1, key2], {
        storeOptions: {
          logLimit: 2,
        },
      });

      // Then repeat the last one two times and log the first one again
      const packet1 = clonePacket(stubPackets.get(key2));
      const packet2 = clonePacket(stubPackets.get(key2));
      const packet3 = clonePacket(stubPackets.get(key1));

      // Repeat ID must be the same even if the timestamp is different.
      packet1.message.timeStamp = 1;
      packet2.message.timeStamp = 2;
      packet3.message.timeStamp = 3;
      dispatch(actions.messagesAdd([packet1, packet2, packet3]));

      // There is still only two messages being logged,
      const messages = getAllMessagesById(getState());
      expect(messages.size).toBe(2);

      // the second one being repeated 3 times
      const repeat = getAllRepeatById(getState());
      expect(repeat[getFirstMessage(getState()).id]).toBe(3);
      expect(repeat[getLastMessage(getState()).id]).toBe(undefined);
    });
  });
});
