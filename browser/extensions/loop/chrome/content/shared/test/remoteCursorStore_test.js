/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

describe("loop.store.RemoteCursorStore", function() {
  "use strict";

  var expect = chai.expect;
  var sharedActions = loop.shared.actions;
  var CURSOR_MESSAGE_TYPES = loop.shared.utils.CURSOR_MESSAGE_TYPES;
  var sandbox, dispatcher, store, fakeSdkDriver;

  beforeEach(function() {
    sandbox = LoopMochaUtils.createSandbox();

    LoopMochaUtils.stubLoopRequest({
      GetLoopPref: sinon.stub()
    });

    dispatcher = new loop.Dispatcher();
    sandbox.stub(dispatcher, "dispatch");

    fakeSdkDriver = {
      sendCursorMessage: sinon.stub()
    };

    store = new loop.store.RemoteCursorStore(dispatcher, {
      sdkDriver: fakeSdkDriver
    });
  });

  afterEach(function() {
    sandbox.restore();
    LoopMochaUtils.restore();
  });

  describe("#constructor", function() {
    it("should throw an error if sdkDriver is missing", function() {
      expect(function() {
        new loop.store.RemoteCursorStore(dispatcher, {});
      }).to.Throw(/sdkDriver/);
    });

    it("should add a event listeners", function() {
      sandbox.stub(loop, "subscribe");
      new loop.store.RemoteCursorStore(dispatcher, { sdkDriver: fakeSdkDriver });
      sinon.assert.calledTwice(loop.subscribe);
      sinon.assert.calledWith(loop.subscribe, "CursorPositionChange");
      sinon.assert.calledWith(loop.subscribe, "CursorClick");
    });
  });

  describe("#cursor position change", function() {
    it("should send cursor data through the sdk", function() {
      var fakeEvent = {
        ratioX: 10,
        ratioY: 10
      };

      LoopMochaUtils.publish("CursorPositionChange", fakeEvent);

      sinon.assert.calledOnce(fakeSdkDriver.sendCursorMessage);
      sinon.assert.calledWith(fakeSdkDriver.sendCursorMessage, {
        type: CURSOR_MESSAGE_TYPES.POSITION,
        ratioX: fakeEvent.ratioX,
        ratioY: fakeEvent.ratioY
      });
    });
  });

  describe("#cursor click", function() {
    it("should send cursor data through the sdk", function() {
      var fakeClick = true;

      LoopMochaUtils.publish("CursorClick", fakeClick);

      sinon.assert.calledOnce(fakeSdkDriver.sendCursorMessage);
      sinon.assert.calledWith(fakeSdkDriver.sendCursorMessage, {
        type: CURSOR_MESSAGE_TYPES.CLICK
      });
    });
  });

  describe("#sendCursorData", function() {
    it("should do nothing if not a proper event", function() {
      var fakeData = {
        ratioX: 10,
        ratioY: 10,
        type: "not-a-position-event"
      };

      store.sendCursorData(new sharedActions.SendCursorData(fakeData));

      sinon.assert.notCalled(fakeSdkDriver.sendCursorMessage);
    });

    it("should send cursor data through the sdk", function() {
      var fakeData = {
        ratioX: 10,
        ratioY: 10,
        type: CURSOR_MESSAGE_TYPES.POSITION
      };

      store.sendCursorData(new sharedActions.SendCursorData(fakeData));

      sinon.assert.calledOnce(fakeSdkDriver.sendCursorMessage);
      sinon.assert.calledWith(fakeSdkDriver.sendCursorMessage, {
        name: "sendCursorData",
        type: fakeData.type,
        ratioX: fakeData.ratioX,
        ratioY: fakeData.ratioY
      });
    });
  });

  describe("#receivedCursorData", function() {

    it("should do nothing if not a proper event", function() {
      sandbox.stub(store, "setStoreState");

      store.receivedCursorData(new sharedActions.ReceivedCursorData({
        ratioX: 10,
        ratioY: 10,
        type: "not-a-position-event"
      }));

      sinon.assert.notCalled(store.setStoreState);
    });

    it("should save the state of the cursor position", function() {
      store.receivedCursorData(new sharedActions.ReceivedCursorData({
        type: CURSOR_MESSAGE_TYPES.POSITION,
        ratioX: 10,
        ratioY: 10
      }));

      expect(store.getStoreState().remoteCursorPosition).eql({
        ratioX: 10,
        ratioY: 10
      });
    });

    it("should save the state of the cursor click", function() {
      store.receivedCursorData(new sharedActions.ReceivedCursorData({
        type: CURSOR_MESSAGE_TYPES.CLICK
      }));

      expect(store.getStoreState().remoteCursorClick).eql(true);
    });
  });

  describe("#videoDimensionsChanged", function() {
    beforeEach(function() {
      store.setStoreState({
        realVideoSize: null
      });
    });

    it("should save the state", function() {
      store.videoDimensionsChanged(new sharedActions.VideoDimensionsChanged({
        isLocal: false,
        videoType: "screen",
        dimensions: {
          height: 10,
          width: 10
        }
      }));

      expect(store.getStoreState().realVideoSize).eql({
        height: 10,
        width: 10
      });
    });

    it("should not save the state if video type is not screen", function() {
      store.videoDimensionsChanged(new sharedActions.VideoDimensionsChanged({
        isLocal: false,
        videoType: "camera",
        dimensions: {
          height: 10,
          width: 10
        }
      }));

      expect(store.getStoreState().realVideoSize).eql(null);
    });
  });

  describe("#videoScreenStreamChanged", function() {
    beforeEach(function() {
      store.setStoreState({
        remoteCursorPosition: {
          ratioX: 1,
          ratioY: 1
        }
      });
    });

    it("should remove cursor position if screen stream has no video", function() {
      store.videoScreenStreamChanged(new sharedActions.VideoScreenStreamChanged({
        hasVideo: false
      }));

      expect(store.getStoreState().remoteCursorPosition).eql(null);
    });

    it("should not remove cursor position if screen stream has video", function() {
      sandbox.stub(store, "setStoreState");

      store.videoScreenStreamChanged(new sharedActions.VideoScreenStreamChanged({
        hasVideo: true
      }));

      sinon.assert.notCalled(store.setStoreState);
      expect(store.getStoreState().remoteCursorPosition).eql({
        ratioX: 1,
        ratioY: 1
      });
    });
  });
});
