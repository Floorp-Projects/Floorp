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

    it("should add a CursorPositionChange event listener", function() {
      sandbox.stub(loop, "subscribe");
      new loop.store.RemoteCursorStore(dispatcher, { sdkDriver: fakeSdkDriver });
      sinon.assert.calledOnce(loop.subscribe);
      sinon.assert.calledWith(loop.subscribe, "CursorPositionChange");
    });
  });

  describe("#_cursorPositionChangeListener", function() {
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

  describe("#receivedCursorData", function() {
    it("should save the state", function() {
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
