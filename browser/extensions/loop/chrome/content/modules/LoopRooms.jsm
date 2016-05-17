/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";var _slicedToArray = function () {function sliceIterator(arr, i) {var _arr = [];var _n = true;var _d = false;var _e = undefined;try {for (var _i = arr[Symbol.iterator](), _s; !(_n = (_s = _i.next()).done); _n = true) {_arr.push(_s.value);if (i && _arr.length === i) break;}} catch (err) {_d = true;_e = err;} finally {try {if (!_n && _i["return"]) _i["return"]();} finally {if (_d) throw _e;}}return _arr;}return function (arr, i) {if (Array.isArray(arr)) {return arr;} else if (Symbol.iterator in Object(arr)) {return sliceIterator(arr, i);} else {throw new TypeError("Invalid attempt to destructure non-iterable instance");}};}();function _toConsumableArray(arr) {if (Array.isArray(arr)) {for (var i = 0, arr2 = Array(arr.length); i < arr.length; i++) {arr2[i] = arr[i];}return arr2;} else {return Array.from(arr);}}var _Components = 

Components;var Cu = _Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Timer.jsm");var _Cu$import = 

Cu.import("chrome://loop/content/modules/MozLoopService.jsm", {});var MozLoopService = _Cu$import.MozLoopService;var LOOP_SESSION_TYPE = _Cu$import.LOOP_SESSION_TYPE;
XPCOMUtils.defineLazyModuleGetter(this, "Promise", 
"resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CommonUtils", 
"resource://services-common/utils.js");
XPCOMUtils.defineLazyModuleGetter(this, "WebChannel", 
"resource://gre/modules/WebChannel.jsm");

XPCOMUtils.defineLazyGetter(this, "eventEmitter", function () {var _Cu$import2 = 
  Cu.import("resource://devtools/shared/event-emitter.js", {});var EventEmitter = _Cu$import2.EventEmitter;
  return new EventEmitter();});


XPCOMUtils.defineLazyModuleGetter(this, "DomainWhitelist", 
"chrome://loop/content/modules/DomainWhitelist.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LoopRoomsCache", 
"chrome://loop/content/modules/LoopRoomsCache.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "loopUtils", 
"chrome://loop/content/modules/utils.js", "utils");
XPCOMUtils.defineLazyModuleGetter(this, "loopCrypto", 
"chrome://loop/content/shared/js/crypto.js", "LoopCrypto");
XPCOMUtils.defineLazyModuleGetter(this, "ObjectUtils", 
"resource://gre/modules/ObjectUtils.jsm");

/* exported LoopRooms, roomsPushNotification */

this.EXPORTED_SYMBOLS = ["LoopRooms", "roomsPushNotification"];

// The maximum number of clients that we support currently.
var CLIENT_MAX_SIZE = 2;

// Wait at least 5 seconds before doing opportunistic encryption.
var MIN_TIME_BEFORE_ENCRYPTION = 5 * 1000;
// Wait at maximum of 30 minutes before doing opportunistic encryption.
var MAX_TIME_BEFORE_ENCRYPTION = 30 * 60 * 1000;
// Wait time between individual re-encryption cycles (1 second).
var TIME_BETWEEN_ENCRYPTIONS = 1000;

// This is the pref name for the url of the standalone pages.
var LINKCLICKER_URL_PREFNAME = "loop.linkClicker.url";

var roomsPushNotification = function roomsPushNotification(version, channelID) {
  return LoopRoomsInternal.onNotification(version, channelID);};


// Since the LoopRoomsInternal.rooms map as defined below is a local cache of
// room objects that are retrieved from the server, this is list may become out
// of date. The Push server may notify us of this event, which will set the global
// 'dirty' flag to TRUE.
var gDirty = true;
// Global variable that keeps track of the currently used account.
var gCurrentUser = null;
// Global variable that keeps track of the room cache.
var gRoomsCache = null;
// Global variable that keeps track of the link clicker channel.
var gLinkClickerChannel = null;
// Global variable that keeps track of in-progress getAll requests.
var gGetAllPromise = null;

/**
 * Extend a `target` object with the properties defined in `source`.
 *
 * @param {Object} target The target object to receive properties defined in `source`
 * @param {Object} source The source object to copy properties from
 */
var extend = function extend(target, source) {var _iteratorNormalCompletion = true;var _didIteratorError = false;var _iteratorError = undefined;try {
    for (var _iterator = Object.getOwnPropertyNames(source)[Symbol.iterator](), _step; !(_iteratorNormalCompletion = (_step = _iterator.next()).done); _iteratorNormalCompletion = true) {var key = _step.value;
      target[key] = source[key];}} catch (err) {_didIteratorError = true;_iteratorError = err;} finally {try {if (!_iteratorNormalCompletion && _iterator.return) {_iterator.return();}} finally {if (_didIteratorError) {throw _iteratorError;}}}

  return target;};


/**
 * Checks whether a participant is already part of a room.
 *
 * @see https://wiki.mozilla.org/Loop/Architecture/Rooms#User_Identification_in_a_Room
 *
 * @param {Object} room        A room object that contains a list of current participants
 * @param {Object} participant Participant to check if it's already there
 * @returns {Boolean} TRUE when the participant is already a member of the room,
 *                    FALSE when it's not.
 */
var containsParticipant = function containsParticipant(room, participant) {var _iteratorNormalCompletion2 = true;var _didIteratorError2 = false;var _iteratorError2 = undefined;try {
    for (var _iterator2 = room.participants[Symbol.iterator](), _step2; !(_iteratorNormalCompletion2 = (_step2 = _iterator2.next()).done); _iteratorNormalCompletion2 = true) {var user = _step2.value;
      if (user.roomConnectionId == participant.roomConnectionId) {
        return true;}}} catch (err) {_didIteratorError2 = true;_iteratorError2 = err;} finally {try {if (!_iteratorNormalCompletion2 && _iterator2.return) {_iterator2.return();}} finally {if (_didIteratorError2) {throw _iteratorError2;}}}


  return false;};


