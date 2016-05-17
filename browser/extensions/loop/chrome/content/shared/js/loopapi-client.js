"use strict"; /* This Source Code Form is subject to the terms of the Mozilla Public
               * License, v. 2.0. If a copy of the MPL was not distributed with this file,
               * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
(function () {
  "use strict";

  var _slice = Array.prototype.slice;

  var kMessageName = "Loop:Message";
  var kPushMessageName = "Loop:Message:Push";
  var kBatchMessage = "Batch";
  var gListeningForMessages = false;
  var gListenersMap = {};
  var gListeningForPushMessages = false;
  var gSubscriptionsMap = {};
  var gRootObj = window;

  loop._lastMessageID = 0;

  /**
   * Builds a proper request payload that adheres to the following conditions:
   *  - The first element (index 0) should be a numeric, unique sequence identifier.
   *  - The second element (index 1) should be the request command name in CapitalCase.
   *  - The rest of the elements (indices 2 and higher) contain the request command
   *    arguments.
   *
   * @param  {Array} args The raw request data
   * @return {Array} The request payload in the correct format, as explained above
   */
  function buildRequestArray(args) {
    var command = args.shift();
    // Normalize the command name to look like 'SomeCommand'.
    command = command.charAt(0).toUpperCase() + command.substr(1);

    var seq = ++loop._lastMessageID;
    args.unshift(seq, command);

    return args;}


  /**
   * Send a request to chrome, running in the main process.
   *
   * Note: it's not necessary for a request to receive a reply; some commands are
   *       as simple as fire-and-forget. Therefore each request will be resolved
   *       after a maximum timeout of `kReplyTimeoutMs` milliseconds.
   *
   * @param {String} command Required name of the command to send.
   * @return {Promise} Gets resolved with the result of the command, IF the chrome
   *                   script sent a reply. It never gets rejected.
   */
  loop.request = function request() {
    var args = _slice.call(arguments);

    return new Promise(function (resolve) {
      var payload = buildRequestArray(args);
      var seq = payload[0];

      if (!gListeningForMessages) {
        gListeningForMessages = function listener(message) {
          var replySeq = message.data[0];
          if (!gListenersMap[replySeq]) {
            return;}

          gListenersMap[replySeq](message.data[1]);
          delete gListenersMap[replySeq];};


        gRootObj.addMessageListener(kMessageName, gListeningForMessages);}


      gListenersMap[seq] = resolve;

      gRootObj.sendAsyncMessage(kMessageName, payload);});};



  // These functions should only be used in unit tests.
  loop.request.inspect = function () {return _.extend({}, gListenersMap);};
  loop.request.reset = function () {
    gListeningForMessages = false;
    gListenersMap = {};};


  loop.storedRequests = {};

  /**
   * Store the result of a request for access at a later time.
   *
   * @param  {Array} request Set of request parameters
   * @param  {mixed} result  Whatever the API returned as a result
   */
  loop.storeRequest = function (request, result) {
    loop.storedRequests[request.join("|")] = result;};


  /**
   * Retrieve the result of a request that was stored previously. If the result
   * can not be found, |NULL| will be returned and an error will be logged to the
   * console.
   *
   * @param  {Array} request Set of request parameters
   * @return {mixed} Whatever the result of the API request was at the time it was
   *                 stored.
   */
  loop.getStoredRequest = function (request) {
    var key = request.join("|");
    if (!(key in loop.storedRequests)) {
      console.error("This request has not been stored!", request);
      return null;}


    return loop.storedRequests[key];};


  /**
   * Send multiple requests at once as a batch.
   *
   * @param {...Array} command An unlimited amount of Arrays may be passed as
   *                           individual arguments of this function. They are
   *                           bundled together and sent across.
   * @return {Promise} Gets resolved when all commands have finished with their
   *                   accumulated results.
   */
  loop.requestMulti = function requestMulti() {
    if (!arguments.length) {
      throw new Error("loop.requestMulti: please pass in a list of calls to process in parallel.");}


    var calls = _slice.call(arguments);
    calls.forEach(function (call) {
      if (!Array.isArray(call)) {
        throw new Error("loop.requestMulti: each call must be an array of options, " + 
        "exactly the same as the argument signature of `loop.request()`");}


      buildRequestArray(call);});


    return new Promise(function (resolve) {
      loop.request(kBatchMessage, calls).then(function (resultSet) {
        if (!resultSet) {
          resolve();
          return;}


        // Collect result as a sequenced array and pass it back.
        var result = Object.getOwnPropertyNames(resultSet).map(function (seq) {
          return resultSet[seq];});


        resolve(result);});});};




  /**
   * Subscribe to push messages coming from chrome scripts. Since these may arrive
   * at any time, the subscriptions are permanently registered.
   *
   * @param  {String}   name     Name of the push message
   * @param  {Function} callback Function invoked when the push message arrives
   */
  loop.subscribe = function subscribe(name, callback) {
    if (!gListeningForPushMessages) {
      gRootObj.addMessageListener(kPushMessageName, gListeningForPushMessages = function gListeningForPushMessages(message) {
        var eventName = message.data[0];
        if (!gSubscriptionsMap[eventName]) {
          return;}

        gSubscriptionsMap[eventName].forEach(function (cb) {
          var data = message.data[1];
          if (!Array.isArray(data)) {
            data = [data];}

          cb.apply(null, data);});});}




    if (!gSubscriptionsMap[name]) {
      gSubscriptionsMap[name] = [];}

    gSubscriptionsMap[name].push(callback);};


  // These functions should only be used in unit tests.
  loop.subscribe.inspect = function () {return _.extend({}, gSubscriptionsMap);};
  loop.subscribe.reset = function () {
    gListeningForPushMessages = false;
    gSubscriptionsMap = {};};


  /**
   * Cancel a subscription to a specific push message.
   *
   * @param  {String}   name     Name of the push message
   * @param  {Function} callback Handler function
   */
  loop.unsubscribe = function unsubscribe(name, callback) {
    if (!gSubscriptionsMap[name]) {
      return;}

    var idx = gSubscriptionsMap[name].indexOf(callback);
    if (idx === -1) {
      return;}

    gSubscriptionsMap[name].splice(idx, 1);};


  /**
   * Cancel all running subscriptions.
   */
  loop.unsubscribeAll = function unsubscribeAll() {
    gSubscriptionsMap = {};
    if (gListeningForPushMessages) {
      gRootObj.removeMessageListener(kPushMessageName, gListeningForPushMessages);
      gListeningForPushMessages = false;}};})();
