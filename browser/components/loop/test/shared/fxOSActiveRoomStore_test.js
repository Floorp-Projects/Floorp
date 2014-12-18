/* global chai, loop */

var expect = chai.expect;
var sharedActions = loop.shared.actions;

describe("loop.store.FxOSActiveRoomStore", function () {
  "use strict";

  var ROOM_STATES = loop.store.ROOM_STATES;

  var sandbox;
  var dispatcher;
  var fakeMozLoop;
  var store;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    sandbox.useFakeTimers();

    dispatcher = new loop.Dispatcher();
    sandbox.stub(dispatcher, "dispatch");

    fakeMozLoop = {
      setLoopPref: sandbox.stub(),
      rooms: {
        join: sinon.stub()
      }
    };

    store = new loop.store.FxOSActiveRoomStore(dispatcher, {
      mozLoop: fakeMozLoop
    });
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("#FxOSActiveRoomStore - constructor", function() {
    it("should throw an error if mozLoop is missing", function() {
      expect(function() {
        new loop.store.FxOSActiveRoomStore(dispatcher);
      }).to.Throw(/mozLoop/);
    });
  });

  describe("#FxOSActiveRoomStore - fetchServerData", function() {
    it("should save the token", function() {
      store.fetchServerData(new sharedActions.FetchServerData({
        windowType: "room",
        token: "fakeToken"
      }));

      expect(store.getStoreState().roomToken).eql("fakeToken");
    });

    it("should set the state to `READY`", function() {
      store.fetchServerData(new sharedActions.FetchServerData({
        windowType: "room",
        token: "fakeToken"
      }));

      expect(store.getStoreState().roomState).eql(ROOM_STATES.READY);
    });
  });

  describe("#FxOSActiveRoomStore - setupOutgoingRoom", function() {
    var realMozActivity;
    var _activityDetail;
    var _onerror;

    function fireError(errorName) {
      _onerror({
        target: {
          error: {
            name: errorName
          }
        }
      });
    }

    before(function() {
      realMozActivity = window.MozActivity;

      window.MozActivity = function(activityDetail) {
        _activityDetail = activityDetail;
        return {
          set onerror(cbk) {
            _onerror = cbk;
          }
        };
      };
    });

    after(function() {
      window.MozActivity = realMozActivity;
    });

    beforeEach(function() {
      sandbox.stub(console, "error");
      _activityDetail = undefined;
      _onerror = undefined;
    });

    afterEach(function() {
      sandbox.restore();
    });

    it("should reset failureReason", function() {
      store.setStoreState({failureReason: "Test"});

      store.joinRoom();

      expect(store.getStoreState().failureReason).eql(undefined);
    });

    it("should create an activity", function() {
      store.setStoreState({
        roomToken: "fakeToken",
        token: "fakeToken"
      });

      expect(_activityDetail).to.not.exist;
      store.joinRoom();
      expect(_activityDetail).to.exist;
      expect(_activityDetail).eql({
        name: "room-call",
        data: {
          type: "loop/rToken",
          token: "fakeToken"
        }
      });
    });

    it("should change the store state when the activity fail with a " +
       "NO_PROVIDER error", function() {

      loop.config = {
        marketplaceUrl: "http://market/"
      };
      store._setupOutgoingRoom(true);
      fireError("NO_PROVIDER");
      expect(store.getStoreState().marketplaceSrc).eql(
        loop.config.marketplaceUrl
      );
    });

    it("should log an error when the activity fail with a error different " +
       "from NO_PROVIDER", function() {
      loop.config = {
        marketplaceUrl: "http://market/"
      };
      store._setupOutgoingRoom(true);
      fireError("whatever");
      sinon.assert.calledOnce(console.error);
      sinon.assert.calledWith(console.error, "Unexpected whatever");
    });

    it("should log an error and exist if an activity error is received when " +
       "the parameter is false ", function() {
      loop.config = {
        marketplaceUrl: "http://market/"
      };
      store._setupOutgoingRoom(false);
      fireError("whatever");
      sinon.assert.calledOnce(console.error);
      sinon.assert.calledWith(console.error,
        "Unexpected activity launch error after the app has been installed");
    });
  });

  describe("#FxOSActiveRoomStore - _onMarketplaceMessage", function() {
    var setupOutgoingRoom;

    beforeEach(function() {
      sandbox.stub(console, "error");
      setupOutgoingRoom = sandbox.stub(store, "_setupOutgoingRoom");
    });

    afterEach(function() {
      setupOutgoingRoom.restore();
    });

    it("We should call trigger a FxOS outgoing call if we get " +
       "install-package message without error", function() {

      sinon.assert.notCalled(setupOutgoingRoom);
      store._onMarketplaceMessage({
        data: {
          name: "install-package"
        }
      });
      sinon.assert.calledOnce(setupOutgoingRoom);
    });

    it("We should log an error if we get install-package message with an " +
       "error", function() {

      sinon.assert.notCalled(setupOutgoingRoom);
      store._onMarketplaceMessage({
        data: {
          name: "install-package",
          error: { error: "whatever error" }
        }
      });
      sinon.assert.notCalled(setupOutgoingRoom);
      sinon.assert.calledOnce(console.error);
      sinon.assert.calledWith(console.error, "whatever error");
    });
  });
});

