"use strict"; /* This Source Code Form is subject to the terms of the Mozilla Public
               * License, v. 2.0. If a copy of the MPL was not distributed with this file,
               * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.store = loop.store || {};

loop.store.TextChatStore = function () {
  "use strict";

  var CHAT_MESSAGE_TYPES = loop.store.CHAT_MESSAGE_TYPES = { 
    RECEIVED: "recv", 
    SENT: "sent", 
    SPECIAL: "special" };


  var CHAT_CONTENT_TYPES = loop.shared.utils.CHAT_CONTENT_TYPES;

  /**
   * A store to handle text chats. The store has a message list that may
   * contain different types of messages and data.
   */
  var TextChatStore = loop.store.createStore({ 
    actions: [
    "dataChannelsAvailable", 
    "receivedTextChatMessage", 
    "sendTextChatMessage", 
    "updateRoomInfo", 
    "updateRoomContext", 
    "remotePeerDisconnected", 
    "remotePeerConnected"], 


    /**
     * Initializes the store.
     *
     * @param  {Object} options An object containing options for this store.
     *                          It should consist of:
     *                          - sdkDriver: The sdkDriver to use for sending
     *                                       messages.
     */
    initialize: function initialize(options) {
      options = options || {};

      if (!options.sdkDriver) {
        throw new Error("Missing option sdkDriver");}


      this._sdkDriver = options.sdkDriver;}, 


    /**
     * Returns initial state data for this active room.
     */
    getInitialStoreState: function getInitialStoreState() {
      return { 
        textChatEnabled: false, 
        // The messages currently received. Care should be taken when updating
        // this - do not update the in-store array directly, but use a clone or
        // separate array and then use setStoreState().
        messageList: [], 
        roomName: null, 
        length: 0 };}, 



    /**
     * Handles information for when data channels are available - enables
     * text chat.
     *
     * @param {sharedActions.DataChannelsAvailable} actionData
     */
    dataChannelsAvailable: function dataChannelsAvailable(actionData) {
      this.setStoreState({ textChatEnabled: actionData.available });

      if (actionData.available) {
        window.dispatchEvent(new CustomEvent("LoopChatEnabled"));}}, 



    /**
     * Appends a message to the store, which may be of type 'sent' or 'received'.
     *
     * @param {CHAT_MESSAGE_TYPES} type
     * @param {Object} messageData Data for this message. Options are:
     * - {CHAT_CONTENT_TYPES} contentType
     * - {String}             message     The message detail.
     * - {Object}             extraData   Extra data associated with the message.
     */
    _appendTextChatMessage: function _appendTextChatMessage(type, messageData) {
      // We create a new list to avoid updating the store's state directly,
      // which confuses the views.
      var message = { 
        type: type, 
        contentType: messageData.contentType, 
        message: messageData.message, 
        extraData: messageData.extraData, 
        sentTimestamp: messageData.sentTimestamp, 
        receivedTimestamp: messageData.receivedTimestamp };

      var newList = [].concat(this._storeState.messageList);
      var isContext = message.contentType === CHAT_CONTENT_TYPES.CONTEXT;
      if (isContext) {
        var contextUpdated = false;
        for (var i = 0, l = newList.length; i < l; ++i) {
          // Replace the current context message with the provided update.
          if (newList[i].contentType === CHAT_CONTENT_TYPES.CONTEXT) {
            newList[i] = message;
            contextUpdated = true;
            break;}}


        if (!contextUpdated) {
          newList.push(message);}} else 

      {
        newList.push(message);}

      this.setStoreState({ messageList: newList });

      // Notify MozLoopService if appropriate that a message has been appended
      // and it should therefore check if we need a different sized window or not.
      if (message.contentType !== CHAT_CONTENT_TYPES.CONTEXT && 
      message.contentType !== CHAT_CONTENT_TYPES.NOTIFICATION) {
        if (this._storeState.textChatEnabled) {
          window.dispatchEvent(new CustomEvent("LoopChatMessageAppended"));} else 
        {
          window.dispatchEvent(new CustomEvent("LoopChatDisabledMessageAppended"));}}}, 




    /**
     * Handles received text chat messages.
     *
     * @param {sharedActions.ReceivedTextChatMessage} actionData
     */
    receivedTextChatMessage: function receivedTextChatMessage(actionData) {
      // If we don't know how to deal with this content, then skip it
      // as this version doesn't support it.
      if (actionData.contentType !== CHAT_CONTENT_TYPES.TEXT && 
      actionData.contentType !== CHAT_CONTENT_TYPES.CONTEXT_TILE && 
      actionData.contentType !== CHAT_CONTENT_TYPES.NOTIFICATION) {
        return;}


      this._appendTextChatMessage(CHAT_MESSAGE_TYPES.RECEIVED, actionData);}, 


    /**
     * Handles sending of a chat message.
     *
     * @param {sharedActions.SendTextChatMessage} actionData
     */
    sendTextChatMessage: function sendTextChatMessage(actionData) {
      this._appendTextChatMessage(CHAT_MESSAGE_TYPES.SENT, actionData);
      this._sdkDriver.sendTextChatMessage(actionData);}, 


    /**
     * Handles receiving information about the room - specifically the room name
     * so it can be updated.
     *
     * @param  {sharedActions.UpdateRoomInfo} actionData
     */
    updateRoomInfo: function updateRoomInfo(actionData) {
      // XXX When we add special messages to desktop, we'll need to not post
      // multiple changes of room name, only the first. Bug 1171940 should fix this.
      if (actionData.roomName) {
        var roomName = actionData.roomName;
        if (!roomName && actionData.roomContextUrls && actionData.roomContextUrls.length) {
          roomName = actionData.roomContextUrls[0].description || 
          actionData.roomContextUrls[0].url;}

        this.setStoreState({ roomName: roomName });}


      // Append the context if we have any.
      if ("roomContextUrls" in actionData && actionData.roomContextUrls && 
      actionData.roomContextUrls.length) {
        // We only support the first url at the moment.
        var urlData = actionData.roomContextUrls[0];

        this._appendTextChatMessage(CHAT_MESSAGE_TYPES.SPECIAL, { 
          contentType: CHAT_CONTENT_TYPES.CONTEXT, 
          message: urlData.description, 
          extraData: { 
            location: urlData.location, 
            thumbnail: urlData.thumbnail } });}}, 





    /**
     * Handles receiving information about the room context due to a change of the tabs
     *
     * @param  {sharedActions.updateRoomContext} actionData
     */
    updateRoomContext: function updateRoomContext(actionData) {
      // Firstly, check if there is a previous context tile, if not, create it
      var contextTile = null;

      for (var i = this._storeState.messageList.length - 1; i >= 0; i--) {
        if (this._storeState.messageList[i].contentType === CHAT_CONTENT_TYPES.CONTEXT_TILE) {
          contextTile = this._storeState.messageList[i];
          break;}}



      if (!contextTile) {
        this._appendContextTileMessage(actionData);
        return;}


      var oldDomain = new URL(contextTile.extraData.newRoomURL).hostname;
      var currentDomain = new URL(actionData.newRoomURL).hostname;

      if (oldDomain === currentDomain) {
        return;}


      this._appendContextTileMessage(actionData);}, 



    /**
     * Handles a remote peer disconnecting from the session.
     * With specific to text chat area, we will put a notification
     * when the peer has left the room or unexpectedly quit.
     *
     * @param  {sharedActions.remotePeerDisconnected} actionData
     */
    remotePeerDisconnected: function remotePeerDisconnected(actionData) {
      var notificationTextKey;

      if (actionData.peerHungup) {
        notificationTextKey = "peer_left_session";} else 
      {
        notificationTextKey = "peer_unexpected_quit";}


      var message = { 
        contentType: CHAT_CONTENT_TYPES.NOTIFICATION, 
        message: notificationTextKey, 
        receivedTimestamp: new Date().toISOString(), 
        extraData: { 
          peerStatus: "disconnected" } };



      this._appendTextChatMessage(CHAT_MESSAGE_TYPES.RECEIVED, message);}, 


    remotePeerConnected: function remotePeerConnected() {
      var notificationTextKey = "peer_join_session";

      var message = { 
        contentType: CHAT_CONTENT_TYPES.NOTIFICATION, 
        message: notificationTextKey, 
        receivedTimestamp: new Date().toISOString(), 
        extraData: { 
          peerStatus: "connected" } };



      this._appendTextChatMessage(CHAT_MESSAGE_TYPES.RECEIVED, message);}, 


    /**
     * Appends a context tile message to the UI and sends it.
     *
     * @param  {sharedActions.updateRoomContext} data
     */
    _appendContextTileMessage: function _appendContextTileMessage(data) {
      var msgData = { 
        contentType: CHAT_CONTENT_TYPES.CONTEXT_TILE, 
        message: data.newRoomDescription, 
        extraData: { 
          roomToken: data.roomToken, 
          newRoomThumbnail: data.newRoomThumbnail, 
          newRoomURL: data.newRoomURL }, 

        sentTimestamp: new Date().toISOString() };


      this.sendTextChatMessage(msgData);} });



  return TextChatStore;}();