/**
 * Compares the list of participants of the room currently in the cache and an
 * updated version of that room. When a new participant is found, the 'joined'
 * event is emitted. When a participant is not found in the update, it emits a
 * 'left' event.
 *
 * @param {Object} room        A room object to compare the participants list
 *                             against
 * @param {Object} updatedRoom A room object that contains the most up-to-date
 *                             list of participants
 */
var checkForParticipantsUpdate = function checkForParticipantsUpdate(room, updatedRoom) {
  // Partially fetched rooms don't contain the participants list yet. Skip the
  // check for now.
  if (!("participants" in room)) {
    return;}


  var participant = void 0;
  // Check for participants that joined.
  var _iteratorNormalCompletion3 = true;var _didIteratorError3 = false;var _iteratorError3 = undefined;try {for (var _iterator3 = updatedRoom.participants[Symbol.iterator](), _step3; !(_iteratorNormalCompletion3 = (_step3 = _iterator3.next()).done); _iteratorNormalCompletion3 = true) {participant = _step3.value;
      if (!containsParticipant(room, participant)) {
        eventEmitter.emit("joined", room, participant);
        eventEmitter.emit("joined:" + room.roomToken, participant);}}



    // Check for participants that left.
  } catch (err) {_didIteratorError3 = true;_iteratorError3 = err;} finally {try {if (!_iteratorNormalCompletion3 && _iterator3.return) {_iterator3.return();}} finally {if (_didIteratorError3) {throw _iteratorError3;}}}var _iteratorNormalCompletion4 = true;var _didIteratorError4 = false;var _iteratorError4 = undefined;try {for (var _iterator4 = room.participants[Symbol.iterator](), _step4; !(_iteratorNormalCompletion4 = (_step4 = _iterator4.next()).done); _iteratorNormalCompletion4 = true) {participant = _step4.value;
      if (!containsParticipant(updatedRoom, participant)) {
        eventEmitter.emit("left", room, participant);
        eventEmitter.emit("left:" + room.roomToken, participant);}}} catch (err) {_didIteratorError4 = true;_iteratorError4 = err;} finally {try {if (!_iteratorNormalCompletion4 && _iterator4.return) {_iterator4.return();}} finally {if (_didIteratorError4) {throw _iteratorError4;}}}};




/**
 * These are wrappers which can be overriden by tests to allow us to manually
 * handle the timeouts.
 */
var timerHandlers = { 
  /**
   * Wrapper for setTimeout.
   *
   * @param  {Function} callback The callback function.
   * @param  {Number}   delay    The delay in milliseconds.
   * @return {Number}            The timer identifier.
   */
  startTimer: function startTimer(callback, delay) {
    return setTimeout(callback, delay);} };



/**
 * The Rooms class.
 *
 * Each method that is a member of this class requires the last argument to be a
 * callback Function. MozLoopAPI will cause things to break if this invariant is
 * violated. You'll notice this as well in the documentation for each method.
 */
