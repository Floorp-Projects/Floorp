/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  getFirstMessage,
  getLastMessage,
  getPrivatePacket,
  getWebConsoleUiMock,
  setupActions,
  setupStore,
} = require("devtools/client/webconsole/test/node/helpers");
const {
  stubPackets,
} = require("devtools/client/webconsole/test/node/fixtures/stubs/index");

const expect = require("expect");

describe("private messages", () => {
  let actions;
  before(() => {
    actions = setupActions();
  });

  it("removes private messages on PRIVATE_MESSAGES_CLEAR action", () => {
    const { dispatch, getState } = setupStore();

    dispatch(
      actions.messagesAdd([
        getPrivatePacket("console.trace()"),
        stubPackets.get("console.log('mymap')"),
        getPrivatePacket("console.log(undefined)"),
        getPrivatePacket("GET request"),
      ])
    );

    let state = getState();
    const messages = getAllMessagesById(state);
    expect(messages.size).toBe(4);

    dispatch(actions.privateMessagesClear());

    state = getState();
    expect(getAllMessagesById(state).size).toBe(1);
    expect(getVisibleMessages(state).length).toBe(1);
  });

  it("cleans messagesUiById on PRIVATE_MESSAGES_CLEAR action", () => {
    const { dispatch, getState } = setupStore();

    dispatch(
      actions.messagesAdd([
        getPrivatePacket("console.trace()"),
        stubPackets.get("console.trace()"),
      ])
    );

    let state = getState();
    expect(getAllMessagesUiById(state).length).toBe(2);

    dispatch(actions.privateMessagesClear());

    state = getState();
    expect(getAllMessagesUiById(state).length).toBe(1);
  });

  it("cleans repeatsById on PRIVATE_MESSAGES_CLEAR action", () => {
    const { dispatch, getState } = setupStore();

    dispatch(
      actions.messagesAdd([
        getPrivatePacket("console.log(undefined)"),
        getPrivatePacket("console.log(undefined)"),
        stubPackets.get("console.log(undefined)"),
        stubPackets.get("console.log(undefined)"),
      ])
    );

    let state = getState();
    expect(getAllRepeatById(state)).toEqual({
      [getFirstMessage(state).id]: 2,
      [getLastMessage(state).id]: 2,
    });

    dispatch(actions.privateMessagesClear());

    state = getState();
    expect(Object.keys(getAllRepeatById(state)).length).toBe(1);
    expect(getAllRepeatById(state)).toEqual({
      [getFirstMessage(state).id]: 2,
    });
  });

  it("cleans messagesPayloadById on PRIVATE_MESSAGES_CLEAR action", () => {
    const { dispatch, getState } = setupStore();

    dispatch(
      actions.messagesAdd([
        getPrivatePacket("console.table(['a', 'b', 'c'])"),
        stubPackets.get("console.table(['a', 'b', 'c'])"),
      ])
    );

    const privateTableData = Symbol("privateTableData");
    const publicTableData = Symbol("publicTableData");
    dispatch(
      actions.messageUpdatePayload(
        getFirstMessage(getState()).id,
        privateTableData
      )
    );
    dispatch(
      actions.messageUpdatePayload(
        getLastMessage(getState()).id,
        publicTableData
      )
    );

    let state = getState();
    expect(getAllMessagesPayloadById(state).size).toBe(2);

    dispatch(actions.privateMessagesClear());

    state = getState();
    expect(getAllMessagesPayloadById(state).size).toBe(1);
    expect(
      getAllMessagesPayloadById(state).get(getFirstMessage(getState()).id)
    ).toBe(publicTableData);
  });

  it("cleans group properties on PRIVATE_MESSAGES_CLEAR action", () => {
    const { dispatch, getState } = setupStore();
    dispatch(
      actions.messagesAdd([
        stubPackets.get("console.group()"),
        getPrivatePacket("console.group()"),
      ])
    );

    let state = getState();
    const publicMessageId = getFirstMessage(state).id;
    const privateMessageId = getLastMessage(state).id;
    expect(getCurrentGroup(state)).toBe(privateMessageId);
    expect(getGroupsById(state).size).toBe(2);

    dispatch(actions.privateMessagesClear());

    state = getState();
    expect(getGroupsById(state).size).toBe(1);
    expect(getGroupsById(state).has(publicMessageId)).toBe(true);
    expect(getCurrentGroup(state)).toBe(publicMessageId);
  });

  it("cleans networkMessagesUpdateById on PRIVATE_MESSAGES_CLEAR action", () => {
    const { dispatch, getState } = setupStore();

    const publicActor = "network/public";
    const privateActor = "network/private";
    const publicPacket = {
      ...stubPackets.get("GET request"),
      actor: publicActor,
    };
    const privatePacket = {
      ...getPrivatePacket("XHR GET request"),
      actor: privateActor,
    };

    dispatch(actions.messagesAdd([publicPacket, privatePacket]));

    let networkUpdates = getAllNetworkMessagesUpdateById(getState());
    expect(Object.keys(networkUpdates)).toEqual([publicActor, privateActor]);

    dispatch(actions.privateMessagesClear());

    networkUpdates = getAllNetworkMessagesUpdateById(getState());
    expect(Object.keys(networkUpdates)).toEqual([publicActor]);
  });

  it("releases private backend actors on PRIVATE_MESSAGES_CLEAR action", () => {
    const releasedActors = [];
    const { dispatch, getState } = setupStore([], {
      webConsoleUI: getWebConsoleUiMock(null, {
        releaseActor: actor => {
          releasedActors.push(actor);
        },
      }),
    });

    // Add a log message.
    dispatch(
      actions.messagesAdd([
        stubPackets.get("console.log('myarray', ['red', 'green', 'blue'])"),
        getPrivatePacket("console.log('mymap')"),
      ])
    );

    const firstMessage = getFirstMessage(getState());
    const firstMessageActor = firstMessage.parameters[1].actor;

    const lastMessage = getLastMessage(getState());
    const lastMessageActor = lastMessage.parameters[1].actor;

    // Kick-off the actor release.
    dispatch(actions.privateMessagesClear());

    expect(releasedActors.length).toBe(1);
    expect(releasedActors).toInclude(lastMessageActor);
    expect(releasedActors).toNotInclude(firstMessageActor);
  });
});
