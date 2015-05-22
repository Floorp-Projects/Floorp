/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.store = loop.store || {};

loop.store.TextChatStore = (function() {
  "use strict";

  var sharedActions = loop.shared.actions;

  var CHAT_MESSAGE_TYPES = loop.store.CHAT_MESSAGE_TYPES = {
    RECEIVED: "recv",
    SENT: "sent"
  };

  var CHAT_CONTENT_TYPES = loop.store.CHAT_CONTENT_TYPES = {
    TEXT: "chat-text"
  };

  /**
   * A store to handle text chats. The store has a message list that may
   * contain different types of messages and data.
   */
  var TextChatStore = loop.store.createStore({
    actions: [
      "dataChannelsAvailable",
      "receivedTextChatMessage",
      "sendTextChatMessage"
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
      // We create a new list to avoid updating the store's state directly,
      // which confuses the views.
      var newList = this._storeState.messageList.concat({
        type: CHAT_MESSAGE_TYPES.RECEIVED,
        contentType: actionData.contentType,
        message: actionData.message
      });
      this.setStoreState({ messageList: newList });
    },

    /**
     * Handles sending of a chat message.
     *
     * @param {sharedActions.SendTextChatMessage} actionData
     */
    sendTextChatMessage: function(actionData) {
      // We create a new list to avoid updating the store's state directly,
      // which confuses the views.
      var newList = this._storeState.messageList.concat({
        type: CHAT_MESSAGE_TYPES.SENT,
        contentType: actionData.contentType,
        message: actionData.message
      });
      this._sdkDriver.sendTextChatMessage(actionData);
      this.setStoreState({ messageList: newList });
    }
  });

  return TextChatStore;
})();