var LoopRoomsInternal = { 
  /**
   * @var {Map} rooms Collection of rooms currently in cache.
   */
  rooms: new Map(), 

  get roomsCache() {
    if (!gRoomsCache) {
      gRoomsCache = new LoopRoomsCache();}

    return gRoomsCache;}, 


  /**
   * @var {Object} encryptionQueue  This stores the list of rooms awaiting
   *                                encryption and associated timers.
   */
  encryptionQueue: { 
    queue: [], 
    timer: null, 
    reset: function reset() {
      this.queue = [];
      this.timer = null;} }, 



  /**
   * Initialises the rooms, sets up the link clicker listener.
   */
  init: function init() {
    Services.prefs.addObserver(LINKCLICKER_URL_PREFNAME, 
    this.setupLinkClickerListener.bind(this), false);

    this.setupLinkClickerListener();}, 


  /**
   * Sets up a WebChannel listener for the link clicker so that we can open
   * rooms in the Firefox UI.
   */
  setupLinkClickerListener: function setupLinkClickerListener() {
    // Ensure any existing channel is tidied up.
    if (gLinkClickerChannel) {
      gLinkClickerChannel.stopListening();
      gLinkClickerChannel = null;}


    var linkClickerUrl = Services.prefs.getCharPref(LINKCLICKER_URL_PREFNAME);

    // Don't do anything if there's no url.
    if (!linkClickerUrl) {
      return;}


    var uri = Services.io.newURI(linkClickerUrl, null, null);

    gLinkClickerChannel = new WebChannel("loop-link-clicker", uri);
    gLinkClickerChannel.listen(this._handleLinkClickerMessage.bind(this));}, 


  /**
   * @var {String} sessionType The type of user session. May be 'FXA' or 'GUEST'.
   */
  get sessionType() {
    return MozLoopService.userProfile ? LOOP_SESSION_TYPE.FXA : 
    LOOP_SESSION_TYPE.GUEST;}, 


  /**
   * @var {Number} participantsCount The total amount of participants currently
   *                                 inside all rooms.
   */
  get participantsCount() {
    var count = 0;var _iteratorNormalCompletion5 = true;var _didIteratorError5 = false;var _iteratorError5 = undefined;try {
      for (var _iterator5 = this.rooms.values()[Symbol.iterator](), _step5; !(_iteratorNormalCompletion5 = (_step5 = _iterator5.next()).done); _iteratorNormalCompletion5 = true) {var room = _step5.value;
        if (room.deleted || !("participants" in room)) {
          continue;}

        count += room.participants.length;}} catch (err) {_didIteratorError5 = true;_iteratorError5 = err;} finally {try {if (!_iteratorNormalCompletion5 && _iterator5.return) {_iterator5.return();}} finally {if (_didIteratorError5) {throw _iteratorError5;}}}

    return count;}, 


  /**
   * Processes the encryption queue. Takes the next item off the queue,
   * restarts the timer if necessary.
   *
   * Although this is only called from a timer callback, it is an async function
   * so that tests can call it and be deterministic.
   */
  processEncryptionQueue: Task.async(function* () {
    var roomToken = this.encryptionQueue.queue.shift();

    // Performed in sync fashion so that we don't queue a timer until it has
    // completed, and to make it easier to run tests.
    var roomData = this.rooms.get(roomToken);

    if (roomData) {
      try {
        // Passing the empty object for roomData is enough for the room to be
        // re-encrypted.
        yield LoopRooms.promise("update", roomToken, {});} 
      catch (error) {
        MozLoopService.log.error("Upgrade encryption of room failed", error);
        // No need to remove the room from the list as that's done in the shift above.
      }}


    if (this.encryptionQueue.queue.length) {
      this.encryptionQueue.timer = 
      timerHandlers.startTimer(this.processEncryptionQueue.bind(this), TIME_BETWEEN_ENCRYPTIONS);} else 
    {
      this.encryptionQueue.timer = null;}}), 



  /**
   * Queues a room for encryption sometime in the future. This is done so as
   * not to overload the server or the browser when we initially request the
   * list of rooms.
   *
   * @param {String} roomToken The token for the room that needs encrypting.
   */
  queueForEncryption: function queueForEncryption(roomToken) {
    if (this.encryptionQueue.queue.indexOf(roomToken) == -1) {
      this.encryptionQueue.queue.push(roomToken);}


    // Set up encryption to happen at a random time later. There's a minimum
    // wait time - we don't need to do this straight away, so no need if the user
    // is starting up. We then add a random factor on top of that. This is to
    // try and avoid any potential with a set of clients being restarted at the
    // same time and flooding the server.
    if (!this.encryptionQueue.timer) {
      var waitTime = (MAX_TIME_BEFORE_ENCRYPTION - MIN_TIME_BEFORE_ENCRYPTION) * 
      Math.random() + MIN_TIME_BEFORE_ENCRYPTION;
      this.encryptionQueue.timer = 
      timerHandlers.startTimer(this.processEncryptionQueue.bind(this), waitTime);}}, 



  /**
   * Gets or creates a room key for a room.
   *
   * It assumes that the room data is decrypted.
   *
   * @param {Object} roomData The roomData to get the key for.
   * @return {Promise} A promise that is resolved whith the room key.
   */
  promiseGetOrCreateRoomKey: Task.async(function* (roomData) {
    if (roomData.roomKey) {
      return roomData.roomKey;}


    return yield loopCrypto.generateKey();}), 


  /**
   * Encrypts a room key for sending to the server using the profile encryption
   * key.
   *
   * @param {String} key The JSON web key to encrypt.
   * @return {Promise} A promise that is resolved with the encrypted room key.
   */
  promiseEncryptedRoomKey: Task.async(function* (key) {
    var profileKey = yield MozLoopService.promiseProfileEncryptionKey();

    var encryptedRoomKey = yield loopCrypto.encryptBytes(profileKey, key);
    return encryptedRoomKey;}), 


  /**
   * Decryptes a room key from the server using the profile encryption key.
   *
   * @param  {String} encryptedKey The room key to decrypt.
   * @return {Promise} A promise that is resolved with the decrypted room key.
   */
  promiseDecryptRoomKey: Task.async(function* (encryptedKey) {
    var profileKey = yield MozLoopService.promiseProfileEncryptionKey();

    var decryptedRoomKey = yield loopCrypto.decryptBytes(profileKey, encryptedKey);
    return decryptedRoomKey;}), 


  /**
   * Updates a roomUrl to add a new key onto the end of the url.
   *
   * @param  {String} roomUrl The existing url that may or may not have a key
   *                          on the end.
   * @param  {String} roomKey The new key to put on the url.
   * @return {String}         The revised url.
   */
  refreshRoomUrlWithNewKey: function refreshRoomUrlWithNewKey(roomUrl, roomKey) {
    // Strip any existing key from the url.
    roomUrl = roomUrl.split("#")[0];
    // Now add the key to the url.
    return roomUrl + "#" + roomKey;}, 


  /**
   * Encrypts room data in a format appropriate to sending to the loop
   * server.
   *
   * @param  {Object} roomData The room data to encrypt.
   * @return {Promise} A promise that is resolved with an object containing
   *                   two objects:
   *                   - encrypted: The encrypted data to send. This excludes
   *                                any decrypted data.
   *                   - all: The roomData with both encrypted and decrypted
   *                          information.
   *                   If rejected, encryption has failed. This could be due to
   *                   missing keys for FxA, which this process can't manage. It
   *                   is generally expected the panel will prompt the user for
   *                   re-auth if the FxA keys are missing.
   */
  promiseEncryptRoomData: Task.async(function* (roomData) {
    var newRoomData = extend({}, roomData);

    if (!newRoomData.context) {
      newRoomData.context = {};}


    // First get the room key.
    var key = yield this.promiseGetOrCreateRoomKey(newRoomData);

    newRoomData.context.wrappedKey = yield this.promiseEncryptedRoomKey(key);

    // Now encrypt the actual data.
    newRoomData.context.value = yield loopCrypto.encryptBytes(key, 
    JSON.stringify(newRoomData.decryptedContext));

    // The algorithm is currently hard-coded as AES-GCM, in case of future
    // changes.
    newRoomData.context.alg = "AES-GCM";
    newRoomData.roomKey = key;

    var serverRoomData = extend({}, newRoomData);

    // We must not send these items to the server.
    delete serverRoomData.decryptedContext;
    delete serverRoomData.roomKey;

    return { 
      encrypted: serverRoomData, 
      all: newRoomData };}), 



  /**
   * Decrypts room data recevied from the server.
   *
   * @param  {Object} roomData The roomData with encrypted context.
   * @return {Promise} A promise that is resolved with the decrypted room data.
   */
  promiseDecryptRoomData: Task.async(function* (roomData) {
    if (!roomData.context) {
      return roomData;}


    if (!roomData.context.wrappedKey) {
      throw new Error("Missing wrappedKey");}


    var savedRoomKey = yield this.roomsCache.getKey(this.sessionType, roomData.roomToken);
    var fallback = false;
    var key = void 0;

    try {
      key = yield this.promiseDecryptRoomKey(roomData.context.wrappedKey);} 
    catch (error) {
      // If we don't have a key saved, then we can't do anything.
      if (!savedRoomKey) {
        throw error;}


      // We failed to decrypt the room key, so has our FxA key changed?
      // If so, we fall-back to the saved room key.
      key = savedRoomKey;
      fallback = true;}


    var decryptedData = yield loopCrypto.decryptBytes(key, roomData.context.value);

    if (fallback) {
      // Fallback decryption succeeded, so we need to re-encrypt the room key and
      // save the data back again.
      MozLoopService.log.debug("Fell back to saved key, queuing for encryption", 
      roomData.roomToken);
      this.queueForEncryption(roomData.roomToken);} else 
    if (!savedRoomKey || key != savedRoomKey) {
      // Decryption succeeded, but we don't have the right key saved.
      try {
        yield this.roomsCache.setKey(this.sessionType, roomData.roomToken, key);} 

      catch (error) {
        MozLoopService.log.error("Failed to save room key:", error);}}



    roomData.roomKey = key;
    roomData.decryptedContext = JSON.parse(decryptedData);

    roomData.roomUrl = this.refreshRoomUrlWithNewKey(roomData.roomUrl, roomData.roomKey);

    return roomData;}), 


  /**
   * Saves room information and notifies updates to any listeners.
   *
   * @param {Object}  roomData The new room data to save.
   * @param {Boolean} isUpdate true if this is an update, false if its an add.
   */
  saveAndNotifyUpdate: function saveAndNotifyUpdate(roomData, isUpdate) {
    this.rooms.set(roomData.roomToken, roomData);

    var eventName = isUpdate ? "update" : "add";
    eventEmitter.emit(eventName, roomData);
    eventEmitter.emit(eventName + ":" + roomData.roomToken, roomData);}, 


  /**
   * Either adds or updates the room to the room store and notifies to any
   * listeners.
   *
   * This will decrypt information if necessary, and adjust for the legacy
   * "roomName" field.
   *
   * @param {Object}  room     The new room to add.
   * @param {Boolean} isUpdate true if this is an update to an existing room.
   */
  addOrUpdateRoom: Task.async(function* (room, isUpdate) {
    if (!room.context) {
      // We don't do anything with roomUrl here as it doesn't need a key
      // string adding at this stage.

      // No encrypted data yet, use the old roomName field.
      room.decryptedContext = { 
        roomName: room.roomName };

      delete room.roomName;

      // This room doesn't have context, so we'll save it for a later encryption
      // cycle.
      this.queueForEncryption(room.roomToken);

      this.saveAndNotifyUpdate(room, isUpdate);} else 
    {
      // We could potentially optimise this later by not decrypting if the
      // encrypted context hasn't already changed. However perf doesn't seem
      // to be too bigger an issue at the moment, so we just decrypt for now.
      // If we do change this, then we need to make sure we get the new room
      // data setup properly, as happens at the end of promiseDecryptRoomData.
      try {
        var roomData = yield this.promiseDecryptRoomData(room);

        this.saveAndNotifyUpdate(roomData, isUpdate);} 
      catch (error) {
        MozLoopService.log.error("Failed to decrypt room data: ", error);
        // Do what we can to save the room data.
        room.decryptedContext = {};
        this.saveAndNotifyUpdate(room, isUpdate);}}}), 




  /**
   * Fetch a list of rooms that the currently registered user is a member of.
   *
   * @param {String}   [version] If set, we will fetch a list of changed rooms since
   *                             `version`. Optional.
   * @return {Promise}           A promise that is resolved with a list of rooms
   *                             on success, or rejected with an error if it fails.
   */
  getAll: function getAll(version) {
    if (gGetAllPromise && !version) {
      return gGetAllPromise;}


    if (!gDirty) {
      return Promise.resolve([].concat(_toConsumableArray(this.rooms.values())));}


    gGetAllPromise = Task.spawn(function* () {
      // Fetch the rooms from the server.
      var url = "/rooms" + (version ? "?version=" + encodeURIComponent(version) : "");
      var response = yield MozLoopService.hawkRequest(this.sessionType, url, "GET");
      var roomsList = JSON.parse(response.body);
      if (!Array.isArray(roomsList)) {
        throw new Error("Missing array of rooms in response.");}var _iteratorNormalCompletion6 = true;var _didIteratorError6 = false;var _iteratorError6 = undefined;try {


        for (var _iterator6 = roomsList[Symbol.iterator](), _step6; !(_iteratorNormalCompletion6 = (_step6 = _iterator6.next()).done); _iteratorNormalCompletion6 = true) {var room = _step6.value;
          // See if we already have this room in our cache.
          var orig = this.rooms.get(room.roomToken);

          if (room.deleted) {
            // If this client deleted the room, then we'll already have
            // deleted the room in the function below.
            if (orig) {
              this.rooms.delete(room.roomToken);}


            eventEmitter.emit("delete", room);
            eventEmitter.emit("delete:" + room.roomToken, room);} else 
          {
            yield this.addOrUpdateRoom(room, !!orig);

            if (orig) {
              checkForParticipantsUpdate(orig, room);}}}




        // If there's no rooms in the list, remove the guest created room flag, so that
        // we don't keep registering for guest when we don't need to.
      } catch (err) {_didIteratorError6 = true;_iteratorError6 = err;} finally {try {if (!_iteratorNormalCompletion6 && _iterator6.return) {_iterator6.return();}} finally {if (_didIteratorError6) {throw _iteratorError6;}}}if (this.sessionType == LOOP_SESSION_TYPE.GUEST && !this.rooms.size) {
        this.setGuestCreatedRoom(false);}


      // Set the 'dirty' flag back to FALSE, since the list is as fresh as can be now.
      gDirty = false;
      gGetAllPromise = null;
      return [].concat(_toConsumableArray(this.rooms.values()));}.
    bind(this)).catch(function (error) {
      gGetAllPromise = null;
      // Re-throw error so callers get notified.
      throw error;});


    return gGetAllPromise;}, 


  /**
   * Request information about number of participants joined to a specific room from the server.
   * Used to determine infobar message state on the number of participants in the room.
   *
   * @param {String}  roomToken Room identifier
   * @return {Number} Count of participants in the room.
   */
  getNumParticipants: function getNumParticipants(roomToken) {
    try {
      if (this.rooms && this.rooms.has(roomToken)) {
        return this.rooms.get(roomToken).participants.length;}

      return 0;} 

    catch (ex) {
      // No room, log error and send back 0 to indicate none in room.
      MozLoopService.log.error("No room found in current session: ", ex);
      return 0;}}, 



  /**
   * Request information about a specific room from the server. It will be
   * returned from the cache if it's already in it.
   *
   * @param {String}   roomToken Room identifier
   * @param {Function} callback  Function that will be invoked once the operation
   *                             finished. The first argument passed will be an
   *                             `Error` object or `null`. The second argument will
   *                             be the list of rooms, if it was fetched successfully.
   */
  get: function get(roomToken, callback) {
    var room = this.rooms.has(roomToken) ? this.rooms.get(roomToken) : {};
    // Check if we need to make a request to the server to collect more room data.
    var needsUpdate = !("participants" in room);
    if (!gDirty && !needsUpdate) {
      // Dirty flag is not set AND the necessary data is available, so we can
      // simply return the room.
      callback(null, room);
      return;}


    Task.spawn(function* () {
      var response = yield MozLoopService.hawkRequest(this.sessionType, 
      "/rooms/" + encodeURIComponent(roomToken), "GET");

      var data = JSON.parse(response.body);

      room.roomToken = roomToken;

      if (data.deleted) {
        this.rooms.delete(room.roomToken);

        extend(room, data);
        eventEmitter.emit("delete", room);
        eventEmitter.emit("delete:" + room.roomToken, room);} else 
      {
        yield this.addOrUpdateRoom(data, !needsUpdate);

        checkForParticipantsUpdate(room, data);}

      callback(null, room);}.
    bind(this)).catch(callback);}, 


  /**
   * Create a room.
   *
   * @param {Object}   room     Properties to be sent to the LoopServer
   * @param {Function} callback Function that will be invoked once the operation
   *                            finished. The first argument passed will be an
   *                            `Error` object or `null`. The second argument will
   *                            be the room, if it was created successfully.
   */
  create: function create(room, callback) {
    if (!("decryptedContext" in room) || !("maxSize" in room)) {
      callback(new Error("Missing required property to create a room"));
      return;}


    if (!("roomOwner" in room)) {
      room.roomOwner = "-";}


    Task.spawn(function* () {var _ref = 
      yield this.promiseEncryptRoomData(room);var all = _ref.all;var encrypted = _ref.encrypted;

      // Save both sets of data...
      room = all;
      // ...but only send the encrypted data.
      var response = yield MozLoopService.hawkRequest(this.sessionType, "/rooms", 
      "POST", encrypted);

      extend(room, JSON.parse(response.body));
      // Do not keep this value - it is a request to the server.
      delete room.expiresIn;
      // Make sure the url has the key on it.
      room.roomUrl = this.refreshRoomUrlWithNewKey(room.roomUrl, room.roomKey);
      this.rooms.set(room.roomToken, room);

      if (this.sessionType == LOOP_SESSION_TYPE.GUEST) {
        this.setGuestCreatedRoom(true);}


      // Now we've got the room token, we can save the key to disk.
      yield this.roomsCache.setKey(this.sessionType, room.roomToken, room.roomKey);

      eventEmitter.emit("add", room);
      callback(null, room);}.
    bind(this)).catch(callback);}, 


  /**
   * Sets whether or not the user has created a room in guest mode.
   *
   * @param {Boolean} created If the user has created the room.
   */
  setGuestCreatedRoom: function setGuestCreatedRoom(created) {
    if (created) {
      Services.prefs.setBoolPref("loop.createdRoom", created);} else 
    {
      Services.prefs.clearUserPref("loop.createdRoom");}}, 



  /**
   * Returns true if the user has a created room in guest mode.
   */
  getGuestCreatedRoom: function getGuestCreatedRoom() {
    try {
      return Services.prefs.getBoolPref("loop.createdRoom");} 
    catch (x) {
      return false;}}, 



  open: function open(roomToken) {
    var windowData = { 
      roomToken: roomToken, 
      type: "room" };


    eventEmitter.emit("open", roomToken);

    return MozLoopService.openChatWindow(windowData, function () {
      eventEmitter.emit("close");});}, 



  /**
   * Deletes a room.
   *
   * @param {String}   roomToken The room token.
   * @param {Function} callback  Function that will be invoked once the operation
   *                             finished. The first argument passed will be an
   *                             `Error` object or `null`.
   */
  delete: function _delete(roomToken, callback) {var _this = this;
    // XXX bug 1092954: Before deleting a room, the client should check room
    //     membership and forceDisconnect() all current participants.
    var room = this.rooms.get(roomToken);
    var url = "/rooms/" + encodeURIComponent(roomToken);
    MozLoopService.hawkRequest(this.sessionType, url, "DELETE").
    then(function () {
      _this.rooms.delete(roomToken);
      eventEmitter.emit("delete", room);
      eventEmitter.emit("delete:" + room.roomToken, room);
      callback(null, room);}, 
    function (error) {return callback(error);}).catch(function (error) {return callback(error);});}, 


  /**
   * Internal function to handle POSTs to a room.
   *
   * @param {String} roomToken  The room token.
   * @param {Object} postData   The data to post to the room.
   * @param {Function} callback Function that will be invoked once the operation
   *                            finished. The first argument passed will be an
   *                            `Error` object or `null`.
   */
  _postToRoom: function _postToRoom(roomToken, postData, callback) {var _this2 = this;
    var url = "/rooms/" + encodeURIComponent(roomToken);
    MozLoopService.hawkRequest(this.sessionType, url, "POST", postData).then(function (response) {
      // Delete doesn't have a body return.
      var joinData = response.body ? JSON.parse(response.body) : {};
      if ("sessionToken" in joinData) {
        var room = _this2.rooms.get(roomToken);
        room.sessionToken = joinData.sessionToken;}

      callback(null, joinData);}, 
    function (error) {return callback(error);}).catch(function (error) {return callback(error);});}, 


  /**
   * Joins a room. The sessionToken that is returned by the server will be stored
   * locally, for future use.
   *
   * @param {String} roomToken   The room token.
   * @param {String} displayName The user's display name.
   * @param {Function} callback  Function that will be invoked once the operation
   *                             finished. The first argument passed will be an
   *                             `Error` object or `null`.
   */
  join: function join(roomToken, displayName, callback) {
    this._postToRoom(roomToken, { 
      action: "join", 
      displayName: displayName, 
      clientMaxSize: CLIENT_MAX_SIZE }, 
    callback);}, 


  /**
   * Refreshes a room
   *
   * @param {String} roomToken    The room token.
   * @param {String} sessionToken The session token for the session that has been
   *                              joined.
   * @param {Function} callback   Function that will be invoked once the operation
   *                              finished. The first argument passed will be an
   *                              `Error` object or `null`.
   */
  refreshMembership: function refreshMembership(roomToken, sessionToken, callback) {
    this._postToRoom(roomToken, { 
      action: "refresh", 
      sessionToken: sessionToken }, 
    callback);}, 


  /**
   * Leaves a room. Although this is an sync function, no data is returned
   * from the server.
   *
   * @param {String} roomToken    The room token.
   * @param {String} sessionToken The session token for the session that has been
   *                              joined
   * @param {Function} callback   Optional. Function that will be invoked once the operation
   *                              finished. The first argument passed will be an
   *                              `Error` object or `null`.
   */
  leave: function leave(roomToken, sessionToken, callback) {
    if (!callback) {
      callback = function callback(error) {
        if (error) {
          MozLoopService.log.error(error);}};}



    var room = this.rooms.get(roomToken);
    if (!sessionToken && room && room.sessionToken) {
      if (!room || !room.sessionToken) {
        return;}

      sessionToken = room.sessionToken;
      delete room.sessionToken;}

    this._postToRoom(roomToken, { 
      action: "leave", 
      sessionToken: sessionToken }, 
    callback);}, 


  /**
   * Forwards connection status to the server.
   *
   * @param {String}                  roomToken The room token.
   * @param {String}                  sessionToken The session token for the
   *                                               session that has been
   *                                               joined.
   * @param {sharedActions.SdkStatus} status The connection status.
   * @param {Function} callback Optional. Function that will be invoked once
   *                            the operation finished. The first argument
   *                            passed will be an `Error` object or `null`.
   */
  sendConnectionStatus: function sendConnectionStatus(roomToken, sessionToken, status, callback) {
    if (!callback) {
      callback = function callback(error) {
        if (error) {
          MozLoopService.log.error(error);}};}



    this._postToRoom(roomToken, { 
      action: "status", 
      event: status.event, 
      state: status.state, 
      connections: status.connections, 
      sendStreams: status.sendStreams, 
      recvStreams: status.recvStreams, 
      sessionToken: sessionToken }, 
    callback);}, 


  _domainLog: { 
    domainMap: new Map(), 
    roomToken: null }, 


  /**
   * Record a url associated to a room for batch submission if whitelisted.
   *
   * @param {String} roomToken The room token
   * @param {String} url       Url with the domain to record
   */
  _recordUrl: function _recordUrl(roomToken, url) {
    // Reset the log of domains if somehow we changed room tokens.
    if (this._domainLog.roomToken !== roomToken) {
      this._domainLog.roomToken = roomToken;
      this._domainLog.domainMap.clear();}


    var domain = void 0;
    try {
      domain = Services.eTLD.getBaseDomain(Services.io.newURI(url, null, null));} 

    catch (ex) {
      // Failed to extract domain, so don't record it.
      return;}


    // Only record domains that are whitelisted.
    if (!DomainWhitelist.check(domain)) {
      return;}


    // Increment the count for previously recorded domains.
    if (this._domainLog.domainMap.has(domain)) {
      this._domainLog.domainMap.get(domain).count++;}

    // Initialize the map for the domain with a value that can be submitted.
    else {
        this._domainLog.domainMap.set(domain, { count: 1, domain: domain });}}, 



  /**
   * Log the domains associated to a room token.
   *
   * @param {String} roomToken  The room token
   * @param {Function} callback Function that will be invoked once the operation
   *                            finished. The first argument passed will be an
   *                            `Error` object or `null`.
   */
  logDomains: function logDomains(roomToken, callback) {
    if (!callback) {
      callback = function callback(error) {
        if (error) {
          MozLoopService.log.error(error);}};}




    // Submit the domains that have been collected so far.
    if (this._domainLog.roomToken === roomToken && 
    this._domainLog.domainMap.size > 0) {
      this._postToRoom(roomToken, { 
        action: "logDomain", 
        domains: [].concat(_toConsumableArray(this._domainLog.domainMap.values())) }, 
      callback);
      this._domainLog.domainMap.clear();}

    // Indicate that nothing was logged.
    else {
        callback(null);}}, 



  /**
   * Updates a room.
   *
   * @param {String} roomToken  The room token
   * @param {Object} roomData   Updated context data for the room. The following
   *                            properties are expected: `roomName` and `urls`.
   *                            IMPORTANT: Data in the `roomData::urls` array
   *                            will be stored as-is, so any data omitted therein
   *                            will be gone forever.
   * @param {Function} callback Function that will be invoked once the operation
   *                            finished. The first argument passed will be an
   *                            `Error` object or `null`.
   */
  update: function update(roomToken, roomData, callback) {
    var room = this.rooms.get(roomToken);
    var url = "/rooms/" + encodeURIComponent(roomToken);
    if (!room.decryptedContext) {
      room.decryptedContext = { 
        roomName: roomData.roomName || room.roomName };} else 

    {
      // room.roomName is the final fallback as this is pre-encryption support.
      // Bug 1166283 is tracking the removal of the fallback.
      room.decryptedContext.roomName = roomData.roomName || 
      room.decryptedContext.roomName || 
      room.roomName;}

    if (roomData.urls && roomData.urls.length) {
      // For now we only support adding one URL to the room context.
      var context = roomData.urls[0];
      room.decryptedContext.urls = [context];

      // Record the url for reporting if enabled.
      if (Services.prefs.getBoolPref("loop.logDomains")) {
        this._recordUrl(roomToken, context.location);}}



    Task.spawn(function* () {var _ref2 = 
      yield this.promiseEncryptRoomData(room);var all = _ref2.all;var encrypted = _ref2.encrypted;

      // For patch, we only send the context data.
      var sendData = { 
        context: encrypted.context };


      // This might be an upgrade to encrypted rename, so store the key
      // just in case.
      yield this.roomsCache.setKey(this.sessionType, all.roomToken, all.roomKey);

      var response = yield MozLoopService.hawkRequest(this.sessionType, 
      url, "PATCH", sendData);

      var newRoomData = all;

      extend(newRoomData, JSON.parse(response.body));
      this.rooms.set(roomToken, newRoomData);
      callback(null, newRoomData);}.
    bind(this)).catch(callback);}, 


  /**
   * Callback used to indicate changes to rooms data on the LoopServer.
   *
   * @param {String} version   Version number assigned to this change set.
   * @param {String} channelID Notification channel identifier.
   */
  onNotification: function onNotification(version, channelID) {
    // See if we received a notification for the channel that's currently active:
    var channelIDs = MozLoopService.channelIDs;
    if (this.sessionType == LOOP_SESSION_TYPE.GUEST && channelID != channelIDs.roomsGuest || 
    this.sessionType == LOOP_SESSION_TYPE.FXA && channelID != channelIDs.roomsFxA) {
      return;}


    var oldDirty = gDirty;
    gDirty = true;

    // If we were already dirty, then get the full set of rooms. For example,
    // we'd already be dirty if we had started up but not got the list of rooms
    // yet.
    this.getAll(oldDirty ? null : version, function () {});}, 


  /**
   * When a user logs in or out, this method should be invoked to check whether
   * the rooms cache needs to be refreshed.
   *
   * @param {String|null} user The FxA userID or NULL
   */
  maybeRefresh: function maybeRefresh() {var user = arguments.length <= 0 || arguments[0] === undefined ? null : arguments[0];
    if (gCurrentUser == user) {
      return;}


    gCurrentUser = user;
    if (!gDirty) {
      gDirty = true;
      this.rooms.clear();
      eventEmitter.emit("refresh");
      this.getAll(null, function () {});}}, 



  /**
   * Handles a message received from the content channel.
   *
   * @param {String} id              The channel id.
   * @param {Object} message         The message received.
   * @param {Object} sendingContext  The context for the sending location.
   */
  _handleLinkClickerMessage: function _handleLinkClickerMessage(id, message, sendingContext) {
    if (!message) {
      return;}


    var sendResponse = function sendResponse(response, alreadyOpen) {
      gLinkClickerChannel.send({ 
        response: response, 
        alreadyOpen: alreadyOpen }, 
      sendingContext);};


    var hasRoom = this.rooms.has(message.roomToken);

    switch (message.command) {
      case "checkWillOpenRoom":
        sendResponse(hasRoom, false);
        break;
      case "openRoom":
        if (hasRoom) {
          if (MozLoopService.isChatWindowOpen(message.roomToken)) {
            sendResponse(hasRoom, true);} else 
          {
            this.open(message.roomToken);
            sendResponse(hasRoom, false);}} else 

        {
          sendResponse(hasRoom, false);}

        break;
      default:
        sendResponse(false, false);
        break;}} };



