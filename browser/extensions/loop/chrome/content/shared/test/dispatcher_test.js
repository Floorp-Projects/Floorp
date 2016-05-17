"use strict"; /* Any copyright is dedicated to the Public Domain.
               * http://creativecommons.org/publicdomain/zero/1.0/ */

describe("loop.Dispatcher", function () {
  "use strict";

  var expect = chai.expect;
  var sharedActions = loop.shared.actions;
  var dispatcher, sandbox;

  beforeEach(function () {
    sandbox = sinon.sandbox.create();
    dispatcher = new loop.Dispatcher();});


  afterEach(function () {
    sandbox.restore();});


  describe("#register", function () {
    it("should register a store against an action name", function () {
      var object = { fake: true };

      dispatcher.register(object, ["getWindowData"]);

      // XXXmikedeboer: Consider changing these tests to not access private
      //                properties anymore (`_eventData`).
      expect(dispatcher._eventData.getWindowData[0]).eql(object);});


    it("should register multiple store against an action name", function () {
      var object1 = { fake: true };
      var object2 = { fake2: true };

      dispatcher.register(object1, ["getWindowData"]);
      dispatcher.register(object2, ["getWindowData"]);

      expect(dispatcher._eventData.getWindowData[0]).eql(object1);
      expect(dispatcher._eventData.getWindowData[1]).eql(object2);});});



  describe("#unregister", function () {
    it("should unregister a store against an action name", function () {
      var object = { fake: true };

      dispatcher.register(object, ["getWindowData"]);
      dispatcher.unregister(object, ["getWindowData"]);

      expect(dispatcher._eventData.hasOwnProperty("getWindowData")).eql(false);});


    it("should unregister multiple stores against an action name", function () {
      var object1 = { fake: true };
      var object2 = { fake2: true };

      dispatcher.register(object1, ["getWindowData"]);
      dispatcher.register(object2, ["getWindowData"]);

      dispatcher.unregister(object1, ["getWindowData"]);
      expect(dispatcher._eventData.getWindowData.length).eql(1);

      dispatcher.unregister(object2, ["getWindowData"]);
      expect(dispatcher._eventData.hasOwnProperty("getWindowData")).eql(false);});});



  describe("#dispatch", function () {
    var getDataStore1, getDataStore2, gotMediaPermissionStore1, mediaConnectedStore1;
    var getDataAction, gotMediaPermissionAction, mediaConnectedAction;

    beforeEach(function () {
      getDataAction = new sharedActions.GetWindowData({ 
        windowId: "42" });


      gotMediaPermissionAction = new sharedActions.GotMediaPermission();
      mediaConnectedAction = new sharedActions.MediaConnected({ 
        sessionData: {} });


      getDataStore1 = { 
        getWindowData: sinon.stub() };

      getDataStore2 = { 
        getWindowData: sinon.stub() };

      gotMediaPermissionStore1 = { 
        gotMediaPermission: sinon.stub() };

      mediaConnectedStore1 = { 
        mediaConnected: function mediaConnected() {} };


      dispatcher.register(getDataStore1, ["getWindowData"]);
      dispatcher.register(getDataStore2, ["getWindowData"]);
      dispatcher.register(gotMediaPermissionStore1, ["gotMediaPermission"]);
      dispatcher.register(mediaConnectedStore1, ["mediaConnected"]);});


    it("should dispatch an action to the required object", function () {
      dispatcher.dispatch(gotMediaPermissionAction);

      sinon.assert.notCalled(getDataStore1.getWindowData);

      sinon.assert.calledOnce(gotMediaPermissionStore1.gotMediaPermission);
      sinon.assert.calledWithExactly(gotMediaPermissionStore1.gotMediaPermission, 
      gotMediaPermissionAction);

      sinon.assert.notCalled(getDataStore2.getWindowData);});


    it("should dispatch actions to multiple objects", function () {
      dispatcher.dispatch(getDataAction);

      sinon.assert.calledOnce(getDataStore1.getWindowData);
      sinon.assert.calledWithExactly(getDataStore1.getWindowData, getDataAction);

      sinon.assert.notCalled(gotMediaPermissionStore1.gotMediaPermission);

      sinon.assert.calledOnce(getDataStore2.getWindowData);
      sinon.assert.calledWithExactly(getDataStore2.getWindowData, getDataAction);});


    it("should dispatch multiple actions", function () {
      dispatcher.dispatch(gotMediaPermissionAction);
      dispatcher.dispatch(getDataAction);

      sinon.assert.calledOnce(gotMediaPermissionStore1.gotMediaPermission);
      sinon.assert.calledOnce(getDataStore1.getWindowData);
      sinon.assert.calledOnce(getDataStore2.getWindowData);});


    describe("Error handling", function () {
      beforeEach(function () {
        sandbox.stub(console, "error");});


      it("should handle uncaught exceptions", function () {
        getDataStore1.getWindowData.throws("Uncaught Error");

        dispatcher.dispatch(getDataAction);
        dispatcher.dispatch(gotMediaPermissionAction);

        sinon.assert.calledOnce(getDataStore1.getWindowData);
        sinon.assert.calledOnce(getDataStore2.getWindowData);
        sinon.assert.calledOnce(gotMediaPermissionStore1.gotMediaPermission);});


      it("should log uncaught exceptions", function () {
        getDataStore1.getWindowData.throws("Uncaught Error");

        dispatcher.dispatch(getDataAction);

        sinon.assert.calledOnce(console.error);});});



    describe("Queued actions", function () {
      beforeEach(function () {
        // Restore the stub, so that we can easily add a function to be
        // returned. Unfortunately, sinon doesn't make this easy.
        sandbox.stub(mediaConnectedStore1, "mediaConnected", function () {
          dispatcher.dispatch(getDataAction);

          sinon.assert.notCalled(getDataStore1.getWindowData);
          sinon.assert.notCalled(getDataStore2.getWindowData);});});



      it("should not dispatch an action if the previous action hasn't finished", function () {
        // Dispatch the first action. The action handler dispatches the second
        // action - see the beforeEach above.
        dispatcher.dispatch(mediaConnectedAction);

        sinon.assert.calledOnce(mediaConnectedStore1.mediaConnected);});


      it("should dispatch an action when the previous action finishes", function () {
        // Dispatch the first action. The action handler dispatches the second
        // action - see the beforeEach above.
        dispatcher.dispatch(mediaConnectedAction);

        sinon.assert.calledOnce(mediaConnectedStore1.mediaConnected);
        // These should be called, because the dispatcher synchronously queues actions.
        sinon.assert.calledOnce(getDataStore1.getWindowData);
        sinon.assert.calledOnce(getDataStore2.getWindowData);});});});});
