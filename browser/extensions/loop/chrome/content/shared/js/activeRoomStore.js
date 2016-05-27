"use strict"; /* This Source Code Form is subject to the terms of the Mozilla Public
               * License, v. 2.0. If a copy of the MPL was not distributed with this file,
               * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.store = loop.store || {};

loop.store.ROOM_STATES = { 
  // The initial state of the room
  INIT: "room-init", 
  // The store is gathering the room data
  GATHER: "room-gather", 
  // The store has got the room data
  READY: "room-ready", 
  // Obtaining media from the user
  MEDIA_WAIT: "room-media-wait", 
  // Joining the room is taking place
  JOINING: "room-joining", 
  // The room is known to be joined on the loop-server
  JOINED: "room-joined", 
  // The room is connected to the sdk server.
  SESSION_CONNECTED: "room-session-connected", 
  // There are participants in the room.
  HAS_PARTICIPANTS: "room-has-participants", 
  // There was an issue with the room
  FAILED: "room-failed", 
  // The room is full
  FULL: "room-full", 
  // The room conversation has ended, displays the feedback view.
  ENDED: "room-ended", 
  // The window is closing
  CLOSING: "room-closing" };


loop.store.ActiveRoomStore = function (mozL10n) {
  "use strict";

  var sharedActions = loop.shared.actions;
  var crypto = loop.crypto;
  var CHAT_CONTENT_TYPES = loop.shared.utils.CHAT_CONTENT_TYPES;
  var FAILURE_DETAILS = loop.shared.utils.FAILURE_DETAILS;
  var SCREEN_SHARE_STATES = loop.shared.utils.SCREEN_SHARE_STATES;

  // Error numbers taken from
  // https://github.com/mozilla-services/loop-server/blob/master/loop/errno.json
  var REST_ERRNOS = loop.shared.utils.REST_ERRNOS;

  var ROOM_STATES = loop.store.ROOM_STATES;

  var ROOM_INFO_FAILURES = loop.shared.utils.ROOM_INFO_FAILURES;

  var OPTIONAL_ROOMINFO_FIELDS = { 
    participants: "participants", 
    roomContextUrls: "roomContextUrls", 
    roomDescription: "roomDescription", 
    roomInfoFailure: "roomInfoFailure", 
    roomName: "roomName", 
    roomState: "roomState", 
    socialShareProviders: "socialShareProviders" };


  var updateContextTimer = null;

  /**
   * Active room store.
   *
   * @param {loop.Dispatcher} dispatcher  The dispatcher for dispatching actions
   *                                      and registering to consume actions.
   * @param {Object} options Options object:
   * - {OTSdkDriver} sdkDriver  The SDK driver instance.
   */
  var ActiveRoomStore = loop.store.createStore({ 
    /**
     * The time factor to adjust the expires time to ensure that we send a refresh
     * before the expiry. Currently set as 90%.
     */
    expiresTimeFactor: 0.9, 

    // XXX Further actions are registered in setupWindowData and
    // fetchServerData when we know what window type this is. At some stage,
    // we might want to consider store mixins or some alternative which
    // means the stores would only be created when we want them.
    actions: [
    "setupWindowData", 
    "fetchServerData"], 


    initialize: function initialize(options) {
      if (!options.sdkDriver) {
        throw new Error("Missing option sdkDriver");}

      this._sdkDriver = options.sdkDriver;

      this._isDesktop = options.isDesktop || false;}, 


    /**
     * This is a list of states that need resetting when the room is left,
     * due to user choice, failure or other reason. It is a subset of
     * getInitialStoreState as some items (e.g. roomState, failureReason,
     * context information) can persist across room exit & re-entry.
     *
     * @type {Array}
     */
    _statesToResetOnLeave: [
    "audioMuted", 
    "chatMessageExchanged", 
    "localSrcMediaElement", 
    "localVideoDimensions", 
    "mediaConnected", 
    "receivingScreenShare", 
    "remoteAudioEnabled", 
    "remotePeerDisconnected", 
    "remoteSrcMediaElement", 
    "remoteVideoDimensions", 
    "remoteVideoEnabled", 
    "streamPaused", 
    "screenSharingState", 
    "screenShareMediaElement", 
    "videoMuted"], 


    /**
     * Returns initial state data for this active room.
     *
     * When adding states, consider if _statesToResetOnLeave needs updating
     * as well.
     */
    getInitialStoreState: function getInitialStoreState() {
      return { 
        roomState: ROOM_STATES.INIT, 
        audioMuted: false, 
        videoMuted: false, 
        remoteAudioEnabled: false, 
        remoteVideoEnabled: false, 
        failureReason: undefined, 
        // Whether or not Firefox can handle this room in the conversation
        // window, rather than us handling it in the standalone.
        userAgentHandlesRoom: undefined, 
        // Tracks if the room has been used during this
        // session. 'Used' means at least one call has been placed
        // with it. Entering and leaving the room without seeing
        // anyone is not considered as 'used'
        used: false, 
        localVideoDimensions: {}, 
        remoteVideoDimensions: {}, 
        screenSharingState: SCREEN_SHARE_STATES.INACTIVE, 
        sharingPaused: false, 
        receivingScreenShare: false, 
        remotePeerDisconnected: false, 
        // Any urls (aka context) associated with the room. null if no context.
        roomContextUrls: null, 
        // The description for a room as stored in the context data.
        roomDescription: null, 
        // Room information failed to be obtained for a reason. See ROOM_INFO_FAILURES.
        roomInfoFailure: null, 
        // The name of the room.
        roomName: null, 
        // True when sharing screen has been paused.
        streamPaused: false, 
        // Social API state.
        socialShareProviders: null, 
        // True if media has been connected both-ways.
        mediaConnected: false, 
        // True if a chat message was sent or received during a session.
        // Read more at https://wiki.mozilla.org/Loop/Session.
        chatMessageExchanged: false };}, 



    /**
     * Handles a room failure.
     *
     * @param {sharedActions.RoomFailure} actionData
     */
    roomFailure: function roomFailure(actionData) {
      function getReason(serverCode) {
        switch (serverCode) {
          case REST_ERRNOS.INVALID_TOKEN:
          case REST_ERRNOS.EXPIRED:
            return FAILURE_DETAILS.EXPIRED_OR_INVALID;
          case undefined:
            // XHR errors reach here with errno as undefined
            return FAILURE_DETAILS.COULD_NOT_CONNECT;
          default:
            return FAILURE_DETAILS.UNKNOWN;}}



      console.error("Error in state `" + this._storeState.roomState + "`:", 
      actionData.error);

      var exitState = this._storeState.roomState !== ROOM_STATES.FAILED ? 
      this._storeState.roomState : this._storeState.failureExitState;

      this.setStoreState({ 
        error: actionData.error, 
        failureReason: getReason(actionData.error.errno), 
        failureExitState: exitState });


      this._leaveRoom(actionData.error.errno === REST_ERRNOS.ROOM_FULL ? 
      ROOM_STATES.FULL : ROOM_STATES.FAILED, actionData.failedJoinRequest);}, 


    /**
     * Attempts to retry getting the room data if there has been a room failure.
     */
    retryAfterRoomFailure: function retryAfterRoomFailure() {
      if (this._storeState.failureReason === FAILURE_DETAILS.EXPIRED_OR_INVALID) {
        console.error("Invalid retry attempt for expired or invalid url");
        return;}


      switch (this._storeState.failureExitState) {
        case ROOM_STATES.GATHER:
          this.dispatchAction(new sharedActions.FetchServerData({ 
            cryptoKey: this._storeState.roomCryptoKey, 
            token: this._storeState.roomToken, 
            windowType: "room" }));

          return;
        case ROOM_STATES.INIT:
        case ROOM_STATES.ENDED:
        case ROOM_STATES.CLOSING:
          console.error("Unexpected retry for exit state", this._storeState.failureExitState);
          return;
        default:
          // For all other states, we simply join the room. We avoid dispatching
          // another action here so that metrics doesn't get two notifications
          // in a row (one for retry, one for the join).
          this.joinRoom();
          return;}}, 



    /**
     * Registers the actions with the dispatcher that this store is interested
     * in after the initial setup has been performed.
     */
    _registerPostSetupActions: function _registerPostSetupActions() {
      // Protect against calling this twice, as we don't want to register
      // before we know what type we are, but in some cases we need to re-do
      // an action (e.g. FetchServerData).
      if (this._registeredActions) {
        return;}


      this._registeredActions = true;

      var actions = [
      "roomFailure", 
      "retryAfterRoomFailure", 
      "updateRoomInfo", 
      "userAgentHandlesRoom", 
      "gotMediaPermission", 
      "joinRoom", 
      "joinedRoom", 
      "connectedToSdkServers", 
      "connectionFailure", 
      "setMute", 
      "screenSharingState", 
      "receivingScreenShare", 
      "remotePeerDisconnected", 
      "remotePeerConnected", 
      "windowUnload", 
      "leaveRoom", 
      "feedbackComplete", 
      "mediaStreamCreated", 
      "mediaStreamDestroyed", 
      "remoteVideoStatus", 
      "videoDimensionsChanged", 
      "startBrowserShare", 
      "endScreenShare", 
      "toggleBrowserSharing", 
      "updateSocialShareInfo", 
      "connectionStatus", 
      "mediaConnected", 
      "videoScreenStreamChanged"];

      // Register actions that are only used on Desktop.
      if (this._isDesktop) {
        // 'receivedTextChatMessage' and  'sendTextChatMessage' actions are only
        // registered for Telemetry. Once measured, they're unregistered.
        actions.push("receivedTextChatMessage", "sendTextChatMessage");}

      this.dispatcher.register(this, actions);

      this._onUpdateListener = this._handleRoomUpdate.bind(this);
      this._onDeleteListener = this._handleRoomDelete.bind(this);
      this._onSocialShareUpdate = this._handleSocialShareUpdate.bind(this);

      var roomToken = this._storeState.roomToken;
      loop.request("Rooms:PushSubscription", ["delete:" + roomToken, "update:" + roomToken]);
      loop.subscribe("Rooms:Delete:" + roomToken, this._handleRoomDelete.bind(this));
      loop.subscribe("Rooms:Update:" + roomToken, this._handleRoomUpdate.bind(this));
      loop.subscribe("SocialProvidersChanged", this._onSocialShareUpdate);}, 


    /**
     * Execute setupWindowData event action from the dispatcher. This gets
     * the room data from the Loop API, and dispatches an UpdateRoomInfo event.
     * It also dispatches JoinRoom as this action is only applicable to the desktop
     * client, and needs to auto-join.
     *
     * @param {sharedActions.SetupWindowData} actionData
     */
    setupWindowData: function setupWindowData(actionData) {
      if (actionData.type !== "room") {
        // Nothing for us to do here, leave it to other stores.
        return Promise.resolve();}


      this.setStoreState({ 
        roomState: ROOM_STATES.GATHER, 
        roomToken: actionData.roomToken, 
        windowId: actionData.windowId });


      this._registerPostSetupActions();

      // Get the window data from the Loop API.
      return loop.requestMulti(
      ["Rooms:Get", actionData.roomToken], 
      ["GetSocialShareProviders"]).
      then(function (results) {
        var room = results[0];
        var socialShareProviders = results[1];

        if (room.isError) {
          this.dispatchAction(new sharedActions.RoomFailure({ 
            error: room, 
            failedJoinRequest: false }));

          return;}


        this.dispatchAction(new sharedActions.UpdateRoomInfo({ 
          participants: room.participants, 
          roomContextUrls: room.decryptedContext.urls, 
          roomDescription: room.decryptedContext.description, 
          roomName: room.decryptedContext.roomName, 
          roomState: ROOM_STATES.READY, 
          roomUrl: room.roomUrl, 
          socialShareProviders: socialShareProviders }));


        // For the conversation window, we need to automatically join the room.
        this.dispatchAction(new sharedActions.JoinRoom());}.
      bind(this));}, 


    /**
     * Execute fetchServerData event action from the dispatcher. For rooms
     * we need to get the room context information from the server. We don't
     * need other data until the user decides to join the room.
     * This action is only used for the standalone UI.
     *
     * @param {sharedActions.FetchServerData} actionData
     * @return {Promise} For testing purposes, returns a promise that is resolved
     *                   once data is received from the server, and it is determined
     *                   if Firefox handles the room or not.
     */
    fetchServerData: function fetchServerData(actionData) {
      if (actionData.windowType !== "room") {
        // Nothing for us to do here, leave it to other stores.
        return Promise.resolve();}


      this.setStoreState({ 
        roomState: ROOM_STATES.GATHER, 
        roomToken: actionData.token, 
        roomCryptoKey: actionData.cryptoKey, 
        standalone: true });


      this._registerPostSetupActions();

      var dataPromise = this._getRoomDataForStandalone(actionData.cryptoKey);

      var userAgentHandlesPromise = this._promiseDetectUserAgentHandles();

      return Promise.all([dataPromise, userAgentHandlesPromise]).then(function (results) {
        results.forEach(function (result) {
          this.dispatcher.dispatch(result);}.
        bind(this));}.
      bind(this));}, 


    /**
     * Gets the room data for the standalone, decrypting it as necessary.
     *
     * @param  {String} roomCryptoKey The crypto key associated to the room.
     * @return {Promise}              A promise that is resolved once the get
     *                                and decryption is complete.
     */
    _getRoomDataForStandalone: function _getRoomDataForStandalone(roomCryptoKey) {
      return new Promise(function (resolve) {
        loop.request("Rooms:Get", this._storeState.roomToken).then(function (result) {
          if (result.isError) {
            resolve(new sharedActions.RoomFailure({ 
              error: result, 
              failedJoinRequest: false }));

            return;}


          var roomInfoData = new sharedActions.UpdateRoomInfo({ 
            // If we've got this far, then we want to go to the ready state
            // regardless of success of failure. This is because failures of
            // crypto don't stop the user using the room, they just stop
            // us putting up the information.
            roomState: ROOM_STATES.READY, 
            roomUrl: result.roomUrl });


          if (!result.context && !result.roomName) {
            roomInfoData.roomInfoFailure = ROOM_INFO_FAILURES.NO_DATA;
            resolve(roomInfoData);
            return;}


          // This handles 'legacy', non-encrypted room names.
          if (result.roomName && !result.context) {
            roomInfoData.roomName = result.roomName;
            resolve(roomInfoData);
            return;}


          if (!crypto.isSupported()) {
            roomInfoData.roomInfoFailure = ROOM_INFO_FAILURES.WEB_CRYPTO_UNSUPPORTED;
            resolve(roomInfoData);
            return;}


          if (!roomCryptoKey) {
            roomInfoData.roomInfoFailure = ROOM_INFO_FAILURES.NO_CRYPTO_KEY;
            resolve(roomInfoData);
            return;}


          crypto.decryptBytes(roomCryptoKey, result.context.value).
          then(function (decryptedResult) {
            var realResult = JSON.parse(decryptedResult);

            roomInfoData.roomDescription = realResult.description;
            roomInfoData.roomContextUrls = realResult.urls;
            roomInfoData.roomName = realResult.roomName;

            resolve(roomInfoData);}, 
          function () {
            roomInfoData.roomInfoFailure = ROOM_INFO_FAILURES.DECRYPT_FAILED;
            resolve(roomInfoData);});}.

        bind(this));}.
      bind(this));}, 


    /**
     * If the user agent is Firefox, it sends a message to Firefox to see if
     * the room can be handled within Firefox rather than the standalone UI.
     *
     * @return {Promise} A promise that is resolved once it has been determined
     *                   if Firefox can handle the room.
     */
    _promiseDetectUserAgentHandles: function _promiseDetectUserAgentHandles() {
      return new Promise(function (resolve) {
        function resolveWithNotHandlingResponse() {
          resolve(new sharedActions.UserAgentHandlesRoom({ 
            handlesRoom: false }));}



        // If we're not Firefox, don't even try to see if it can be handled
        // in the browser.
        if (!loop.shared.utils.isFirefox(navigator.userAgent)) {
          resolveWithNotHandlingResponse();
          return;}


        // Set up a timer in case older versions of Firefox don't give us a response.
        var timer = setTimeout(resolveWithNotHandlingResponse, 250);
        var webChannelListenerFunc;

        // Listen for the result.
        function webChannelListener(e) {
          if (e.detail.id !== "loop-link-clicker") {
            return;}


          // Stop the default response.
          clearTimeout(timer);

          // Remove the listener.
          window.removeEventListener("WebChannelMessageToContent", webChannelListenerFunc);

          // Resolve with the details of if we're able to handle or not.
          resolve(new sharedActions.UserAgentHandlesRoom({ 
            handlesRoom: !!e.detail.message && e.detail.message.response }));}



        webChannelListenerFunc = webChannelListener.bind(this);

        window.addEventListener("WebChannelMessageToContent", webChannelListenerFunc);

        // Now send a message to the chrome to see if it can handle this room.
        window.dispatchEvent(new window.CustomEvent("WebChannelMessageToChrome", { 
          detail: { 
            id: "loop-link-clicker", 
            message: { 
              command: "checkWillOpenRoom", 
              roomToken: this._storeState.roomToken } } }));}.



      bind(this));}, 


    /**
     * Handles the updateRoomInfo action. Updates the room data.
     *
     * @param {sharedActions.UpdateRoomInfo} actionData
     */
    updateRoomInfo: function updateRoomInfo(actionData) {
      var newState = { 
        roomUrl: actionData.roomUrl };

      // Iterate over the optional fields that _may_ be present on the actionData
      // object.
      Object.keys(OPTIONAL_ROOMINFO_FIELDS).forEach(function (field) {
        if (actionData[field] !== undefined) {
          newState[OPTIONAL_ROOMINFO_FIELDS[field]] = actionData[field];}});


      this.setStoreState(newState);}, 


    /**
     * Handles the userAgentHandlesRoom action. Updates the store's data with
     * the new state.
     *
     * @param {sharedActions.userAgentHandlesRoom} actionData
     */
    userAgentHandlesRoom: function userAgentHandlesRoom(actionData) {
      this.setStoreState({ 
        userAgentHandlesRoom: actionData.handlesRoom });}, 



    /**
     * Handles the updateSocialShareInfo action. Updates the room data with new
     * Social API info.
     *
     * @param  {sharedActions.UpdateSocialShareInfo} actionData
     */
    updateSocialShareInfo: function updateSocialShareInfo(actionData) {
      this.setStoreState({ 
        socialShareProviders: actionData.socialShareProviders });}, 



    /**
     * Handles room updates notified by the Loop rooms API.
     *
     * @param {Object} roomData  The new roomData.
     */
    _handleRoomUpdate: function _handleRoomUpdate(roomData) {
      this.dispatchAction(new sharedActions.UpdateRoomInfo({ 
        roomContextUrls: roomData.decryptedContext.urls, 
        roomDescription: roomData.decryptedContext.description, 
        participants: roomData.participants, 
        roomName: roomData.decryptedContext.roomName, 
        roomUrl: roomData.roomUrl }));}, 



    /**
     * Handles the deletion of a room, notified by the Loop rooms API.
     *
     */
    _handleRoomDelete: function _handleRoomDelete() {
      this._sdkDriver.forceDisconnectAll(function () {
        window.close();});}, 



    /**
     * Handles an update of the position of the Share widget and changes to list
     * of Social API providers, notified by the Loop API.
     */
    _handleSocialShareUpdate: function _handleSocialShareUpdate() {
      loop.request("GetSocialShareProviders").then(function (result) {
        this.dispatchAction(new sharedActions.UpdateSocialShareInfo({ 
          socialShareProviders: result }));}.

      bind(this));}, 


    /**
     * Checks that there are audio and video devices available, and joins the
     * room if there are. If there aren't then it will dispatch a ConnectionFailure
     * action with NO_MEDIA.
     */
    _checkDevicesAndJoinRoom: function _checkDevicesAndJoinRoom() {
      // XXX Ideally we'd do this check before joining a room, but we're waiting
      // for the UX for that. See bug 1166824. In the meantime this gives us
      // additional information for analysis.
      loop.shared.utils.hasAudioOrVideoDevices(function (hasDevices) {
        if (hasDevices) {
          // MEDIA_WAIT causes the views to dispatch sharedActions.SetupStreamElements,
          // which in turn starts the sdk obtaining the device permission.
          this.setStoreState({ roomState: ROOM_STATES.MEDIA_WAIT });} else 
        {
          this.dispatchAction(new sharedActions.ConnectionFailure({ 
            reason: FAILURE_DETAILS.NO_MEDIA }));}}.


      bind(this));}, 


    /**
     * Hands off the room join to Firefox.
     */
    _handoffRoomJoin: function _handoffRoomJoin() {
      var channelListener;

      function handleRoomJoinResponse(e) {
        if (e.detail.id !== "loop-link-clicker") {
          return;}


        window.removeEventListener("WebChannelMessageToContent", channelListener);

        if (!e.detail.message || !e.detail.message.response) {
          // XXX Firefox didn't handle this, even though it said it could
          // previously. We should add better user feedback here.
          console.error("Firefox didn't handle room it said it could.");} else 
        if (e.detail.message.alreadyOpen) {
          this.dispatcher.dispatch(new sharedActions.ConnectionFailure({ 
            reason: FAILURE_DETAILS.ROOM_ALREADY_OPEN }));} else 

        {
          this.dispatcher.dispatch(new sharedActions.JoinedRoom({ 
            apiKey: "", 
            sessionToken: "", 
            sessionId: "", 
            expires: 0 }));}}




      channelListener = handleRoomJoinResponse.bind(this);

      window.addEventListener("WebChannelMessageToContent", channelListener);

      // Now we're set up, dispatch an event.
      window.dispatchEvent(new window.CustomEvent("WebChannelMessageToChrome", { 
        detail: { 
          id: "loop-link-clicker", 
          message: { 
            command: "openRoom", 
            roomToken: this._storeState.roomToken } } }));}, 





    /**
     * Handles the action to join to a room.
     */
    joinRoom: function joinRoom() {
      // Reset the failure reason if necessary.
      if (this.getStoreState().failureReason) {
        this.setStoreState({ failureReason: undefined });}


      // If we're standalone and we know Firefox can handle the room, then hand
      // it off.
      if (this._storeState.standalone && this._storeState.userAgentHandlesRoom) {
        this.dispatcher.dispatch(new sharedActions.MetricsLogJoinRoom({ 
          userAgentHandledRoom: true, 
          ownRoom: true }));

        this._handoffRoomJoin();
        return;}


      this.dispatcher.dispatch(new sharedActions.MetricsLogJoinRoom({ 
        userAgentHandledRoom: false }));


      // Otherwise, we handle the room ourselves.
      this._checkDevicesAndJoinRoom();}, 


    /**
     * Handles the action that signifies when media permission has been
     * granted and starts joining the room.
     */
    gotMediaPermission: function gotMediaPermission() {
      this.setStoreState({ roomState: ROOM_STATES.JOINING });

      loop.request("Rooms:Join", this._storeState.roomToken, 
      mozL10n.get("display_name_guest")).then(function (result) {
        if (result.isError) {
          this.dispatchAction(new sharedActions.RoomFailure({ 
            error: result, 
            // This is an explicit flag to avoid the leave happening if join
            // fails. We can't track it on ROOM_STATES.JOINING as the user
            // might choose to leave the room whilst the XHR is in progress
            // which would then mean we'd run the race condition of not
            // notifying the server of a leave.
            failedJoinRequest: true }));

          return;}


        this.dispatchAction(new sharedActions.JoinedRoom({ 
          apiKey: result.apiKey, 
          sessionToken: result.sessionToken, 
          sessionId: result.sessionId, 
          expires: result.expires }));}.

      bind(this));}, 


    /**
     * Handles the data received from joining a room. It stores the relevant
     * data, and sets up the refresh timeout for ensuring membership of the room
     * is refreshed regularly.
     *
     * @param {sharedActions.JoinedRoom} actionData
     */
    joinedRoom: function joinedRoom(actionData) {
      // If we're standalone and firefox is handling, then just store the new
      // state. No need to do anything else.
      if (this._storeState.standalone && this._storeState.userAgentHandlesRoom) {
        this.setStoreState({ 
          roomState: ROOM_STATES.JOINED });

        return;}


      this.setStoreState({ 
        apiKey: actionData.apiKey, 
        sessionToken: actionData.sessionToken, 
        sessionId: actionData.sessionId, 
        roomState: ROOM_STATES.JOINED });


      this._setRefreshTimeout(actionData.expires);

      // Only send media telemetry on one side of the call: the desktop side.
      actionData.sendTwoWayMediaTelemetry = this._isDesktop;

      this._sdkDriver.connectSession(actionData);

      loop.request("AddConversationContext", this._storeState.windowId, 
      actionData.sessionId, "");}, 


    /**
     * Handles recording when the sdk has connected to the servers.
     */
    connectedToSdkServers: function connectedToSdkServers() {
      this.setStoreState({ 
        roomState: ROOM_STATES.SESSION_CONNECTED });}, 



    /**
     * Handles disconnection of this local client from the sdk servers.
     *
     * @param {sharedActions.ConnectionFailure} actionData
     */
    connectionFailure: function connectionFailure(actionData) {
      var exitState = this._storeState.roomState === ROOM_STATES.FAILED ? 
      this._storeState.failureExitState : this._storeState.roomState;

      // Treat all reasons as something failed. In theory, clientDisconnected
      // could be a success case, but there's no way we should be intentionally
      // sending that and still have the window open.
      this.setStoreState({ 
        failureReason: actionData.reason, 
        failureExitState: exitState });


      this._leaveRoom(ROOM_STATES.FAILED);}, 


    /**
     * Records the mute state for the stream.
     *
     * @param {sharedActions.setMute} actionData The mute state for the stream type.
     */
    setMute: function setMute(actionData) {
      var muteState = {};
      muteState[actionData.type + "Muted"] = !actionData.enabled;
      this.setStoreState(muteState);}, 


    /**
     * Handles a media stream being created. This may be a local or a remote stream.
     *
     * @param {sharedActions.MediaStreamCreated} actionData
     */
    mediaStreamCreated: function mediaStreamCreated(actionData) {
      if (actionData.isLocal) {
        this.setStoreState({ 
          localAudioEnabled: actionData.hasAudio, 
          localVideoEnabled: actionData.hasVideo, 
          localSrcMediaElement: actionData.srcMediaElement });

        return;}


      this.setStoreState({ 
        remoteAudioEnabled: actionData.hasAudio, 
        remoteVideoEnabled: actionData.hasVideo, 
        remoteSrcMediaElement: actionData.srcMediaElement });}, 



    /**
     * Handles a media stream being destroyed. This may be a local or a remote stream.
     *
     * @param {sharedActions.MediaStreamDestroyed} actionData
     */
    mediaStreamDestroyed: function mediaStreamDestroyed(actionData) {
      if (actionData.isLocal) {
        this.setStoreState({ 
          localSrcMediaElement: null });

        return;}


      this.setStoreState({ 
        remoteSrcMediaElement: null });}, 



    /**
     * Handles a remote stream having video enabled or disabled.
     *
     * @param {sharedActions.RemoteVideoStatus} actionData
     */
    remoteVideoStatus: function remoteVideoStatus(actionData) {
      this.setStoreState({ 
        remoteVideoEnabled: actionData.videoEnabled });}, 



    /**
     * Records when the remote media has been connected.
     */
    mediaConnected: function mediaConnected() {
      this.setStoreState({ mediaConnected: true });}, 


    /**
     * Used to note the current screensharing state.
     */
    screenSharingState: function screenSharingState(actionData) {
      this.setStoreState({ screenSharingState: actionData.state });

      loop.request("SetScreenShareState", this.getStoreState().windowId, 
      actionData.state === SCREEN_SHARE_STATES.ACTIVE);}, 


    /**
     * Used to note the current state of receiving screenshare data.
     *
     * XXX this should be split into multiple actions to make the code clearer.
     */
    receivingScreenShare: function receivingScreenShare(actionData) {
      if (!actionData.receiving && 
      this.getStoreState().remoteVideoDimensions.screen) {
        // Remove the remote video dimensions for type screen as we're not
        // getting the share anymore.
        var newDimensions = _.extend(this.getStoreState().remoteVideoDimensions);
        delete newDimensions.screen;
        this.setStoreState({ 
          receivingScreenShare: actionData.receiving, 
          remoteVideoDimensions: newDimensions, 
          screenShareMediaElement: null });} else 

      {
        this.setStoreState({ 
          receivingScreenShare: actionData.receiving, 
          screenShareMediaElement: actionData.srcMediaElement ? 
          actionData.srcMediaElement : null });}}, 




    /**
     * Handles switching browser (aka tab) sharing to a new window. Should
     * only be used for browser sharing.
     *
     * @param {Number} windowId  The new windowId to start sharing.
     */
    _handleSwitchBrowserShare: function _handleSwitchBrowserShare(windowId) {
      if (Array.isArray(windowId)) {
        windowId = windowId[0];}

      if (!windowId) {
        return;}

      if (windowId.isError) {
        console.error("Error getting the windowId: " + windowId.message);
        this.dispatchAction(new sharedActions.ScreenSharingState({ 
          state: SCREEN_SHARE_STATES.INACTIVE }));

        return;}


      var screenSharingState = this.getStoreState().screenSharingState;

      if (screenSharingState === SCREEN_SHARE_STATES.PENDING) {
        // Screen sharing is still pending, so assume that we need to kick it off.
        var options = { 
          videoSource: "browser", 
          constraints: { 
            browserWindow: windowId, 
            scrollWithPage: true } };


        this._sdkDriver.startScreenShare(options);} else 
      if (screenSharingState === SCREEN_SHARE_STATES.ACTIVE) {
        // Just update the current share.
        this._sdkDriver.switchAcquiredWindow(windowId);} else 
      {
        console.error("Unexpectedly received windowId for browser sharing when pending");}


      // Only update context if sharing is not paused and there's somebody.
      if (!this.getStoreState().sharingPaused && this._hasParticipants()) {
        this._checkTabContext();}}, 



    /**
     * Get the current tab context to update the room context.
     */
    _checkTabContext: function _checkTabContext() {
      loop.request("GetSelectedTabMetadata").then(function (meta) {
        // Avoid sending the event if there is no data nor url.
        if (!meta || !meta.url) {
          return;}


        if (updateContextTimer) {
          clearTimeout(updateContextTimer);}


        updateContextTimer = setTimeout(function () {
          this.dispatchAction(new sharedActions.UpdateRoomContext({ 
            newRoomDescription: meta.title || meta.description || meta.url, 
            newRoomThumbnail: meta.favicon, 
            newRoomURL: meta.url, 
            roomToken: this.getStoreState().roomToken }));

          updateContextTimer = null;}.
        bind(this), 500);}.
      bind(this));}, 


    /**
     * Initiates a browser tab sharing publisher.
     *
     * @param {sharedActions.StartBrowserShare} actionData
     */
    startBrowserShare: function startBrowserShare() {
      if (this._storeState.screenSharingState !== SCREEN_SHARE_STATES.INACTIVE) {
        console.error("Attempting to start browser sharing when already running.");
        return;}


      // For the unit test we already set the state here, instead of indirectly
      // via an action, because actions are queued thus depending on the
      // asynchronous nature of `loop.request`.
      this.setStoreState({ screenSharingState: SCREEN_SHARE_STATES.PENDING });
      this.dispatchAction(new sharedActions.ScreenSharingState({ 
        state: SCREEN_SHARE_STATES.PENDING }));


      this._browserSharingListener = this._handleSwitchBrowserShare.bind(this);

      // Set up a listener for watching screen shares. This will get notified
      // with the first windowId when it is added, so we start off the sharing
      // from within the listener.
      loop.request("AddBrowserSharingListener", this.getStoreState().windowId).
      then(this._browserSharingListener);
      loop.subscribe("BrowserSwitch", this._browserSharingListener);}, 


    /**
     * Ends an active screenshare session.
     */
    endScreenShare: function endScreenShare() {
      if (this._browserSharingListener) {
        // Remove the browser sharing listener as we don't need it now.
        loop.request("RemoveBrowserSharingListener", this.getStoreState().windowId);
        loop.unsubscribe("BrowserSwitch", this._browserSharingListener);
        this._browserSharingListener = null;}


      if (this._sdkDriver.endScreenShare()) {
        this.dispatchAction(new sharedActions.ScreenSharingState({ 
          state: SCREEN_SHARE_STATES.INACTIVE }));}}, 




    /**
     * Handle browser sharing being enabled or disabled.
     *
     * @param {sharedActions.ToggleBrowserSharing} actionData
     */
    toggleBrowserSharing: function toggleBrowserSharing(actionData) {
      this.setStoreState({ 
        sharingPaused: !actionData.enabled });


      // If unpausing, check the context as it might have changed.
      if (actionData.enabled) {
        this._checkTabContext();}}, 



    /**
     * Handles recording when a remote peer has connected to the servers.
     */
    remotePeerConnected: function remotePeerConnected() {
      this.setStoreState({ 
        remotePeerDisconnected: false, 
        roomState: ROOM_STATES.HAS_PARTICIPANTS, 
        used: true });}, 



    /**
     * Handles a remote peer disconnecting from the session. As we currently only
     * support 2 participants, we declare the room as SESSION_CONNECTED as soon as
     * one participant leaves.
     */
    remotePeerDisconnected: function remotePeerDisconnected() {
      // Update the participants to just the owner.
      var participants = this.getStoreState("participants");
      if (participants) {
        participants = participants.filter(function (participant) {
          return participant.owner;});}



      this.setStoreState({ 
        mediaConnected: false, 
        participants: participants, 
        roomState: ROOM_STATES.SESSION_CONNECTED, 
        remotePeerDisconnected: true, 
        remoteSrcMediaElement: null, 
        streamPaused: false });}, 



    /**
     * Handles an SDK status update, forwarding it to the server.
     *
     * @param {sharedActions.ConnectionStatus} actionData
     */
    connectionStatus: function connectionStatus(actionData) {
      loop.request("Rooms:SendConnectionStatus", this.getStoreState("roomToken"), 
      this.getStoreState("sessionToken"), actionData);}, 


    /**
     * Handles the window being unloaded. Ensures the room is left.
     */
    windowUnload: function windowUnload() {
      this._leaveRoom(ROOM_STATES.CLOSING);

      if (!this._onUpdateListener) {
        return;}


      // If we're closing the window, we can stop listening to updates.
      var roomToken = this.getStoreState().roomToken;
      loop.unsubscribe("Rooms:Update:" + roomToken, this._onUpdateListener);
      loop.unsubscribe("Rooms:Delete:" + roomToken, this._onDeleteListener);
      loop.unsubscribe("SocialProvidersChanged", this._onSocialProvidersUpdate);
      delete this._onUpdateListener;
      delete this._onDeleteListener;
      delete this._onShareWidgetUpdate;
      delete this._onSocialProvidersUpdate;}, 


    /**
     * Handles a room being left.
     *
     * @param {sharedActions.LeaveRoom} actionData
     */
    leaveRoom: function leaveRoom(actionData) {
      this._leaveRoom(ROOM_STATES.ENDED, 
      false, 
      actionData && actionData.windowStayingOpen);}, 


    /**
     * Handles setting of the refresh timeout callback.
     *
     * @param {Integer} expireTime The time until expiry (in seconds).
     */
    _setRefreshTimeout: function _setRefreshTimeout(expireTime) {
      this._timeout = setTimeout(this._refreshMembership.bind(this), 
      expireTime * this.expiresTimeFactor * 1000);}, 


    /**
     * Refreshes the membership of the room with the server, and then
     * sets up the refresh for the next cycle.
     */
    _refreshMembership: function _refreshMembership() {
      loop.request("Rooms:RefreshMembership", this._storeState.roomToken, 
      this._storeState.sessionToken).
      then(function (result) {
        if (result.isError) {
          this.dispatchAction(new sharedActions.RoomFailure({ 
            error: result, 
            failedJoinRequest: false }));

          return;}


        this._setRefreshTimeout(result.expires);}.
      bind(this));}, 


    /**
     * Handles leaving a room. Clears any membership timeouts, then
     * signals to the server the leave of the room.
     * NOTE: if you add something here, please also consider if something needs
     *       to be done on the chrome side as well (e.g.
     *       MozLoopService#openChatWindow).
     *
     * @param {ROOM_STATES} nextState         The next state to switch to.
     * @param {Boolean}     failedJoinRequest Optional. Set to true if the join
     *                                        request to loop-server failed. It
     *                                        will skip the leave message.
     * @param {Boolean}     windowStayingOpen Optional. Set to true to ensure
     *                                        that messages relating to ending
     *                                        of the conversation are sent on desktop.
     */
    _leaveRoom: function _leaveRoom(nextState, failedJoinRequest, windowStayingOpen) {
      if (this._storeState.standalone && this._storeState.userAgentHandlesRoom) {
        // If the user agent is handling the room, all we need to do is advance
        // to the next state.
        this.setStoreState({ 
          roomState: nextState });

        return;}


      if (loop.standaloneMedia) {
        loop.standaloneMedia.multiplexGum.reset();}


      if (this._browserSharingListener) {
        // Remove the browser sharing listener as we don't need it now.
        loop.unsubscribe("BrowserSwitch", this._browserSharingListener);
        this._browserSharingListener = null;}


      // We probably don't need to end screen share separately, but lets be safe.
      this._sdkDriver.disconnectSession();

      // Reset various states.
      var originalStoreState = this.getInitialStoreState();
      var newStoreState = {};

      this._statesToResetOnLeave.forEach(function (state) {
        newStoreState[state] = originalStoreState[state];});

      this.setStoreState(newStoreState);

      if (this._timeout) {
        clearTimeout(this._timeout);
        delete this._timeout;}


      // If we're not going to close the window, we can hangup the call ourselves.
      // NOTE: when the window _is_ closed, hanging up the call is performed by
      //       MozLoopService, because we can't get a message across to LoopAPI
      //       in time whilst a window is closing.
      if ((nextState === ROOM_STATES.FAILED || windowStayingOpen || !this._isDesktop) && 
      !failedJoinRequest) {
        loop.request("HangupNow", this._storeState.roomToken, 
        this._storeState.sessionToken, this._storeState.windowId);}


      this.setStoreState({ roomState: nextState });}, 


    /**
     * When feedback is complete, we go back to the ready state, rather than
     * init or gather, as we don't need to get the data from the server again.
     */
    feedbackComplete: function feedbackComplete() {
      this.setStoreState({ 
        roomState: ROOM_STATES.READY, 
        // Reset the used state here as the user has now given feedback and the
        // next time they enter the room, the other person might not be there.
        used: false });}, 



    /**
     * Handles a change in dimensions of a video stream and updates the store data
     * with the new dimensions of a local or remote stream.
     *
     * @param {sharedActions.VideoDimensionsChanged} actionData
     */
    videoDimensionsChanged: function videoDimensionsChanged(actionData) {
      // NOTE: in the future, when multiple remote video streams are supported,
      //       we'll need to make this support multiple remotes as well. Good
      //       starting point for video tiling.
      var storeProp = (actionData.isLocal ? "local" : "remote") + "VideoDimensions";
      var nextState = {};
      nextState[storeProp] = this.getStoreState()[storeProp];
      nextState[storeProp][actionData.videoType] = actionData.dimensions;
      this.setStoreState(nextState);}, 


    /**
     * Listen to screen stream changes in order to check if sharing screen
     * has been paused.
     *
     * @param {sharedActions.VideoScreenStreamChanged} actionData
     */
    videoScreenStreamChanged: function videoScreenStreamChanged(actionData) {
      this.setStoreState({ 
        streamPaused: !actionData.hasVideo });}, 



    /**
     * Handles chat messages received and/ or about to send. If this is the first
     * chat message for the current session, register a count with telemetry.
     * It will unhook the listeners when the telemetry criteria have been
     * fulfilled to make sure we remain lean.
     * Note: the 'receivedTextChatMessage' and 'sendTextChatMessage' actions are
     *       only registered on Desktop.
     *
     * @param  {sharedActions.ReceivedTextChatMessage|SendTextChatMessage} actionData
     */
    _handleTextChatMessage: function _handleTextChatMessage(actionData) {
      if (!this._isDesktop || this.getStoreState().chatMessageExchanged || 
      actionData.contentType !== CHAT_CONTENT_TYPES.TEXT) {
        return;}


      this.setStoreState({ chatMessageExchanged: true });
      // There's no need to listen to these actions anymore.
      this.dispatcher.unregister(this, [
      "receivedTextChatMessage", 
      "sendTextChatMessage"]);

      // Ping telemetry of this session with successful message(s) exchange.
      loop.request("TelemetryAddValue", "LOOP_ROOM_SESSION_WITHCHAT", 1);}, 


    /**
     * Handles received text chat messages. For telemetry purposes only.
     *
     * @param {sharedActions.ReceivedTextChatMessage} actionData
     */
    receivedTextChatMessage: function receivedTextChatMessage(actionData) {
      this._handleTextChatMessage(actionData);}, 


    /**
     * Handles sending of a chat message. For telemetry purposes only.
     *
     * @param {sharedActions.SendTextChatMessage} actionData
     */
    sendTextChatMessage: function sendTextChatMessage(actionData) {
      this._handleTextChatMessage(actionData);}, 


    /**
     * Checks if the room is empty or has participants.
     *
     */
    _hasParticipants: function _hasParticipants() {
      // Update the participants to just the owner.
      var participants = this.getStoreState("participants");
      if (participants) {
        participants = participants.filter(function (participant) {
          return !participant.owner;});


        return participants.length > 0;}


      return false;} });



  return ActiveRoomStore;}(
navigator.mozL10n || document.mozL10n);