Object.freeze(LoopRoomsInternal);

/**
 * Public Loop Rooms API.
 *
 * LoopRooms implements the EventEmitter interface by exposing three methods -
 * `on`, `once` and `off` - to subscribe to events.
 * At this point the following events may be subscribed to:
 *  - 'add[:{room-id}]':    A new room object was successfully added to the data
 *                          store.
 *  - 'delete[:{room-id}]': A room was successfully removed from the data store.
 *  - 'update[:{room-id}]': A room object was successfully updated with changed
 *                          properties in the data store.
 *  - 'joined[:{room-id}]': A participant joined a room.
 *  - 'left[:{room-id}]':   A participant left a room.
 *
 * See the internal code for the API documentation.
 */
this.LoopRooms = { 
  init: function init() {
    LoopRoomsInternal.init();}, 


  get participantsCount() {
    return LoopRoomsInternal.participantsCount;}, 


  getAll: function getAll(version, callback) {
    if (!callback) {
      callback = version;
      version = null;}


    LoopRoomsInternal.getAll(version).then(function (result) {return callback(null, result);}).
    catch(function (error) {return callback(error);});}, 


  get: function get(roomToken, callback) {
    return LoopRoomsInternal.get(roomToken, callback);}, 


  create: function create(options, callback) {
    return LoopRoomsInternal.create(options, callback);}, 


  open: function open(roomToken) {
    return LoopRoomsInternal.open(roomToken);}, 


  delete: function _delete(roomToken, callback) {
    return LoopRoomsInternal.delete(roomToken, callback);}, 


  join: function join(roomToken, displayName, callback) {
    return LoopRoomsInternal.join(roomToken, displayName, callback);}, 


  refreshMembership: function refreshMembership(roomToken, sessionToken, callback) {
    return LoopRoomsInternal.refreshMembership(roomToken, sessionToken, 
    callback);}, 


  leave: function leave(roomToken, sessionToken, callback) {
    return LoopRoomsInternal.leave(roomToken, sessionToken, callback);}, 


  sendConnectionStatus: function sendConnectionStatus(roomToken, sessionToken, status, callback) {
    return LoopRoomsInternal.sendConnectionStatus(roomToken, sessionToken, status, callback);}, 


  logDomains: function logDomains(roomToken, callback) {
    return LoopRoomsInternal.logDomains(roomToken, callback);}, 


  update: function update(roomToken, roomData, callback) {
    return LoopRoomsInternal.update(roomToken, roomData, callback);}, 


  getGuestCreatedRoom: function getGuestCreatedRoom() {
    return LoopRoomsInternal.getGuestCreatedRoom();}, 


  maybeRefresh: function maybeRefresh(user) {
    return LoopRoomsInternal.maybeRefresh(user);}, 


  getNumParticipants: function getNumParticipants(roomToken) {
    return LoopRoomsInternal.getNumParticipants(roomToken);}, 


  /**
   * This method is only useful for unit tests to set the rooms cache to contain
   * a list of fake room data that can be asserted in tests.
   *
   * @param {Map} stub Stub cache containing fake rooms data
   */
  stubCache: function stubCache(stub) {
    LoopRoomsInternal.rooms.clear();
    if (stub) {
      // Fill up the rooms cache with room objects provided in the `stub` Map.
      var _iteratorNormalCompletion7 = true;var _didIteratorError7 = false;var _iteratorError7 = undefined;try {for (var _iterator7 = stub.entries()[Symbol.iterator](), _step7; !(_iteratorNormalCompletion7 = (_step7 = _iterator7.next()).done); _iteratorNormalCompletion7 = true) {var _ref3 = _step7.value;var _ref4 = _slicedToArray(_ref3, 2);var key = _ref4[0];var value = _ref4[1];
          LoopRoomsInternal.rooms.set(key, value);}} catch (err) {_didIteratorError7 = true;_iteratorError7 = err;} finally {try {if (!_iteratorNormalCompletion7 && _iterator7.return) {_iterator7.return();}} finally {if (_didIteratorError7) {throw _iteratorError7;}}}

      gDirty = false;} else 
    {
      // Restore the cache to not be stubbed anymore, but it'll need a refresh
      // from the server for sure.
      gDirty = true;}}, 



  promise: function promise(method) {var _this3 = this;for (var _len = arguments.length, params = Array(_len > 1 ? _len - 1 : 0), _key = 1; _key < _len; _key++) {params[_key - 1] = arguments[_key];}
    if (method == "getAll") {
      return LoopRoomsInternal.getAll.apply(LoopRoomsInternal, params);}


    return new Promise(function (resolve, reject) {
      _this3[method].apply(_this3, params.concat([function (error, result) {
        if (error) {
          reject(error);} else 
        {
          resolve(result);}}]));});}, 





  on: function on() {var _eventEmitter;return (_eventEmitter = eventEmitter).on.apply(_eventEmitter, arguments);}, 

  once: function once() {var _eventEmitter2;return (_eventEmitter2 = eventEmitter).once.apply(_eventEmitter2, arguments);}, 

  off: function off() {var _eventEmitter3;return (_eventEmitter3 = eventEmitter).off.apply(_eventEmitter3, arguments);}, 

  /**
   * Expose the internal rooms map for testing purposes only. This avoids
   * needing to mock the server interfaces.
   *
   * @param {Map} roomsCache The new cache data to set for testing purposes. If
   *                         not specified, it will reset the cache.
   */
  _setRoomsCache: function _setRoomsCache(roomsCache, orig) {
    LoopRoomsInternal.rooms.clear();
    gDirty = true;

    if (roomsCache) {
      // Need a clone as the internal map is read-only.
      var _iteratorNormalCompletion8 = true;var _didIteratorError8 = false;var _iteratorError8 = undefined;try {for (var _iterator8 = roomsCache[Symbol.iterator](), _step8; !(_iteratorNormalCompletion8 = (_step8 = _iterator8.next()).done); _iteratorNormalCompletion8 = true) {var _ref5 = _step8.value;var _ref6 = _slicedToArray(_ref5, 2);var key = _ref6[0];var value = _ref6[1];

          LoopRoomsInternal.rooms.set(key, value);
          if (orig) {
            checkForParticipantsUpdate(orig, value);}}} catch (err) {_didIteratorError8 = true;_iteratorError8 = err;} finally {try {if (!_iteratorNormalCompletion8 && _iterator8.return) {_iterator8.return();}} finally {if (_didIteratorError8) {throw _iteratorError8;}}}


      gGetAllPromise = null;
      gDirty = false;}} };



Object.freeze(this.LoopRooms);
