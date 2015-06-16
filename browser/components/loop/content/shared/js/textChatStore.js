/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.store = loop.store || {};

loop.store.TextChatStore = (function(mozL10n) {
  "use strict";

  var sharedActions = loop.shared.actions;

  var CHAT_MESSAGE_TYPES = loop.store.CHAT_MESSAGE_TYPES = {
    RECEIVED: "recv",
    SENT: "sent",
    SPECIAL: "special"
  };

  var CHAT_CONTENT_TYPES = loop.store.CHAT_CONTENT_TYPES = {
    TEXT: "chat-text",
    ROOM_NAME: "room-name"
  };

  /**
   * A store to handle text chats. The store has a message list that may
   * contain different types of messages and data.
   */
  var TextChatStore = loop.store.createStore({
    actions: [
      "dataChannelsAvailable",
      "receivedTextChatMessage",
      "sendTextChatMessage",
      "updateRoomInfo"
    ],

    /**
     * Initializes the store.
     *
     * @param  {Object} options An object containing options for this store.
     *                          It should consist of:
     *                          - sdkDriver: The sdkDriver to use for sending
     *                                       messages.
     */
    initialize: function(options) {
      options = options || {};

      if (!options.sdkDriver) {
        throw new Error("Missing option sdkDriver");
      }

      this._sdkDriver = options.sdkDriver;
    },

    /**
     * Returns initial state data for this active room.
     */
    getInitialStoreState: function() {
      return {
        textChatEnabled: false,
        // The messages currently received. Care should be taken when updating
        // this - do not update the in-store array directly, but use a clone or
        // separate array and then use setStoreState().
        messageList: [],
        length: 0
      };
    },

    /**
     * Handles information for when data channels are available - enables
     * text chat.
     */
    dataChannelsAvailable: function() {
      this.setStoreState({ textChatEnabled: true });
      window.dispatchEvent(new CustomEvent("LoopChatEnabled"));
    },

    /**
     * Appends a message to the store, which may be of type 'sent' or 'received'.
     *
     * @param {String} type
     * @param {sharedActions.ReceivedTextChatMessage|sharedActions.SendTextChatMessage} actionData
     */
    _appendTextChatMessage: function(type, actionData) {
      // We create a new list to avoid updating the store's state directly,
      // which confuses the views.
      var message = {
        type: type,
        contentType: actionData.contentType,
        message: actionData.message
      };
      var newList = this._storeState.messageList.concat(message);
      this.setStoreState({ messageList: newList });

      // Notify MozLoopService if appropriate that a message has been appended
      // and it should therefore check if we need a different sized window or not.
      if (type != CHAT_MESSAGE_TYPES.SPECIAL) {
        window.dispatchEvent(new CustomEvent("LoopChatMessageAppended"));
      }
    },

    /**
     * Handles received text chat messages.
     *
     * @param {sharedActions.ReceivedTextChatMessage} actionData
     */
    receivedTextChatMessage: function(actionData) {
      // If we don't know how to deal with this content, then skip it
      // as this version doesn't support it.
      if (actionData.contentType != CHAT_CONTENT_TYPES.TEXT) {
        return;
      }

      this._appendTextChatMessage(CHAT_MESSAGE_TYPES.RECEIVED, actionData);
    },

    /**
     * Handles sending of a chat message.
     *
     * @param {sharedActions.SendTextChatMessage} actionData
     */
    sendTextChatMessage: function(actionData) {
      this._appendTextChatMessage(CHAT_MESSAGE_TYPES.SENT, actionData);
      this._sdkDriver.sendTextChatMessage(actionData);
    },

    /**
     * Handles receiving information about the room - specifically the room name
     * so it can be added to the list.
     *
     * @param  {sharedActions.UpdateRoomInfo} actionData
     */
    updateRoomInfo: function(actionData) {
      // XXX When we add special messages to desktop, we'll need to not post
      // multiple changes of room name, only the first. Bug 1171940 should fix this.
      this._appendTextChatMessage(CHAT_MESSAGE_TYPES.SPECIAL, {
        contentType: CHAT_CONTENT_TYPES.ROOM_NAME,
        message: mozL10n.get("rooms_welcome_title", {conversationName: actionData.roomName})
      });
    }
  });

  return TextChatStore;
})(navigator.mozL10n || window.mozL10n);
