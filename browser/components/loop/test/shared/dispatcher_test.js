/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var expect = chai.expect;

describe("loop.Dispatcher", function () {
  "use strict";

  var sharedActions = loop.shared.actions;
  var dispatcher, sandbox;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    dispatcher = new loop.Dispatcher();
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("#register", function() {
    it("should register a store against an action name", function() {
      var object = { fake: true };

      dispatcher.register(object, ["getWindowData"]);

      expect(dispatcher._eventData["getWindowData"][0]).eql(object);
    });

    it("should register multiple store against an action name", function() {
      var object1 = { fake: true };
      var object2 = { fake2: true };

      dispatcher.register(object1, ["getWindowData"]);
      dispatcher.register(object2, ["getWindowData"]);

      expect(dispatcher._eventData["getWindowData"][0]).eql(object1);
      expect(dispatcher._eventData["getWindowData"][1]).eql(object2);
    });
  });

  describe("#dispatch", function() {
    var getDataStore1, getDataStore2, cancelStore1, connectStore1;
    var getDataAction, cancelAction, connectAction, resolveCancelStore1;

    beforeEach(function() {
      getDataAction = new sharedActions.GetWindowData({
        windowId: "42"
      });

      cancelAction = new sharedActions.CancelCall();
      connectAction = new sharedActions.ConnectCall({
        sessionData: {}
      });

      getDataStore1 = {
        getWindowData: sinon.stub()
      };
      getDataStore2 = {
        getWindowData: sinon.stub()
      };
      cancelStore1 = {
        cancelCall: sinon.stub()
      };
      connectStore1 = {
        connectCall: function() {}
      };

      dispatcher.register(getDataStore1, ["getWindowData"]);
      dispatcher.register(getDataStore2, ["getWindowData"]);
      dispatcher.register(cancelStore1, ["cancelCall"]);
      dispatcher.register(connectStore1, ["connectCall"]);
    });

    it("should dispatch an action to the required object", function() {
      dispatcher.dispatch(cancelAction);

      sinon.assert.notCalled(getDataStore1.getWindowData);

      sinon.assert.calledOnce(cancelStore1.cancelCall);
      sinon.assert.calledWithExactly(cancelStore1.cancelCall, cancelAction);

      sinon.assert.notCalled(getDataStore2.getWindowData);
    });

    it("should dispatch actions to multiple objects", function() {
      dispatcher.dispatch(getDataAction);

      sinon.assert.calledOnce(getDataStore1.getWindowData);
      sinon.assert.calledWithExactly(getDataStore1.getWindowData, getDataAction);

      sinon.assert.notCalled(cancelStore1.cancelCall);

      sinon.assert.calledOnce(getDataStore2.getWindowData);
      sinon.assert.calledWithExactly(getDataStore2.getWindowData, getDataAction);
    });

    it("should dispatch multiple actions", function() {
      dispatcher.dispatch(cancelAction);
      dispatcher.dispatch(getDataAction);

      sinon.assert.calledOnce(cancelStore1.cancelCall);
      sinon.assert.calledOnce(getDataStore1.getWindowData);
      sinon.assert.calledOnce(getDataStore2.getWindowData);
    });

    describe("Queued actions", function() {
      beforeEach(function() {
        // Restore the stub, so that we can easily add a function to be
        // returned. Unfortunately, sinon doesn't make this easy.
        sandbox.stub(connectStore1, "connectCall", function() {
          dispatcher.dispatch(getDataAction);

          sinon.assert.notCalled(getDataStore1.getWindowData);
          sinon.assert.notCalled(getDataStore2.getWindowData);
        });
      });

      it("should not dispatch an action if the previous action hasn't finished", function() {
        // Dispatch the first action. The action handler dispatches the second
        // action - see the beforeEach above.
        dispatcher.dispatch(connectAction);

        sinon.assert.calledOnce(connectStore1.connectCall);
      });

      it("should dispatch an action when the previous action finishes", function() {
        // Dispatch the first action. The action handler dispatches the second
        // action - see the beforeEach above.
        dispatcher.dispatch(connectAction);

        sinon.assert.calledOnce(connectStore1.connectCall);
        // These should be called, because the dispatcher synchronously queues actions.
        sinon.assert.calledOnce(getDataStore1.getWindowData);
        sinon.assert.calledOnce(getDataStore2.getWindowData);
      });
    });
  });
});
