"use strict"; /* This Source Code Form is subject to the terms of the Mozilla Public
               * License, v. 2.0. If a copy of the MPL was not distributed with this file,
               * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.store = loop.store || {};

(function (mozL10n) {
  "use strict";

  /**
   * Shared actions.
   * @type {Object}
   */
  var sharedActions = loop.shared.actions;

  /**
   * Maximum size given to createRoom; only 2 is supported (and is
   * always passed) because that's what the user-experience is currently
   * designed and tested to handle.
   * @type {Number}
   */
  var MAX_ROOM_CREATION_SIZE = loop.store.MAX_ROOM_CREATION_SIZE = 2;

  /**
   * Room validation schema. See validate.js.
   * @type {Object}
   */
  var roomSchema = { 
    roomToken: String, 
    roomUrl: String, 
    // roomName: String - Optional.
    // roomKey: String - Optional.
    maxSize: Number, 
    participants: Array, 
    ctime: Number };


  /**
   * Room type. Basically acts as a typed object constructor.
   *
   * @param {Object} values Room property values.
   */
  function Room(values) {var _this = this;
    var validatedData = new loop.validate.Validator(roomSchema || {}).
    validate(values || {});
    Object.keys(validatedData).forEach(function (prop) {
      _this[prop] = validatedData[prop];});}



  loop.store.Room = Room;

  /**
   * Room store.
   *
   * @param {loop.Dispatcher} dispatcher  The dispatcher for dispatching actions
   *                                      and registering to consume actions.
   * @param {Object} options Options object:
   * - {ActiveRoomStore} activeRoomStore  An optional substore for active room
   *                                      state.
   * - {Notifications}   notifications    A notifications item that is required.
   * - {Object}          constants        A set of constants that are used
   *                                      throughout the store.
   */
  loop.store.RoomStore = loop.store.createStore({ 
    /**
     * Maximum size given to createRoom; only 2 is supported (and is
     * always passed) because that's what the user-experience is currently
     * designed and tested to handle.
     * @type {Number}
     */
    maxRoomCreationSize: MAX_ROOM_CREATION_SIZE, 

    /**
     * Registered actions.
     * @type {Array}
     */
    actions: [
    "addSocialShareProvider", 
    "createRoom", 
    "createdRoom", 
    "createRoomError", 
    "copyRoomUrl", 
    "deleteRoom", 
    "deleteRoomError", 
    "emailRoomUrl", 
    "facebookShareRoomUrl", 
    "getAllRooms", 
    "getAllRoomsError", 
    "openRoom", 
    "shareRoomUrl", 
    "updateRoomContext", 
    "updateRoomContextDone", 
    "updateRoomContextError", 
    "updateRoomList"], 


    initialize: function initialize(options) {
      if (!options.constants) {
        throw new Error("Missing option constants");}


      this._notifications = options.notifications;
      this._constants = options.constants;
      this._gotAllRooms = false;

      if (options.activeRoomStore) {
        this.activeRoomStore = options.activeRoomStore;
        this.activeRoomStore.on("change", 
        this._onActiveRoomStoreChange.bind(this));}}, 



    getInitialStoreState: function getInitialStoreState() {
      return { 
        activeRoom: this.activeRoomStore ? this.activeRoomStore.getStoreState() : {}, 
        closingNewRoom: false, 
        error: null, 
        lastCreatedRoom: null, 
        openedRoom: null, 
        pendingCreation: false, 
        pendingInitialRetrieval: true, 
        rooms: [], 
        savingContext: false };}, 



    /**
     * Registers Loop API rooms events.
     */
    startListeningToRoomEvents: function startListeningToRoomEvents() {
      // Rooms event registration
      loop.request("Rooms:PushSubscription", ["add", "close", "delete", "open", 
      "refresh", "update"]);
      loop.subscribe("Rooms:Add", this._onRoomAdded.bind(this));
      loop.subscribe("Rooms:Close", this._onRoomClose.bind(this));
      loop.subscribe("Rooms:Open", this._onRoomOpen.bind(this));
      loop.subscribe("Rooms:Update", this._onRoomUpdated.bind(this));
      loop.subscribe("Rooms:Delete", this._onRoomRemoved.bind(this));
      loop.subscribe("Rooms:Refresh", this._onRoomsRefresh.bind(this));}, 


    /**
     * Updates active room store state.
     */
    _onActiveRoomStoreChange: function _onActiveRoomStoreChange() {
      this.setStoreState({ activeRoom: this.activeRoomStore.getStoreState() });}, 


    /**
     * Updates current room list when a new room is available.
     *
     * @param {Object} addedRoomData The added room data.
     */
    _onRoomAdded: function _onRoomAdded(addedRoomData) {
      addedRoomData.participants = addedRoomData.participants || [];
      addedRoomData.ctime = addedRoomData.ctime || new Date().getTime();

      this.dispatchAction(new sharedActions.UpdateRoomList({ 
        // Ensure the room isn't part of the list already, then add it.
        roomList: this._storeState.rooms.filter(function (room) {
          return addedRoomData.roomToken !== room.roomToken;}).
        concat(new Room(addedRoomData)) }));}, 



    /**
     * Clears the current active room.
     */
    _onRoomClose: function _onRoomClose() {
      var state = this.getStoreState();

      // If the room getting closed has been just created, then open the panel.
      if (state.lastCreatedRoom && state.openedRoom === state.lastCreatedRoom) {
        this.setStoreState({ 
          closingNewRoom: true });

        loop.request("SetNameNewRoom");}


      // reset state for closed room
      this.setStoreState({ 
        closingNewRoom: false, 
        lastCreatedRoom: null, 
        openedRoom: null });}, 



    /**
     * Updates the current active room.
     *
     * @param {String} roomToken Identifier of the room.
     */
    _onRoomOpen: function _onRoomOpen(roomToken) {
      this.setStoreState({ 
        openedRoom: roomToken });}, 



    /**
     * Executed when a room is updated.
     *
     * @param {Object} updatedRoomData The updated room data.
     */
    _onRoomUpdated: function _onRoomUpdated(updatedRoomData) {
      this.dispatchAction(new sharedActions.UpdateRoomList({ 
        roomList: this._storeState.rooms.map(function (room) {
          return room.roomToken === updatedRoomData.roomToken ? 
          updatedRoomData : room;}) }));}, 




    /**
     * Executed when a room is deleted.
     *
     * @param {Object} removedRoomData The removed room data.
     */
    _onRoomRemoved: function _onRoomRemoved(removedRoomData) {
      this.dispatchAction(new sharedActions.UpdateRoomList({ 
        roomList: this._storeState.rooms.filter(function (room) {
          return room.roomToken !== removedRoomData.roomToken;}) }));}, 




    /**
     * Executed when the user switches accounts.
     */
    _onRoomsRefresh: function _onRoomsRefresh() {
      this.dispatchAction(new sharedActions.UpdateRoomList({ 
        roomList: [] }));}, 



    /**
     * Maps and sorts the raw room list received from the Loop API.
     *
     * @param  {Array} rawRoomList Raw room list.
     * @return {Array}
     */
    _processRoomList: function _processRoomList(rawRoomList) {
      if (!rawRoomList) {
        return [];}

      return rawRoomList.
      map(function (rawRoom) {
        return new Room(rawRoom);}).

      slice().
      sort(function (a, b) {
        return b.ctime - a.ctime;});}, 



    /**
     * Creates a new room.
     *
     * @param {sharedActions.CreateRoom} actionData The new room information.
     */
    createRoom: function createRoom(actionData) {
      this.setStoreState({ 
        pendingCreation: true, 
        error: null });


      var roomCreationData = { 
        decryptedContext: {}, 
        maxSize: this.maxRoomCreationSize };


      if ("urls" in actionData) {
        roomCreationData.decryptedContext.urls = actionData.urls;}


      this._notifications.remove("create-room-error");
      loop.request("Rooms:Create", roomCreationData).then(function (result) {
        var buckets = this._constants.ROOM_CREATE;
        if (!result || result.isError) {
          loop.request("TelemetryAddValue", "LOOP_ROOM_CREATE", buckets.CREATE_FAIL);
          this.dispatchAction(new sharedActions.CreateRoomError({ 
            error: result ? result : new Error("no result") }));

          return;}


        // Keep the token for the last created room.
        this.setStoreState({ 
          lastCreatedRoom: result.roomToken });


        this.dispatchAction(new sharedActions.CreatedRoom({ 
          decryptedContext: result.decryptedContext, 
          roomToken: result.roomToken, 
          roomUrl: result.roomUrl }));

        loop.request("TelemetryAddValue", "LOOP_ROOM_CREATE", buckets.CREATE_SUCCESS);}.
      bind(this));}, 


    /**
     * Executed when a room has been created
     */
    createdRoom: function createdRoom(actionData) {
      this.setStoreState({ 
        activeRoom: { 
          decryptedContext: actionData.decryptedContext, 
          roomToken: actionData.roomToken, 
          roomUrl: actionData.roomUrl }, 

        pendingCreation: false });}, 



    /**
     * Executed when a room creation error occurs.
     *
     * @param {sharedActions.CreateRoomError} actionData The action data.
     */
    createRoomError: function createRoomError(actionData) {
      this.setStoreState({ 
        error: actionData.error, 
        pendingCreation: false });


      // XXX Needs a more descriptive error - bug 1109151.
      this._notifications.set({ 
        id: "create-room-error", 
        level: "error", 
        message: mozL10n.get("generic_failure_message") });}, 



    /**
     * Copy a room url.
     *
     * @param  {sharedActions.CopyRoomUrl} actionData The action data.
     */
    copyRoomUrl: function copyRoomUrl(actionData) {
      loop.requestMulti(
      ["CopyString", actionData.roomUrl], 
      ["NotifyUITour", "Loop:RoomURLCopied"]);

      var from = actionData.from;
      var bucket = this._constants.SHARING_ROOM_URL["COPY_FROM_" + from.toUpperCase()];
      if (typeof bucket === "undefined") {
        console.error("No URL sharing type bucket found for '" + from + "'");
        return;}

      loop.requestMulti(
      ["TelemetryAddValue", "LOOP_SHARING_ROOM_URL", bucket], 
      ["TelemetryAddValue", "LOOP_ACTIVITY_COUNTER", this._constants.LOOP_MAU_TYPE.ROOM_SHARE]);}, 



    /**
     * Emails a room url.
     *
     * @param  {sharedActions.EmailRoomUrl} actionData The action data.
     */
    emailRoomUrl: function emailRoomUrl(actionData) {
      var from = actionData.from;
      loop.shared.utils.composeCallUrlEmail(actionData.roomUrl, null, 
      actionData.roomDescription);

      var bucket = this._constants.SHARING_ROOM_URL[
      "EMAIL_FROM_" + (from || "").toUpperCase()];

      if (typeof bucket === "undefined") {
        console.error("No URL sharing type bucket found for '" + from + "'");
        return;}

      loop.requestMulti(
      ["NotifyUITour", "Loop:RoomURLEmailed"], 
      ["TelemetryAddValue", "LOOP_SHARING_ROOM_URL", bucket], 
      ["TelemetryAddValue", "LOOP_ACTIVITY_COUNTER", this._constants.LOOP_MAU_TYPE.ROOM_SHARE]);}, 



    /**
     * Share a room url with Facebook
     *
     * @param  {sharedActions.FacebookShareRoomUrl} actionData The action data.
     */
    facebookShareRoomUrl: function facebookShareRoomUrl(actionData) {
      var encodedRoom = encodeURIComponent(actionData.roomUrl);

      loop.requestMulti(
      ["GetLoopPref", "facebook.appId"], 
      ["GetLoopPref", "facebook.fallbackUrl"], 
      ["GetLoopPref", "facebook.shareUrl"]).
      then(function (results) {
        var app_id = results[0];
        var fallback_url = results[1];
        var redirect_url = encodeURIComponent(actionData.originUrl || 
        fallback_url);

        var finalURL = results[2].replace("%ROOM_URL%", encodedRoom).
        replace("%APP_ID%", app_id).
        replace("%REDIRECT_URI%", redirect_url);

        return loop.request("OpenURL", finalURL);}).
      then(function () {
        loop.request("NotifyUITour", "Loop:RoomURLShared");});


      var from = actionData.from;
      var bucket = this._constants.SHARING_ROOM_URL["FACEBOOK_FROM_" + from.toUpperCase()];
      if (typeof bucket === "undefined") {
        console.error("No URL sharing type bucket found for '" + from + "'");
        return;}

      loop.requestMulti(
      ["TelemetryAddValue", "LOOP_SHARING_ROOM_URL", bucket], 
      ["TelemetryAddValue", "LOOP_ACTIVITY_COUNTER", this._constants.LOOP_MAU_TYPE.ROOM_SHARE]);}, 



    /**
     * Share a room url.
     *
     * @param  {sharedActions.ShareRoomUrl} actionData The action data.
     */
    shareRoomUrl: function shareRoomUrl(actionData) {
      var providerOrigin = new URL(actionData.provider.origin).hostname;
      var shareTitle = "";
      var shareBody = null;

      switch (providerOrigin) {
        case "mail.google.com":
          shareTitle = mozL10n.get("share_email_subject7");
          shareBody = mozL10n.get("share_email_body7", { 
            callUrl: actionData.roomUrl });

          shareBody += mozL10n.get("share_email_footer2");
          break;
        case "twitter.com":
        default:
          shareTitle = mozL10n.get("share_tweet", { 
            clientShortname2: mozL10n.get("clientShortname2") });

          break;}


      loop.requestMulti(
      ["SocialShareRoom", actionData.provider.origin, actionData.roomUrl, 
      shareTitle, shareBody], 
      ["NotifyUITour", "Loop:RoomURLShared"]);}, 


    /**
     * Open the share panel to add a Social share provider.
     */
    addSocialShareProvider: function addSocialShareProvider() {
      loop.request("AddSocialShareProvider");}, 


    /**
     * Creates a new room.
     *
     * @param {sharedActions.DeleteRoom} actionData The action data.
     */
    deleteRoom: function deleteRoom(actionData) {
      loop.request("Rooms:Delete", actionData.roomToken).then(function (result) {
        var isError = result && result.isError;
        if (isError) {
          this.dispatchAction(new sharedActions.DeleteRoomError({ error: result }));}

        var buckets = this._constants.ROOM_DELETE;
        loop.requestMulti(
        ["TelemetryAddValue", "LOOP_ROOM_DELETE", buckets[isError ? 
        "DELETE_FAIL" : "DELETE_SUCCESS"]], 
        ["TelemetryAddValue", "LOOP_ACTIVITY_COUNTER", this._constants.LOOP_MAU_TYPE.ROOM_DELETE]);}.

      bind(this));}, 


    /**
     * Executed when a room deletion error occurs.
     *
     * @param {sharedActions.DeleteRoomError} actionData The action data.
     */
    deleteRoomError: function deleteRoomError(actionData) {
      this.setStoreState({ error: actionData.error });}, 


    /**
     * Gather the list of all available rooms from the Loop API.
     */
    getAllRooms: function getAllRooms() {
      // XXX Ideally, we'd have a specific command to "start up" the room store
      // to get the rooms. We should address this alongside bug 1074665.
      if (this._gotAllRooms) {
        return;}


      loop.request("Rooms:GetAll", null).then(function (result) {
        var action;

        this.setStoreState({ pendingInitialRetrieval: false });

        if (result && result.isError) {
          action = new sharedActions.GetAllRoomsError({ error: result });} else 
        {
          action = new sharedActions.UpdateRoomList({ roomList: result });}


        this.dispatchAction(action);

        this._gotAllRooms = true;

        // We can only start listening to room events after getAll() has been
        // called executed first.
        this.startListeningToRoomEvents();}.
      bind(this));}, 


    /**
     * Updates current error state in case getAllRooms failed.
     *
     * @param {sharedActions.GetAllRoomsError} actionData The action data.
     */
    getAllRoomsError: function getAllRoomsError(actionData) {
      this.setStoreState({ error: actionData.error });}, 


    /**
     * Updates current room list.
     *
     * @param {sharedActions.UpdateRoomList} actionData The action data.
     */
    updateRoomList: function updateRoomList(actionData) {
      this.setStoreState({ 
        error: undefined, 
        rooms: this._processRoomList(actionData.roomList) });}, 



    /**
     * Opens a room
     *
     * @param {sharedActions.OpenRoom} actionData The action data.
     */
    openRoom: function openRoom(actionData) {
      loop.requestMulti(
      ["Rooms:Open", actionData.roomToken], 
      ["TelemetryAddValue", "LOOP_ACTIVITY_COUNTER", this._constants.LOOP_MAU_TYPE.ROOM_OPEN]);}, 



    /**
     * Updates the context data attached to a room.
     *
     * @param {sharedActions.UpdateRoomContext} actionData
     */
    updateRoomContext: function updateRoomContext(actionData) {
      this.setStoreState({ savingContext: true });
      loop.request("Rooms:Get", actionData.roomToken).then(function (result) {
        if (result.isError) {
          this.dispatchAction(new sharedActions.UpdateRoomContextError({ 
            error: result }));

          return;}


        var roomData = {};
        var context = result.decryptedContext;
        var oldRoomName = context.roomName;
        var newRoomName = (actionData.newRoomName || "").trim();
        if (newRoomName && oldRoomName !== newRoomName) {
          roomData.roomName = newRoomName;}

        var oldRoomURLs = context.urls;
        var oldRoomURL = oldRoomURLs && oldRoomURLs[0];
        // Since we want to prevent storing falsy (i.e. empty) values for context
        // data, there's no need to send that to the server as an update.
        var newRoomURL = loop.shared.utils.stripFalsyValues({ 
          location: (actionData.newRoomURL || "").trim(), 
          thumbnail: (actionData.newRoomThumbnail || "").trim(), 
          description: (actionData.newRoomDescription || "").trim() });

        // Only attach a context to the room when
        // 1) there was already a URL set,
        // 2) a new URL is provided as of now,
        // 3) the URL data has changed.
        var diff = loop.shared.utils.objectDiff(oldRoomURL, newRoomURL);
        if (diff.added.length || diff.updated.length) {
          newRoomURL = _.extend(oldRoomURL || {}, newRoomURL);
          var isValidURL = false;
          try {
            isValidURL = new URL(newRoomURL.location);} 
          catch (ex) {
            // URL may throw, default to false;
          }
          if (isValidURL) {
            roomData.urls = [newRoomURL];}}


        // TODO: there currently is no clear UX defined on what to do when all
        // context data was cleared, e.g. when diff.removed contains all the
        // context properties. Until then, we can't deal with context removal here.

        // When no properties have been set on the roomData object, there's nothing
        // to save.
        if (!Object.getOwnPropertyNames(roomData).length) {
          // Ensure async actions so that we get separate setStoreState events
          // that React components won't miss.
          setTimeout(function () {
            this.dispatchAction(new sharedActions.UpdateRoomContextDone());}.
          bind(this), 0);
          return;}


        this.setStoreState({ error: null });
        loop.request("Rooms:Update", actionData.roomToken, roomData).then(function (result2) {
          var isError = result2 && result2.isError;
          var action = isError ? 
          new sharedActions.UpdateRoomContextError({ error: result2 }) : 
          new sharedActions.UpdateRoomContextDone();
          this.dispatchAction(action);}.
        bind(this));}.
      bind(this));}, 


    /**
     * Handles the updateRoomContextDone action.
     */
    updateRoomContextDone: function updateRoomContextDone() {
      this.setStoreState({ savingContext: false });}, 


    /**
     * Updating the context data attached to a room error.
     *
     * @param {sharedActions.UpdateRoomContextError} actionData
     */
    updateRoomContextError: function updateRoomContextError(actionData) {
      this.setStoreState({ 
        error: actionData.error, 
        savingContext: false });} });})(



document.mozL10n || navigator.mozL10n);
