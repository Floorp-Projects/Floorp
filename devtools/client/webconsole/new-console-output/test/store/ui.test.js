/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const expect = require("expect");

const actions = require("devtools/client/webconsole/new-console-output/actions/index");
const { setupStore, getFirstMessage, getLastMessage } = require("devtools/client/webconsole/new-console-output/test/helpers");
const { stubPackets, stubPreparedMessages } = require("devtools/client/webconsole/new-console-output/test/fixtures/stubs/index");

describe("Testing UI", () => {
  let store;

  beforeEach(() => {
    store = setupStore();
  });

  describe("Toggle sidebar", () => {
    it("sidebar is toggled on and off", () => {
      const packet = stubPackets.get("inspect({a: 1})");
      const message = stubPreparedMessages.get("inspect({a: 1})");
      store.dispatch(actions.messagesAdd([packet]));

      const actorId = message.parameters[0].actor;
      const messageId = getFirstMessage(store.getState()).id;
      store.dispatch(actions.showObjectInSidebar(actorId, messageId));

      expect(store.getState().ui.sidebarVisible).toEqual(true);
      store.dispatch(actions.sidebarClose());
      expect(store.getState().ui.sidebarVisible).toEqual(false);
    });
  });

  describe("Hide sidebar on clear", () => {
    it("sidebar is hidden on clear", () => {
      const packet = stubPackets.get("inspect({a: 1})");
      const message = stubPreparedMessages.get("inspect({a: 1})");
      store.dispatch(actions.messagesAdd([packet]));

      const actorId = message.parameters[0].actor;
      const messageId = getFirstMessage(store.getState()).id;
      store.dispatch(actions.showObjectInSidebar(actorId, messageId));

      expect(store.getState().ui.sidebarVisible).toEqual(true);
      store.dispatch(actions.messagesClear());
      expect(store.getState().ui.sidebarVisible).toEqual(false);
      store.dispatch(actions.messagesClear());
      expect(store.getState().ui.sidebarVisible).toEqual(false);
    });
  });

  describe("Show object in sidebar", () => {
    it("sidebar is shown with correct object", () => {
      const packet = stubPackets.get("inspect({a: 1})");
      const message = stubPreparedMessages.get("inspect({a: 1})");
      store.dispatch(actions.messagesAdd([packet]));

      const actorId = message.parameters[0].actor;
      const messageId = getFirstMessage(store.getState()).id;
      store.dispatch(actions.showObjectInSidebar(actorId, messageId));

      expect(store.getState().ui.sidebarVisible).toEqual(true);
      expect(store.getState().ui.gripInSidebar).toEqual(message.parameters[0]);
    });

    it("sidebar is not updated for the same object", () => {
      const packet = stubPackets.get("inspect({a: 1})");
      const message = stubPreparedMessages.get("inspect({a: 1})");
      store.dispatch(actions.messagesAdd([packet]));

      const actorId = message.parameters[0].actor;
      const messageId = getFirstMessage(store.getState()).id;
      store.dispatch(actions.showObjectInSidebar(actorId, messageId));

      expect(store.getState().ui.sidebarVisible).toEqual(true);
      expect(store.getState().ui.gripInSidebar).toEqual(message.parameters[0]);
      let state = store.getState().ui;

      store.dispatch(actions.showObjectInSidebar(actorId, messageId));
      expect(store.getState().ui).toEqual(state);
    });

    it("sidebar shown and updated for new object", () => {
      const packet = stubPackets.get("inspect({a: 1})");
      const message = stubPreparedMessages.get("inspect({a: 1})");
      store.dispatch(actions.messagesAdd([packet]));

      const actorId = message.parameters[0].actor;
      const messageId = getFirstMessage(store.getState()).id;
      store.dispatch(actions.showObjectInSidebar(actorId, messageId));

      expect(store.getState().ui.sidebarVisible).toEqual(true);
      expect(store.getState().ui.gripInSidebar).toEqual(message.parameters[0]);

      const newPacket = stubPackets.get("new Date(0)");
      const newMessage = stubPreparedMessages.get("new Date(0)");
      store.dispatch(actions.messagesAdd([newPacket]));

      const newActorId = newMessage.parameters[0].actor;
      const newMessageId = getLastMessage(store.getState()).id;
      store.dispatch(actions.showObjectInSidebar(newActorId, newMessageId));

      expect(store.getState().ui.sidebarVisible).toEqual(true);
      expect(store.getState().ui.gripInSidebar).toEqual(newMessage.parameters[0]);
    });
  });
});
