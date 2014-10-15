/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var expect = chai.expect;

describe("loop.store.ConversationStore", function () {
  "use strict";

  var CALL_STATES = loop.store.CALL_STATES;
  var WS_STATES = loop.store.WS_STATES;
  var sharedActions = loop.shared.actions;
  var sharedUtils = loop.shared.utils;
  var sandbox, dispatcher, client, store, fakeSessionData, sdkDriver;
  var contact;
  var connectPromise, resolveConnectPromise, rejectConnectPromise;
  var wsCancelSpy, wsCloseSpy, wsMediaUpSpy, fakeWebsocket;

  function checkFailures(done, f) {
    try {
      f();
      done();
    } catch (err) {
      done(err);
    }
  }

  beforeEach(function() {
    sandbox = sinon.sandbox.create();

    contact = {
      name: [ "Mr Smith" ],
      email: [{
        type: "home",
        value: "fakeEmail",
        pref: true
      }]
    };

    dispatcher = new loop.Dispatcher();
    client = {
      setupOutgoingCall: sinon.stub()
    };
    sdkDriver = {
      connectSession: sinon.stub(),
      disconnectSession: sinon.stub()
    };

    wsCancelSpy = sinon.spy();
    wsCloseSpy = sinon.spy();
    wsMediaUpSpy = sinon.spy();

    fakeWebsocket = {
      cancel: wsCancelSpy,
      close: wsCloseSpy,
      mediaUp: wsMediaUpSpy
    };

    store = new loop.store.ConversationStore({}, {
      client: client,
      dispatcher: dispatcher,
      sdkDriver: sdkDriver
    });
    fakeSessionData = {
      apiKey: "fakeKey",
      callId: "142536",
      sessionId: "321456",
      sessionToken: "341256",
      websocketToken: "543216",
      progressURL: "fakeURL"
    };

    var dummySocket = {
      close: sinon.spy(),
      send: sinon.spy()
    };

    connectPromise = new Promise(function(resolve, reject) {
      resolveConnectPromise = resolve;
      rejectConnectPromise = reject;
    });

    sandbox.stub(loop.CallConnectionWebSocket.prototype,
      "promiseConnect").returns(connectPromise);
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("#initialize", function() {
    it("should throw an error if the dispatcher is missing", function() {
      expect(function() {
        new loop.store.ConversationStore({}, {
          client: client,
          sdkDriver: sdkDriver
        });
      }).to.Throw(/dispatcher/);
    });

    it("should throw an error if the client is missing", function() {
      expect(function() {
        new loop.store.ConversationStore({}, {
          dispatcher: dispatcher,
          sdkDriver: sdkDriver
        });
      }).to.Throw(/client/);
    });

    it("should throw an error if the sdkDriver is missing", function() {
      expect(function() {
        new loop.store.ConversationStore({}, {
          client: client,
          dispatcher: dispatcher
        });
      }).to.Throw(/sdkDriver/);
    });
  });

  describe("#connectionFailure", function() {
    beforeEach(function() {
      store._websocket = fakeWebsocket;
    });

    it("should disconnect the session", function() {
      dispatcher.dispatch(
        new sharedActions.ConnectionFailure({reason: "fake"}));

      sinon.assert.calledOnce(sdkDriver.disconnectSession);
    });

    it("should ensure the websocket is closed", function() {
      dispatcher.dispatch(
        new sharedActions.ConnectionFailure({reason: "fake"}));

      sinon.assert.calledOnce(wsCloseSpy);
    });

    it("should set the state to 'terminated'", function() {
      store.set({callState: CALL_STATES.ALERTING});

      dispatcher.dispatch(
        new sharedActions.ConnectionFailure({reason: "fake"}));

      expect(store.get("callState")).eql(CALL_STATES.TERMINATED);
      expect(store.get("callStateReason")).eql("fake");
    });
  });

  describe("#connectionProgress", function() {
    describe("progress: init", function() {
      it("should change the state from 'gather' to 'connecting'", function() {
        store.set({callState: CALL_STATES.GATHER});

        dispatcher.dispatch(
          new sharedActions.ConnectionProgress({wsState: WS_STATES.INIT}));

        expect(store.get("callState")).eql(CALL_STATES.CONNECTING);
      });
    });

    describe("progress: alerting", function() {
      it("should change the state from 'gather' to 'alerting'", function() {
        store.set({callState: CALL_STATES.GATHER});

        dispatcher.dispatch(
          new sharedActions.ConnectionProgress({wsState: WS_STATES.ALERTING}));

        expect(store.get("callState")).eql(CALL_STATES.ALERTING);
      });

      it("should change the state from 'init' to 'alerting'", function() {
        store.set({callState: CALL_STATES.INIT});

        dispatcher.dispatch(
          new sharedActions.ConnectionProgress({wsState: WS_STATES.ALERTING}));

        expect(store.get("callState")).eql(CALL_STATES.ALERTING);
      });
    });

    describe("progress: connecting", function() {
      beforeEach(function() {
        store.set({callState: CALL_STATES.ALERTING});
      });

      it("should change the state to 'ongoing'", function() {
        dispatcher.dispatch(
          new sharedActions.ConnectionProgress({wsState: WS_STATES.CONNECTING}));

        expect(store.get("callState")).eql(CALL_STATES.ONGOING);
      });

      it("should connect the session", function() {
        store.set(fakeSessionData);

        dispatcher.dispatch(
          new sharedActions.ConnectionProgress({wsState: WS_STATES.CONNECTING}));

        sinon.assert.calledOnce(sdkDriver.connectSession);
        sinon.assert.calledWithExactly(sdkDriver.connectSession, {
          apiKey: "fakeKey",
          sessionId: "321456",
          sessionToken: "341256"
        });
      });
    });
  });

  describe("#gatherCallData", function() {
    beforeEach(function() {
      store.set({callState: CALL_STATES.INIT});

      navigator.mozLoop = {
        getCallData: function() {
          return {
            contact: contact,
            callType: sharedUtils.CALL_TYPES.AUDIO_VIDEO
          };
        }
      };
    });

    afterEach(function() {
      delete navigator.mozLoop;
    });

    it("should set the state to 'gather'", function() {
      dispatcher.dispatch(
        new sharedActions.GatherCallData({
          callId: "76543218",
          outgoing: true
        }));

      expect(store.get("callState")).eql(CALL_STATES.GATHER);
    });

    it("should save the basic call information", function() {
      dispatcher.dispatch(
        new sharedActions.GatherCallData({
          callId: "123456",
          outgoing: true
        }));

      expect(store.get("callId")).eql("123456");
      expect(store.get("outgoing")).eql(true);
    });

    it("should save the basic information from the mozLoop api", function() {
      dispatcher.dispatch(
        new sharedActions.GatherCallData({
          callId: "123456",
          outgoing: true
        }));

      expect(store.get("contact")).eql(contact);
      expect(store.get("callType")).eql(sharedUtils.CALL_TYPES.AUDIO_VIDEO);
    });

    describe("outgoing calls", function() {
      var outgoingCallData;

      beforeEach(function() {
        outgoingCallData = {
          callId: "123456",
          outgoing: true
        };
      });

      it("should request the outgoing call data", function() {
        dispatcher.dispatch(
          new sharedActions.GatherCallData(outgoingCallData));

        sinon.assert.calledOnce(client.setupOutgoingCall);
        sinon.assert.calledWith(client.setupOutgoingCall,
          ["fakeEmail"], sharedUtils.CALL_TYPES.AUDIO_VIDEO);
      });

      describe("server response handling", function() {
        beforeEach(function() {
          sandbox.stub(dispatcher, "dispatch");
        });

        it("should dispatch a connect call action on success", function() {
          var callData = {
            apiKey: "fakeKey"
          };

          client.setupOutgoingCall.callsArgWith(2, null, callData);

          store.gatherCallData(
            new sharedActions.GatherCallData(outgoingCallData));

          sinon.assert.calledOnce(dispatcher.dispatch);
          // Can't use instanceof here, as that matches any action
          sinon.assert.calledWithMatch(dispatcher.dispatch,
            sinon.match.hasOwn("name", "connectCall"));
          sinon.assert.calledWithMatch(dispatcher.dispatch,
            sinon.match.hasOwn("sessionData", callData));
        });

        it("should dispatch a connection failure action on failure", function() {
          client.setupOutgoingCall.callsArgWith(2, {});

          store.gatherCallData(
            new sharedActions.GatherCallData(outgoingCallData));

          sinon.assert.calledOnce(dispatcher.dispatch);
          // Can't use instanceof here, as that matches any action
          sinon.assert.calledWithMatch(dispatcher.dispatch,
            sinon.match.hasOwn("name", "connectionFailure"));
          sinon.assert.calledWithMatch(dispatcher.dispatch,
            sinon.match.hasOwn("reason", "setup"));
        });
      });
    });
  });

  describe("#connectCall", function() {
    it("should save the call session data", function() {
      dispatcher.dispatch(
        new sharedActions.ConnectCall({sessionData: fakeSessionData}));

      expect(store.get("apiKey")).eql("fakeKey");
      expect(store.get("callId")).eql("142536");
      expect(store.get("sessionId")).eql("321456");
      expect(store.get("sessionToken")).eql("341256");
      expect(store.get("websocketToken")).eql("543216");
      expect(store.get("progressURL")).eql("fakeURL");
    });

    it("should initialize the websocket", function() {
      sandbox.stub(loop, "CallConnectionWebSocket").returns({
        promiseConnect: function() { return connectPromise; },
        on: sinon.spy()
      });

      dispatcher.dispatch(
        new sharedActions.ConnectCall({sessionData: fakeSessionData}));

      sinon.assert.calledOnce(loop.CallConnectionWebSocket);
      sinon.assert.calledWithExactly(loop.CallConnectionWebSocket, {
        url: "fakeURL",
        callId: "142536",
        websocketToken: "543216"
      });
    });

    it("should connect the websocket to the server", function() {
      dispatcher.dispatch(
        new sharedActions.ConnectCall({sessionData: fakeSessionData}));

      sinon.assert.calledOnce(store._websocket.promiseConnect);
    });

    describe("WebSocket connection result", function() {
      beforeEach(function() {
        dispatcher.dispatch(
          new sharedActions.ConnectCall({sessionData: fakeSessionData}));

        sandbox.stub(dispatcher, "dispatch");
      });

      it("should dispatch a connection progress action on success", function(done) {
        resolveConnectPromise(WS_STATES.INIT);

        connectPromise.then(function() {
          checkFailures(done, function() {
            sinon.assert.calledOnce(dispatcher.dispatch);
            // Can't use instanceof here, as that matches any action
            sinon.assert.calledWithMatch(dispatcher.dispatch,
              sinon.match.hasOwn("name", "connectionProgress"));
            sinon.assert.calledWithMatch(dispatcher.dispatch,
              sinon.match.hasOwn("wsState", WS_STATES.INIT));
          });
        }, function() {
          done(new Error("Promise should have been resolve, not rejected"));
        });
      });

      it("should dispatch a connection failure action on failure", function(done) {
        rejectConnectPromise();

        connectPromise.then(function() {
          done(new Error("Promise should have been rejected, not resolved"));
        }, function() {
          checkFailures(done, function() {
            sinon.assert.calledOnce(dispatcher.dispatch);
            // Can't use instanceof here, as that matches any action
            sinon.assert.calledWithMatch(dispatcher.dispatch,
              sinon.match.hasOwn("name", "connectionFailure"));
            sinon.assert.calledWithMatch(dispatcher.dispatch,
              sinon.match.hasOwn("reason", "websocket-setup"));
           });
        });
      });
    });
  });

  describe("#hangupCall", function() {
    var wsMediaFailSpy, wsCloseSpy;
    beforeEach(function() {
      wsMediaFailSpy = sinon.spy();
      wsCloseSpy = sinon.spy();

      store._websocket = {
        mediaFail: wsMediaFailSpy,
        close: wsCloseSpy
      };
      store.set({callState: CALL_STATES.ONGOING});
    });

    it("should disconnect the session", function() {
      dispatcher.dispatch(new sharedActions.HangupCall());

      sinon.assert.calledOnce(sdkDriver.disconnectSession);
    });

    it("should send a media-fail message to the websocket if it is open", function() {
      dispatcher.dispatch(new sharedActions.HangupCall());

      sinon.assert.calledOnce(wsMediaFailSpy);
    });

    it("should ensure the websocket is closed", function() {
      dispatcher.dispatch(new sharedActions.HangupCall());

      sinon.assert.calledOnce(wsCloseSpy);
    });

    it("should set the callState to finished", function() {
      dispatcher.dispatch(new sharedActions.HangupCall());

      expect(store.get("callState")).eql(CALL_STATES.FINISHED);
    });
  });

  describe("#peerHungupCall", function() {
    var wsMediaFailSpy, wsCloseSpy;
    beforeEach(function() {
      wsMediaFailSpy = sinon.spy();
      wsCloseSpy = sinon.spy();

      store._websocket = {
        mediaFail: wsMediaFailSpy,
        close: wsCloseSpy
      };
      store.set({callState: CALL_STATES.ONGOING});
    });

    it("should disconnect the session", function() {
      dispatcher.dispatch(new sharedActions.PeerHungupCall());

      sinon.assert.calledOnce(sdkDriver.disconnectSession);
    });

    it("should ensure the websocket is closed", function() {
      dispatcher.dispatch(new sharedActions.PeerHungupCall());

      sinon.assert.calledOnce(wsCloseSpy);
    });

    it("should set the callState to finished", function() {
      dispatcher.dispatch(new sharedActions.PeerHungupCall());

      expect(store.get("callState")).eql(CALL_STATES.FINISHED);
    });
  });

  describe("#cancelCall", function() {
    beforeEach(function() {
      store._websocket = fakeWebsocket;

      store.set({callState: CALL_STATES.CONNECTING});
    });

    it("should disconnect the session", function() {
      dispatcher.dispatch(new sharedActions.CancelCall());

      sinon.assert.calledOnce(sdkDriver.disconnectSession);
    });

    it("should send a cancel message to the websocket if it is open", function() {
      dispatcher.dispatch(new sharedActions.CancelCall());

      sinon.assert.calledOnce(wsCancelSpy);
    });

    it("should ensure the websocket is closed", function() {
      dispatcher.dispatch(new sharedActions.CancelCall());

      sinon.assert.calledOnce(wsCloseSpy);
    });

    it("should set the state to close if the call is connecting", function() {
      dispatcher.dispatch(new sharedActions.CancelCall());

      expect(store.get("callState")).eql(CALL_STATES.CLOSE);
    });

    it("should set the state to close if the call has terminated already", function() {
      store.set({callState: CALL_STATES.TERMINATED});

      dispatcher.dispatch(new sharedActions.CancelCall());

      expect(store.get("callState")).eql(CALL_STATES.CLOSE);
    });

  });

  describe("#retryCall", function() {
    it("should set the state to gather", function() {
      store.set({callState: CALL_STATES.TERMINATED});

      dispatcher.dispatch(new sharedActions.RetryCall());

      expect(store.get("callState")).eql(CALL_STATES.GATHER);
    });

    it("should request the outgoing call data", function() {
      store.set({
        callState: CALL_STATES.TERMINATED,
        outgoing: true,
        callType: sharedUtils.CALL_TYPES.AUDIO_VIDEO,
        contact: contact
      });

      dispatcher.dispatch(new sharedActions.RetryCall());

      sinon.assert.calledOnce(client.setupOutgoingCall);
      sinon.assert.calledWith(client.setupOutgoingCall,
        ["fakeEmail"], sharedUtils.CALL_TYPES.AUDIO_VIDEO);
    });
  });

  describe("#mediaConnected", function() {
    it("should send mediaUp via the websocket", function() {
      store._websocket = fakeWebsocket;

      dispatcher.dispatch(new sharedActions.MediaConnected());

      sinon.assert.calledOnce(wsMediaUpSpy);
    });
  });

  describe("#setMute", function() {
    it("should save the mute state for the audio stream", function() {
      store.set({"audioMuted": false});

      dispatcher.dispatch(new sharedActions.SetMute({
        type: "audio",
        enabled: true
      }));

      expect(store.get("audioMuted")).eql(false);
    });

    it("should save the mute state for the video stream", function() {
      store.set({"videoMuted": true});

      dispatcher.dispatch(new sharedActions.SetMute({
        type: "video",
        enabled: false
      }));

      expect(store.get("videoMuted")).eql(true);
    });
  });

  describe("Events", function() {
    describe("Websocket progress", function() {
      beforeEach(function() {
        dispatcher.dispatch(
          new sharedActions.ConnectCall({sessionData: fakeSessionData}));

        sandbox.stub(dispatcher, "dispatch");
      });

      it("should dispatch a connection failure action on 'terminate'", function() {
        store._websocket.trigger("progress", {
          state: WS_STATES.TERMINATED,
          reason: "reject"
        });

        sinon.assert.calledOnce(dispatcher.dispatch);
        // Can't use instanceof here, as that matches any action
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "connectionFailure"));
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("reason", "reject"));
      });

      it("should dispatch a connection progress action on 'alerting'", function() {
        store._websocket.trigger("progress", {state: WS_STATES.ALERTING});

        sinon.assert.calledOnce(dispatcher.dispatch);
        // Can't use instanceof here, as that matches any action
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "connectionProgress"));
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("wsState", WS_STATES.ALERTING));
      });
    });
  });
});
