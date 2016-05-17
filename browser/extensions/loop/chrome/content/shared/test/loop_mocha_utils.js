"use strict"; /* Any copyright is dedicated to the Public Domain.
               * http://creativecommons.org/publicdomain/zero/1.0/ */

/* exported LoopMochaUtils */

var LoopMochaUtils = function (global, _) {
  "use strict";

  var gStubbedRequests;
  var gListenerCallbacks = [];
  var gPushListenerCallbacks = [];
  var gOldAddMessageListener, gOldSendAsyncMessage;
  var gUncaughtError;
  var gCaughtIssues = [];


  /**
   * The messaging between chrome and content (pubsub.js) is using Promises which
   * are inherently async - i.e. resume and resolve on the next tick at the earliest.
   * This complicates things for unit tests writers, as they need to take this
   * into account and prevent/ resolve race conditions when attempting to stub
   * data sources.
   * In order to not impose this burden, we simply replace the asynchronous
   * Promises with synchronous ones here, whilst still allowing for async operations.
   * The upside here is that we can make stubbed chrome message handlers return
   * directly with fake data without the need to make the unit test asynchronous
   * with chai-as-promised or a 'done' callback.
   * This class mirrors the DOM Promise API, so there's no need to learn a new
   * API. `createSandbox` swaps the native Promise object out with this one, so
   * test writers will be using this class automagically.
   *
   * @param  {Function} asyncFn Function to invoke directly that contains async
   *                            continuation(s).
   */
  function SyncThenable(asyncFn) {
    var continuations = [];
    var resolved = false;
    var resolvedWith = null;

    this.then = function (contFn) {
      if (resolved) {
        contFn(resolvedWith);
        return this;}


      continuations.push(contFn);
      return this;};


    /**
     * Used to resolve an object of type SyncThenable.
     *
     * @param  {*} result     The result to return.
     * @return {SyncThenable} A resolved SyncThenable object.
     */
    this.resolve = function (result) {
      resolved = true;
      resolvedWith = result;

      if (!continuations.length) {
        return this;}

      var contFn = continuations.shift();
      while (contFn) {
        contFn(result);
        contFn = continuations.shift();}

      return this;};


    this.reject = function (result) {
      throw result;};


    this.catch = function () {};

    asyncFn(this.resolve.bind(this), this.reject.bind(this));}


  SyncThenable.all = function (promises) {
    return new SyncThenable(function (resolve) {
      var results = [];

      promises.forEach(function (promise) {
        promise.then(function (result) {
          results.push(result);});});



      resolve(results);});};



  /**
   * This simulates the equivalent of Promise.resolve() - calling the
   * resolve function on the raw object.
   *
   * @param  {*} result     The result to return.
   * @return {SyncThenable} A resolved SyncThenable object.
   */
  SyncThenable.resolve = function (result) {
    return new SyncThenable(function (resolve) {
      resolve(result);});};



  /**
   * Simple wrapper around `sinon.sandbox.create()` to also stub the native Promise
   * object out with `SyncThenable`.
   *
   * @return {Sandbox} A Sinon.JS sandbox object.
   */
  function createSandbox() {
    var sandbox = sinon.sandbox.create();
    sandbox.stub(global, "Promise", SyncThenable);
    return sandbox;}


  /**
   * Internal, see `handleIncomingRequest`.
   */
  function invokeListenerCallbacks(data) {
    gListenerCallbacks.forEach(function (cb) {
      cb(data);});}



  /**
   * Invoked when `window.sendAsyncMessage` is called. This means a message came
   * in, containing a request for the chrome data source.
   *
   * @param  {Array}   data    Payload of the request, containing a sequence number,
   *                           action name and action parameters.
   * @param  {Boolean} isBatch |TRUE| when called recursively by a Batch-type message.
   */
  function handleIncomingRequest(data, isBatch) {
    var seq = data.shift();
    var action = data.shift();
    var result;

    if (action === "Batch") {
      result = {};
      data[0].forEach(function (req) {
        result[req[0]] = handleIncomingRequest(req, true);});

      invokeListenerCallbacks({ data: [seq, result] });} else 
    {
      if (!gStubbedRequests[action]) {
        throw new Error("Action '" + action + "' not part of stubbed requests! Please add it!");}

      result = gStubbedRequests[action].apply(gStubbedRequests, data);
      if (isBatch) {
        return result;}

      invokeListenerCallbacks({ data: [seq, result] });}

    return undefined;}


  /**
   * Stub function that replaces `window.addMessageListener`.
   *
   * @param {String}   name             Name of the message to listen for.
   * @param {Function} listenerCallback Callback to invoke when the named message
   *                                    is being sent.
   */
  function addMessageListenerInternal(name, listenerCallback) {
    if (name === "Loop:Message") {
      gListenerCallbacks.push(listenerCallback);} else 
    if (name === "Loop:Message:Push") {
      gPushListenerCallbacks.push(listenerCallback);}}



  /**
   * Stub function that replaces `window.sendAsyncMessageInternal`. See
   * `handleIncomingRequest` for more information.
   *
   * @param  {String} messageName Not used, will always be "Loop:Message".
   * @param  {Array}  data        Payload of the request.
   */
  function sendAsyncMessageInternal(messageName, data) {
    handleIncomingRequest(data);}


  /**
   * Entry point for test writers to add stubs for message handlers that they
   * expect to be called and want them to return fake/ mock data. This allows for
   * finegrained control of the test run.
   * This function can be called multiple times during a test run; the newly provided
   * message handlers will replace the older ones that are active at that time.
   *
   * @param  {Object} stubbedRequests A map of message handlers with the message
   *                                  name as key and the handler function as value.
   *                                  The return value of the handler function will
   *                                  be passed on as the result of a request.
   */
  function stubLoopRequest(stubbedRequests) {
    if (!global.addMessageListener || global.addMessageListener !== addMessageListenerInternal) {
      // Save older versions for later.
      if (!gOldSendAsyncMessage) {
        gOldAddMessageListener = global.addMessageListener;}

      if (!gOldSendAsyncMessage) {
        gOldSendAsyncMessage = global.sendAsyncMessage;}


      global.addMessageListener = addMessageListenerInternal;
      global.sendAsyncMessage = sendAsyncMessageInternal;

      gStubbedRequests = {};}


    _.extend(gStubbedRequests, stubbedRequests);}


  /**
   * Broadcast a push message on demand, which will invoke any active listeners
   * that have subscribed.
   *
   * @param {String} name        Name of the push message.
   * @param {mixed}  [...params] Arbitrary amount of arguments that will be passed
   *                             to the listeners.
   */
  function publish() {
    // Convert to a proper array.
    var args = Array.prototype.slice.call(arguments);
    var name = args.shift();
    gPushListenerCallbacks.forEach(function (cb) {
      cb({ data: [name, args] });});}



  /**
   * Restores our internal state to its original values and reverts adjustments
   * made to the global object.
   * It is recommended to call this function in the tests' tear-down, to make sure
   * that subsequent test runs are not affected by stubs set up earlier.
   */
  function restore() {
    if (!global.addMessageListener) {
      return;}


    if (gOldAddMessageListener) {
      global.addMessageListener = gOldAddMessageListener;} else 
    {
      delete global.addMessageListener;}

    if (gOldSendAsyncMessage) {
      global.sendAsyncMessage = gOldSendAsyncMessage;} else 
    {
      delete global.sendAsyncMessage;}

    gStubbedRequests = null;
    gListenerCallbacks = [];
    gPushListenerCallbacks = [];
    loop.request.reset();
    loop.subscribe.reset();}


  /**
   * Used to initiate trapping of errors and warnings when running tests.
   * addErrorCheckingTests() should be called to add the actual processing of
   * results.
   */
  function trapErrors() {
    window.addEventListener("error", function (error) {
      gUncaughtError = error;});

    var consoleWarn = console.warn;
    var consoleError = console.error;
    console.warn = function () {
      var args = Array.prototype.slice.call(arguments);
      try {
        throw new Error();} 
      catch (e) {
        gCaughtIssues.push([args, e.stack]);}

      consoleWarn.apply(console, args);};

    console.error = function () {
      var args = Array.prototype.slice.call(arguments);
      try {
        throw new Error();} 
      catch (e) {
        gCaughtIssues.push([args, e.stack]);}

      consoleError.apply(console, args);};}



  /**
   * Adds tests to check no warnings nor errors have occurred since trapErrors
   * was called.
   */
  function addErrorCheckingTests() {
    describe("Uncaught Error Check", function () {
      it("should load the tests without errors", function () {
        chai.expect(gUncaughtError && gUncaughtError.message).to.be.undefined;});});



    describe("Unexpected Logged Warnings and Errors Check", function () {
      it("should not log any warnings nor errors", function () {
        if (gCaughtIssues.length) {
          throw new Error(gCaughtIssues);} else 
        {
          chai.expect(gCaughtIssues.length).to.eql(0);}});});}





  /**
   * Utility function for starting the mocha test run. Adds a marker for when
   * the tests have completed.
   */
  function runTests() {
    mocha.run(function () {
      var completeNode = document.createElement("p");
      completeNode.setAttribute("id", "complete");
      completeNode.appendChild(document.createTextNode("Complete"));
      document.getElementById("mocha").appendChild(completeNode);});}



  return { 
    addErrorCheckingTests: addErrorCheckingTests, 
    createSandbox: createSandbox, 
    publish: publish, 
    restore: restore, 
    runTests: runTests, 
    stubLoopRequest: stubLoopRequest, 
    trapErrors: trapErrors };}(

this, _);
