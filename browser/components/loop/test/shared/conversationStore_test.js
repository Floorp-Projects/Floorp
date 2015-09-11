/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

describe("loop.store.ConversationStore", function () {
  "use strict";

  var expect = chai.expect;
  var CALL_STATES = loop.store.CALL_STATES;
  var WS_STATES = loop.store.WS_STATES;
  var CALL_TYPES = loop.shared.utils.CALL_TYPES;
  var WEBSOCKET_REASONS = loop.shared.utils.WEBSOCKET_REASONS;
  var REST_ERRNOS = loop.shared.utils.REST_ERRNOS;
  var FAILURE_DETAILS = loop.shared.utils.FAILURE_DETAILS;
  var sharedActions = loop.shared.actions;
  var sharedUtils = loop.shared.utils;
  var sandbox, dispatcher, client, store, fakeSessionData, sdkDriver;
  var contact, fakeMozLoop, fakeVideoElement;
  var connectPromise, resolveConnectPromise, rejectConnectPromise;
  var wsCancelSpy, wsCloseSpy, wsDeclineSpy, wsMediaUpSpy, fakeWebsocket;

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

    fakeMozLoop = {
      ROOM_CREATE: {
        CREATE_SUCCESS: 0,
        CREATE_FAIL: 1
      },
      getLoopPref: sandbox.stub(),
      addConversationContext: sandbox.stub(),
      calls: {
        setCallInProgress: sandbox.stub(),
        clearCallInProgress: sandbox.stub(),
        blockDirectCaller: sandbox.stub()
      },
      rooms: {
        create: sandbox.stub()
      },
      telemetryAddValue: sandbox.stub()
    };

    dispatcher = new loop.Dispatcher();
    client = {
      setupOutgoingCall: sinon.stub(),
      requestCallUrl: sinon.stub()
    };
    sdkDriver = {
      connectSession: sinon.stub(),
      disconnectSession: sinon.stub(),
      retryPublishWithoutVideo: sinon.stub()
    };

    wsCancelSpy = sinon.spy();
    wsCloseSpy = sinon.spy();
    wsDeclineSpy = sinon.spy();
    wsMediaUpSpy = sinon.spy();

    fakeWebsocket = {
      cancel: wsCancelSpy,
      close: wsCloseSpy,
      decline: wsDeclineSpy,
      mediaUp: wsMediaUpSpy
    };

    store = new loop.store.ConversationStore(dispatcher, {
      client: client,
      mozLoop: fakeMozLoop,
      sdkDriver: sdkDriver
    });
    fakeSessionData = {
      apiKey: "fakeKey",
      callId: "142536",
      sessionId: "321456",
      sessionToken: "341256",
      websocketToken: "543216",
      windowId: "28",
      progressURL: "fakeURL"
    };

    fakeVideoElement = { id: "fakeVideoElement" };

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
    it("should throw an error if the client is missing", function() {
      expect(function() {
        new loop.store.ConversationStore(dispatcher, {
          sdkDriver: sdkDriver
        });
      }).to.Throw(/client/);
    });

    it("should throw an error if the sdkDriver is missing", function() {
      expect(function() {
        new loop.store.ConversationStore(dispatcher, {
          client: client
        });
      }).to.Throw(/sdkDriver/);
    });

    it("should throw an error if mozLoop is missing", function() {
      expect(function() {
        new loop.store.ConversationStore(dispatcher, {
          sdkDriver: sdkDriver,
          client: client
        });
      }).to.Throw(/mozLoop/);
    });
  });

  describe("#connectionFailure", function() {
    beforeEach(function() {
      store._websocket = fakeWebsocket;
      store.setStoreState({windowId: "42"});
    });

    it("should disconnect the session", function() {
      store.connectionFailure(
        new sharedActions.ConnectionFailure({reason: "fake"}));

      sinon.assert.calledOnce(sdkDriver.disconnectSession);
    });

    it("should ensure the websocket is closed", function() {
      store.connectionFailure(
        new sharedActions.ConnectionFailure({reason: "fake"}));

      sinon.assert.calledOnce(wsCloseSpy);
    });

    it("should set the state to 'terminated'", function() {
      store.setStoreState({callState: CALL_STATES.ALERTING});

      store.connectionFailure(
        new sharedActions.ConnectionFailure({reason: "fake"}));

      expect(store.getStoreState("callState")).eql(CALL_STATES.TERMINATED);
      expect(store.getStoreState("callStateReason")).eql("fake");
    });

    it("should release mozLoop callsData", function() {
      store.connectionFailure(
        new sharedActions.ConnectionFailure({reason: "fake"}));

      sinon.assert.calledOnce(fakeMozLoop.calls.clearCallInProgress);
      sinon.assert.calledWithExactly(
        fakeMozLoop.calls.clearCallInProgress, "42");
    });
  });

  describe("#connectionProgress", function() {
    describe("progress: init", function() {
      it("should change the state from 'gather' to 'connecting'", function() {
        store.setStoreState({callState: CALL_STATES.GATHER});

        store.connectionProgress(
          new sharedActions.ConnectionProgress({wsState: WS_STATES.INIT}));

        expect(store.getStoreState("callState")).eql(CALL_STATES.CONNECTING);
      });
    });

    describe("progress: alerting", function() {
      it("should change the state from 'gather' to 'alerting'", function() {
        store.setStoreState({callState: CALL_STATES.GATHER});

        store.connectionProgress(
          new sharedActions.ConnectionProgress({wsState: WS_STATES.ALERTING}));

        expect(store.getStoreState("callState")).eql(CALL_STATES.ALERTING);
      });

      it("should change the state from 'init' to 'alerting'", function() {
        store.setStoreState({callState: CALL_STATES.INIT});

        store.connectionProgress(
          new sharedActions.ConnectionProgress({wsState: WS_STATES.ALERTING}));

        expect(store.getStoreState("callState")).eql(CALL_STATES.ALERTING);
      });
    });

    describe("progress: connecting (outgoing calls)", function() {
      beforeEach(function() {
        store.setStoreState({
          callState: CALL_STATES.ALERTING,
          outgoing: true
        });
      });

      it("should change the state to 'ongoing'", function() {
        store.connectionProgress(
          new sharedActions.ConnectionProgress({wsState: WS_STATES.CONNECTING}));

        expect(store.getStoreState("callState")).eql(CALL_STATES.ONGOING);
      });

      it("should connect the session", function() {
        store.setStoreState(fakeSessionData);

        store.connectionProgress(
          new sharedActions.ConnectionProgress({wsState: WS_STATES.CONNECTING}));

        sinon.assert.calledOnce(sdkDriver.connectSession);
        sinon.assert.calledWithExactly(sdkDriver.connectSession, {
          apiKey: "fakeKey",
          sessionId: "321456",
          sessionToken: "341256",
          sendTwoWayMediaTelemetry: true
        });
      });

      it("should call mozLoop.addConversationContext", function() {
        store.setStoreState(fakeSessionData);

        store.connectionProgress(
          new sharedActions.ConnectionProgress({wsState: WS_STATES.CONNECTING}));

        sinon.assert.calledOnce(fakeMozLoop.addConversationContext);
        sinon.assert.calledWithExactly(fakeMozLoop.addConversationContext,
                                       "28", "321456", "142536");
      });
    });

    describe("progress: connecting (incoming calls)", function() {
      beforeEach(function() {
        store.setStoreState({
          callState: CALL_STATES.ALERTING,
          outgoing: false,
          windowId: 42
        });

        sandbox.stub(console, "error");
        store._websocket = fakeWebsocket;
      });

      it("should log an error", function() {
        store.connectionProgress(
          new sharedActions.ConnectionProgress({wsState: WS_STATES.CONNECTING}));

        sinon.assert.calledOnce(console.error);
      });

      it("should call decline on the websocket", function() {
        store.connectionProgress(
          new sharedActions.ConnectionProgress({wsState: WS_STATES.CONNECTING}));

        sinon.assert.calledOnce(fakeWebsocket.decline);
      });

      it("should close the websocket", function() {
        store.connectionProgress(
          new sharedActions.ConnectionProgress({wsState: WS_STATES.CONNECTING}));

        sinon.assert.calledOnce(fakeWebsocket.close);
      });

      it("should clear the call in progress for the backend", function() {
        store.connectionProgress(
          new sharedActions.ConnectionProgress({wsState: WS_STATES.CONNECTING}));

        sinon.assert.calledOnce(fakeMozLoop.calls.clearCallInProgress);
        sinon.assert.calledWithExactly(fakeMozLoop.calls.clearCallInProgress, 42);
      });

      it("should set the call state to CLOSE", function() {
        store.connectionProgress(
          new sharedActions.ConnectionProgress({wsState: WS_STATES.CONNECTING}));

        expect(store.getStoreState("callState")).eql(CALL_STATES.CLOSE);
      });
    });
  });

  describe("#setupWindowData", function() {
    var fakeSetupWindowData;

    beforeEach(function() {
      store.setStoreState({callState: CALL_STATES.INIT});
      fakeSetupWindowData = {
        windowId: "123456",
        type: "outgoing",
        contact: contact,
        callType: sharedUtils.CALL_TYPES.AUDIO_VIDEO
      };
    });

    it("should set the state to 'gather'", function() {
      dispatcher.dispatch(
        new sharedActions.SetupWindowData(fakeSetupWindowData));

      expect(store.getStoreState("callState")).eql(CALL_STATES.GATHER);
    });

    it("should save the basic call information", function() {
      dispatcher.dispatch(
        new sharedActions.SetupWindowData(fakeSetupWindowData));

      expect(store.getStoreState("windowId")).eql("123456");
      expect(store.getStoreState("outgoing")).eql(true);
    });

    it("should save the basic information from the mozLoop api", function() {
      dispatcher.dispatch(
        new sharedActions.SetupWindowData(fakeSetupWindowData));

      expect(store.getStoreState("contact")).eql(contact);
      expect(store.getStoreState("callType"))
        .eql(sharedUtils.CALL_TYPES.AUDIO_VIDEO);
    });

    describe("incoming calls", function() {
      beforeEach(function() {
        store.setStoreState({outgoing: false});
      });

      it("should initialize the websocket", function() {
        sandbox.stub(loop, "CallConnectionWebSocket").returns({
          promiseConnect: function() { return connectPromise; },
          on: sinon.spy()
        });

        store.connectCall(
          new sharedActions.ConnectCall({sessionData: fakeSessionData}));

        sinon.assert.calledOnce(loop.CallConnectionWebSocket);
        sinon.assert.calledWithExactly(loop.CallConnectionWebSocket, {
          url: "fakeURL",
          callId: "142536",
          websocketToken: "543216"
        });
      });

      it("should connect the websocket to the server", function() {
        store.connectCall(
          new sharedActions.ConnectCall({sessionData: fakeSessionData}));

        sinon.assert.calledOnce(store._websocket.promiseConnect);
      });

      describe("WebSocket connection result", function() {
        beforeEach(function() {
          store.connectCall(
            new sharedActions.ConnectCall({sessionData: fakeSessionData}));

          sandbox.stub(dispatcher, "dispatch");
          // This is already covered by a test. Stub just prevents console msgs.
          sandbox.stub(window.console, "error");
        });

        it("should dispatch a connection progress action on success", function(done) {
          resolveConnectPromise(WS_STATES.INIT);

          connectPromise.then(function() {
            checkFailures(done, function() {
              sinon.assert.calledOnce(dispatcher.dispatch);
              sinon.assert.calledWithExactly(dispatcher.dispatch,
                new sharedActions.ConnectionProgress({
                  wsState: WS_STATES.INIT
                }));
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
              sinon.assert.calledOnce(dispatcher.dispatch,
                new sharedActions.ConnectionFailure({
                  reason: "websocket-setup"
                }));
             });
          });
        });
      });
    });

    describe("outgoing calls", function() {
      it("should request the outgoing call data", function() {
        dispatcher.dispatch(
          new sharedActions.SetupWindowData(fakeSetupWindowData));

        sinon.assert.calledOnce(client.setupOutgoingCall);
        sinon.assert.calledWith(client.setupOutgoingCall,
          ["fakeEmail"], sharedUtils.CALL_TYPES.AUDIO_VIDEO);
      });

      it("should include all email addresses in the call data", function() {
        fakeSetupWindowData.contact = {
          name: [ "Mr Smith" ],
          email: [{
            type: "home",
            value: "fakeEmail",
            pref: true
          },
          {
            type: "work",
            value: "emailFake",
            pref: false
          }]
        };

        dispatcher.dispatch(
          new sharedActions.SetupWindowData(fakeSetupWindowData));

        sinon.assert.calledOnce(client.setupOutgoingCall);
        sinon.assert.calledWith(client.setupOutgoingCall,
          ["fakeEmail", "emailFake"], sharedUtils.CALL_TYPES.AUDIO_VIDEO);
      });

      it("should include trim phone numbers for the call data", function() {
        fakeSetupWindowData.contact = {
          name: [ "Mr Smith" ],
          tel: [{
            type: "home",
            value: "+44-5667+345 496(2335)45+ 456+",
            pref: true
          }]
        };

        dispatcher.dispatch(
          new sharedActions.SetupWindowData(fakeSetupWindowData));

        sinon.assert.calledOnce(client.setupOutgoingCall);
        sinon.assert.calledWith(client.setupOutgoingCall,
          ["+445667345496233545456"], sharedUtils.CALL_TYPES.AUDIO_VIDEO);
      });

      it("should include all email and telephone values in the call data", function() {
        fakeSetupWindowData.contact = {
          name: [ "Mr Smith" ],
          email: [{
            type: "home",
            value: "fakeEmail",
            pref: true
          }, {
            type: "work",
            value: "emailFake",
            pref: false
          }],
          tel: [{
            type: "work",
            value: "01234567890",
            pref: false
          }, {
            type: "home",
            value: "09876543210",
            pref: false
          }]
        };

        dispatcher.dispatch(
          new sharedActions.SetupWindowData(fakeSetupWindowData));

        sinon.assert.calledOnce(client.setupOutgoingCall);
        sinon.assert.calledWith(client.setupOutgoingCall,
          ["fakeEmail", "emailFake", "01234567890", "09876543210"],
          sharedUtils.CALL_TYPES.AUDIO_VIDEO);
      });

      describe("server response handling", function() {
        beforeEach(function() {
          sandbox.stub(dispatcher, "dispatch");
          sandbox.stub(window.console, "error");
        });

        it("should dispatch a connect call action on success", function() {
          var callData = {
            apiKey: "fakeKey"
          };

          client.setupOutgoingCall.callsArgWith(2, null, callData);

          store.setupWindowData(
            new sharedActions.SetupWindowData(fakeSetupWindowData));

          sinon.assert.calledOnce(dispatcher.dispatch);
          // Can't use instanceof here, as that matches any action
          sinon.assert.calledWithMatch(dispatcher.dispatch,
            sinon.match.hasOwn("name", "connectCall"));
          sinon.assert.calledWithMatch(dispatcher.dispatch,
            sinon.match.hasOwn("sessionData", callData));
        });

        it("should dispatch a connection failure action on failure", function() {
          client.setupOutgoingCall.callsArgWith(2, {});

          store.setupWindowData(
            new sharedActions.SetupWindowData(fakeSetupWindowData));

          sinon.assert.calledOnce(dispatcher.dispatch);
          // Can't use instanceof here, as that matches any action
          sinon.assert.calledWithMatch(dispatcher.dispatch,
            sinon.match.hasOwn("name", "connectionFailure"));
          sinon.assert.calledWithMatch(dispatcher.dispatch,
            sinon.match.hasOwn("reason", FAILURE_DETAILS.UNKNOWN));
        });

        it("should dispatch a connection failure action on failure with user unavailable", function() {
          client.setupOutgoingCall.callsArgWith(2, {
            errno: REST_ERRNOS.USER_UNAVAILABLE
          });

          store.setupWindowData(
            new sharedActions.SetupWindowData(fakeSetupWindowData));

          sinon.assert.calledOnce(dispatcher.dispatch);
          // Can't use instanceof here, as that matches any action
          sinon.assert.calledWithMatch(dispatcher.dispatch,
            sinon.match.hasOwn("name", "connectionFailure"));
          sinon.assert.calledWithMatch(dispatcher.dispatch,
            sinon.match.hasOwn("reason", FAILURE_DETAILS.USER_UNAVAILABLE));
        });
      });
    });
  });

  describe("#acceptCall", function() {
    beforeEach(function() {
      store._websocket = {
        accept: sinon.stub()
      };
    });

    it("should save the call type", function() {
      store.acceptCall(
        new sharedActions.AcceptCall({callType: CALL_TYPES.AUDIO_ONLY}));

      expect(store.getStoreState("callType")).eql(CALL_TYPES.AUDIO_ONLY);
      expect(store.getStoreState("videoMuted")).eql(true);
    });

    it("should call accept on the websocket", function() {
      store.acceptCall(
        new sharedActions.AcceptCall({callType: CALL_TYPES.AUDIO_ONLY}));

      sinon.assert.calledOnce(store._websocket.accept);
    });

    it("should change the state to 'ongoing'", function() {
      store.acceptCall(
        new sharedActions.AcceptCall({callType: CALL_TYPES.AUDIO_ONLY}));

      expect(store.getStoreState("callState")).eql(CALL_STATES.ONGOING);
    });

    it("should connect the session with sendTwoWayMediaTelemetry set as falsy", function() {
      store.setStoreState(fakeSessionData);

      store.acceptCall(
        new sharedActions.AcceptCall({callType: CALL_TYPES.AUDIO_ONLY}));

      sinon.assert.calledOnce(sdkDriver.connectSession);
      sinon.assert.calledWithExactly(sdkDriver.connectSession, {
        apiKey: "fakeKey",
        sessionId: "321456",
        sessionToken: "341256",
        sendTwoWayMediaTelemetry: undefined
      });
    });

    it("should call mozLoop.addConversationContext", function() {
      store.setStoreState(fakeSessionData);

      store.acceptCall(
        new sharedActions.AcceptCall({callType: CALL_TYPES.AUDIO_ONLY}));

      sinon.assert.calledOnce(fakeMozLoop.addConversationContext);
      sinon.assert.calledWithExactly(fakeMozLoop.addConversationContext,
                                     "28", "321456", "142536");
    });
  });

  describe("#declineCall", function() {
    beforeEach(function() {
      store._websocket = fakeWebsocket;

      store.setStoreState({windowId: 42});
    });

    it("should block the caller if necessary", function() {
      store.declineCall(new sharedActions.DeclineCall({blockCaller: true}));

      sinon.assert.calledOnce(fakeMozLoop.calls.blockDirectCaller);
    });

    it("should call decline on the websocket", function() {
      store.declineCall(new sharedActions.DeclineCall({blockCaller: false}));

      sinon.assert.calledOnce(fakeWebsocket.decline);
    });

    it("should close the websocket", function() {
      store.declineCall(new sharedActions.DeclineCall({blockCaller: false}));

      sinon.assert.calledOnce(fakeWebsocket.close);
    });

    it("should clear the call in progress for the backend", function() {
      store.declineCall(new sharedActions.DeclineCall({blockCaller: false}));

      sinon.assert.calledOnce(fakeMozLoop.calls.clearCallInProgress);
      sinon.assert.calledWithExactly(fakeMozLoop.calls.clearCallInProgress, 42);
    });

    it("should set the call state to CLOSE", function() {
      store.declineCall(new sharedActions.DeclineCall({blockCaller: false}));

      expect(store.getStoreState("callState")).eql(CALL_STATES.CLOSE);
    });
  });

  describe("#connectCall", function() {
    it("should save the call session data", function() {
      store.connectCall(
        new sharedActions.ConnectCall({sessionData: fakeSessionData}));

      expect(store.getStoreState("apiKey")).eql("fakeKey");
      expect(store.getStoreState("callId")).eql("142536");
      expect(store.getStoreState("sessionId")).eql("321456");
      expect(store.getStoreState("sessionToken")).eql("341256");
      expect(store.getStoreState("websocketToken")).eql("543216");
      expect(store.getStoreState("progressURL")).eql("fakeURL");
    });

    it("should initialize the websocket", function() {
      sandbox.stub(loop, "CallConnectionWebSocket").returns({
        promiseConnect: function() { return connectPromise; },
        on: sinon.spy()
      });

      store.connectCall(
        new sharedActions.ConnectCall({sessionData: fakeSessionData}));

      sinon.assert.calledOnce(loop.CallConnectionWebSocket);
      sinon.assert.calledWithExactly(loop.CallConnectionWebSocket, {
        url: "fakeURL",
        callId: "142536",
        websocketToken: "543216"
      });
    });

    it("should connect the websocket to the server", function() {
      store.connectCall(
        new sharedActions.ConnectCall({sessionData: fakeSessionData}));

      sinon.assert.calledOnce(store._websocket.promiseConnect);
    });

    describe("WebSocket connection result", function() {
      beforeEach(function() {
        sandbox.stub(window.console, "error");
        store.connectCall(
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

      it("should log an error when connection fails", function(done) {
        rejectConnectPromise();

        connectPromise.then(function() {
          done(new Error("Promise not reject"));
        }, function() {
          checkFailures(done, function() {
            sinon.assert.calledOnce(console.error);
          });
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
    var wsMediaFailSpy, wsHangupSpy;
    beforeEach(function() {
      wsMediaFailSpy = sinon.spy();
      wsHangupSpy = sinon.spy();

      store._websocket = {
        mediaFail: wsMediaFailSpy,
        close: wsHangupSpy
      };
      store.setStoreState({callState: CALL_STATES.ONGOING});
      store.setStoreState({windowId: "42"});
    });

    it("should disconnect the session", function() {
      store.hangupCall(new sharedActions.HangupCall());

      sinon.assert.calledOnce(sdkDriver.disconnectSession);
    });

    it("should send a media-fail message to the websocket if it is open", function() {
      store.hangupCall(new sharedActions.HangupCall());

      sinon.assert.calledOnce(wsMediaFailSpy);
    });

    it("should ensure the websocket is closed", function() {
      store.hangupCall(new sharedActions.HangupCall());

      sinon.assert.calledOnce(wsHangupSpy);
    });

    it("should set the callState to finished", function() {
      store.hangupCall(new sharedActions.HangupCall());

      expect(store.getStoreState("callState")).eql(CALL_STATES.FINISHED);
    });

    it("should release mozLoop callsData", function() {
      store.hangupCall(new sharedActions.HangupCall());

      sinon.assert.calledOnce(fakeMozLoop.calls.clearCallInProgress);
      sinon.assert.calledWithExactly(
        fakeMozLoop.calls.clearCallInProgress, "42");
    });
  });

  describe("#remotePeerDisconnected", function() {
    var wsMediaFailSpy, wsDisconnectSpy;
    beforeEach(function() {
      wsMediaFailSpy = sinon.spy();
      wsDisconnectSpy = sinon.spy();

      store._websocket = {
        mediaFail: wsMediaFailSpy,
        close: wsDisconnectSpy
      };
      store.setStoreState({callState: CALL_STATES.ONGOING});
      store.setStoreState({windowId: "42"});
    });

    it("should disconnect the session", function() {
      store.remotePeerDisconnected(new sharedActions.RemotePeerDisconnected({
        peerHungup: true
      }));

      sinon.assert.calledOnce(sdkDriver.disconnectSession);
    });

    it("should ensure the websocket is closed", function() {
      store.remotePeerDisconnected(new sharedActions.RemotePeerDisconnected({
        peerHungup: true
      }));

      sinon.assert.calledOnce(wsDisconnectSpy);
    });

    it("should release mozLoop callsData", function() {
      store.remotePeerDisconnected(new sharedActions.RemotePeerDisconnected({
        peerHungup: true
      }));

      sinon.assert.calledOnce(fakeMozLoop.calls.clearCallInProgress);
      sinon.assert.calledWithExactly(
        fakeMozLoop.calls.clearCallInProgress, "42");
    });

    it("should set the callState to finished if the peer hungup", function() {
      store.remotePeerDisconnected(new sharedActions.RemotePeerDisconnected({
        peerHungup: true
      }));

      expect(store.getStoreState("callState")).eql(CALL_STATES.FINISHED);
    });

    it("should set the callState to terminated if the peer was disconnected" +
      "for an unintentional reason", function() {
        store.remotePeerDisconnected(new sharedActions.RemotePeerDisconnected({
          peerHungup: false
        }));

        expect(store.getStoreState("callState")).eql(CALL_STATES.TERMINATED);
      });

    it("should set the reason to peerNetworkDisconnected if the peer was" +
      "disconnected for an unintentional reason", function() {
        store.remotePeerDisconnected(new sharedActions.RemotePeerDisconnected({
          peerHungup: false
        }));

        expect(store.getStoreState("callStateReason"))
          .eql("peerNetworkDisconnected");
    });
  });

  describe("#cancelCall", function() {
    beforeEach(function() {
      store._websocket = fakeWebsocket;

      store.setStoreState({callState: CALL_STATES.CONNECTING});
      store.setStoreState({windowId: "42"});
    });

    it("should disconnect the session", function() {
      store.cancelCall(new sharedActions.CancelCall());

      sinon.assert.calledOnce(sdkDriver.disconnectSession);
    });

    it("should send a cancel message to the websocket if it is open for outgoing calls", function() {
      store.setStoreState({outgoing: true});

      store.cancelCall(new sharedActions.CancelCall());

      sinon.assert.calledOnce(wsCancelSpy);
    });

    it("should ensure the websocket is closed", function() {
      store.cancelCall(new sharedActions.CancelCall());

      sinon.assert.calledOnce(wsCloseSpy);
    });

    it("should set the state to close if the call is connecting", function() {
      store.cancelCall(new sharedActions.CancelCall());

      expect(store.getStoreState("callState")).eql(CALL_STATES.CLOSE);
    });

    it("should set the state to close if the call has terminated already", function() {
      store.setStoreState({callState: CALL_STATES.TERMINATED});

      store.cancelCall(new sharedActions.CancelCall());

      expect(store.getStoreState("callState")).eql(CALL_STATES.CLOSE);
    });

    it("should release mozLoop callsData", function() {
      store.cancelCall(new sharedActions.CancelCall());

      sinon.assert.calledOnce(fakeMozLoop.calls.clearCallInProgress);
      sinon.assert.calledWithExactly(
        fakeMozLoop.calls.clearCallInProgress, "42");
    });
  });

  describe("#retryCall", function() {
    it("should set the state to gather", function() {
      store.setStoreState({callState: CALL_STATES.TERMINATED});

      store.retryCall(new sharedActions.RetryCall());

      expect(store.getStoreState("callState"))
        .eql(CALL_STATES.GATHER);
    });

    it("should request the outgoing call data", function() {
      store.setStoreState({
        callState: CALL_STATES.TERMINATED,
        outgoing: true,
        callType: sharedUtils.CALL_TYPES.AUDIO_VIDEO,
        contact: contact
      });

      store.retryCall(new sharedActions.RetryCall());

      sinon.assert.calledOnce(client.setupOutgoingCall);
      sinon.assert.calledWith(client.setupOutgoingCall,
        ["fakeEmail"], sharedUtils.CALL_TYPES.AUDIO_VIDEO);
    });
  });

  describe("#mediaConnected", function() {
    it("should send mediaUp via the websocket", function() {
      store._websocket = fakeWebsocket;

      store.mediaConnected(new sharedActions.MediaConnected());

      sinon.assert.calledOnce(wsMediaUpSpy);
    });

    it("should set store.mediaConnected to true", function () {
      store._websocket = fakeWebsocket;

      store.mediaConnected(new sharedActions.MediaConnected());

      expect(store.getStoreState("mediaConnected")).eql(true);
    });
  });

  describe("#mediaStreamCreated", function() {
    it("should add a local video object to the store", function() {
      expect(store.getStoreState()).to.not.have.property("localSrcVideoObject");

      store.mediaStreamCreated(new sharedActions.MediaStreamCreated({
        hasVideo: false,
        isLocal: true,
        srcVideoObject: fakeVideoElement
      }));

      expect(store.getStoreState().localSrcVideoObject).eql(fakeVideoElement);
      expect(store.getStoreState()).to.not.have.property("remoteSrcVideoObject");
    });

    it("should set the local video enabled", function() {
      store.setStoreState({
        localVideoEnabled: false,
        remoteVideoEnabled: false
      });

      store.mediaStreamCreated(new sharedActions.MediaStreamCreated({
        hasVideo: true,
        isLocal: true,
        srcVideoObject: fakeVideoElement
      }));

      expect(store.getStoreState().localVideoEnabled).eql(true);
      expect(store.getStoreState().remoteVideoEnabled).eql(false);
    });

    it("should add a remote video object to the store", function() {
      expect(store.getStoreState()).to.not.have.property("remoteSrcVideoObject");

      store.mediaStreamCreated(new sharedActions.MediaStreamCreated({
        hasVideo: false,
        isLocal: false,
        srcVideoObject: fakeVideoElement
      }));

      expect(store.getStoreState()).not.have.property("localSrcVideoObject");
      expect(store.getStoreState().remoteSrcVideoObject).eql(fakeVideoElement);
    });

    it("should set the remote video enabled", function() {
      store.setStoreState({
        localVideoEnabled: false,
        remoteVideoEnabled: false
      });

      store.mediaStreamCreated(new sharedActions.MediaStreamCreated({
        hasVideo: true,
        isLocal: false,
        srcVideoObject: fakeVideoElement
      }));

      expect(store.getStoreState().localVideoEnabled).eql(false);
      expect(store.getStoreState().remoteVideoEnabled).eql(true);
    });
  });

  describe("#mediaStreamDestroyed", function() {
    beforeEach(function() {
      store.setStoreState({
        localSrcVideoObject: fakeVideoElement,
        remoteSrcVideoObject: fakeVideoElement
      });
    });

    it("should clear the local video object", function() {
      store.mediaStreamDestroyed(new sharedActions.MediaStreamDestroyed({
        isLocal: true
      }));

      expect(store.getStoreState().localSrcVideoObject).eql(null);
      expect(store.getStoreState().remoteSrcVideoObject).eql(fakeVideoElement);
    });

    it("should clear the remote video object", function() {
      store.mediaStreamDestroyed(new sharedActions.MediaStreamDestroyed({
        isLocal: false
      }));

      expect(store.getStoreState().localSrcVideoObject).eql(fakeVideoElement);
      expect(store.getStoreState().remoteSrcVideoObject).eql(null);
    });
  });

  describe("#remoteVideoStatus", function() {
    it("should set remoteVideoEnabled to true", function() {
      store.setStoreState({
        remoteVideoEnabled: false
      });

      store.remoteVideoStatus(new sharedActions.RemoteVideoStatus({
        videoEnabled: true
      }));

      expect(store.getStoreState().remoteVideoEnabled).eql(true);
    });

    it("should set remoteVideoEnabled to false", function() {
      store.setStoreState({
        remoteVideoEnabled: true
      });

      store.remoteVideoStatus(new sharedActions.RemoteVideoStatus({
        videoEnabled: false
      }));

      expect(store.getStoreState().remoteVideoEnabled).eql(false);
    });
  });

  describe("#setMute", function() {
    beforeEach(function() {
      dispatcher.dispatch(
        // Setup store to prevent console warnings.
        new sharedActions.SetupWindowData({
          windowId: "123456",
          type: "outgoing",
          contact: contact,
          callType: sharedUtils.CALL_TYPES.AUDIO_VIDEO
        }));
    });

    it("should save the mute state for the audio stream", function() {
      store.setStoreState({"audioMuted": false});

      dispatcher.dispatch(new sharedActions.SetMute({
        type: "audio",
        enabled: true
      }));

      expect(store.getStoreState("audioMuted")).eql(false);
    });

    it("should save the mute state for the video stream", function() {
      store.setStoreState({"videoMuted": true});

      dispatcher.dispatch(new sharedActions.SetMute({
        type: "video",
        enabled: false
      }));

      expect(store.getStoreState("videoMuted")).eql(true);
    });
  });

  describe("#fetchRoomEmailLink", function() {
    it("should request a new call url to the server", function() {
      store.fetchRoomEmailLink(new sharedActions.FetchRoomEmailLink({
        roomName: "FakeRoomName"
      }));

      sinon.assert.calledOnce(fakeMozLoop.rooms.create);
      sinon.assert.calledWithMatch(fakeMozLoop.rooms.create, {
        decryptedContext: {
          roomName: "FakeRoomName"
        }
      });
    });

    it("should update the emailLink attribute when the new room url is received",
      function() {
        fakeMozLoop.rooms.create = function(roomData, cb) {
          cb(null, {roomUrl: "http://fake.invalid/"});
        };
        store.fetchRoomEmailLink(new sharedActions.FetchRoomEmailLink({
          roomName: "FakeRoomName"
        }));

        expect(store.getStoreState("emailLink")).eql("http://fake.invalid/");
        sinon.assert.calledOnce(fakeMozLoop.telemetryAddValue);
        sinon.assert.calledWithExactly(fakeMozLoop.telemetryAddValue, "LOOP_ROOM_CREATE", 0);
      });

    it("should trigger an error:emailLink event in case of failure",
      function() {
        var trigger = sandbox.stub(store, "trigger");
        fakeMozLoop.rooms.create = function(roomData, cb) {
          cb(new Error("error"));
        };
        store.fetchRoomEmailLink(new sharedActions.FetchRoomEmailLink({
          roomName: "FakeRoomName"
        }));

        sinon.assert.calledOnce(trigger);
        sinon.assert.calledWithExactly(trigger, "error:emailLink");
        sinon.assert.calledOnce(fakeMozLoop.telemetryAddValue);
        sinon.assert.calledWithExactly(fakeMozLoop.telemetryAddValue, "LOOP_ROOM_CREATE", 1);
      });
  });

  describe("#windowUnload", function() {
    var fakeWs;

    beforeEach(function() {
      fakeWs = store._websocket = {
        close: sinon.stub(),
        decline: sinon.stub()
      };

      store.setStoreState({windowId: 42});
    });

    it("should decline the connection on the websocket for incoming calls if the state is alerting", function() {
      store.setStoreState({
        callState: CALL_STATES.ALERTING,
        outgoing: false
      });

      store.windowUnload();

      sinon.assert.calledOnce(fakeWs.decline);
    });

    it("should disconnect the sdk session", function() {
      store.windowUnload();

      sinon.assert.calledOnce(sdkDriver.disconnectSession);
    });

    it("should close the websocket", function() {
      store.windowUnload();

      sinon.assert.calledOnce(fakeWs.close);
    });

    it("should clear the call in progress for the backend", function() {
      store.windowUnload();

      sinon.assert.calledOnce(fakeMozLoop.calls.clearCallInProgress);
      sinon.assert.calledWithExactly(fakeMozLoop.calls.clearCallInProgress, 42);
    });
  });

  describe("Events", function() {
    describe("Websocket progress", function() {
      beforeEach(function() {
        store.connectCall(
          new sharedActions.ConnectCall({sessionData: fakeSessionData}));

        sandbox.stub(dispatcher, "dispatch");
      });

      it("should dispatch a connection failure action on 'terminate' for incoming calls if the previous state was not 'alerting' or 'init'", function() {
        store.setStoreState({
          outgoing: false
        });

        store._websocket.trigger("progress", {
          state: WS_STATES.TERMINATED,
          reason: WEBSOCKET_REASONS.CANCEL
        }, WS_STATES.CONNECTING);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.ConnectionFailure({
            reason: WEBSOCKET_REASONS.CANCEL
          }));
      });

      it("should dispatch a cancel call action on 'terminate' for incoming calls if the previous state was 'init'", function() {
        store.setStoreState({
          outgoing: false
        });

        store._websocket.trigger("progress", {
          state: WS_STATES.TERMINATED,
          reason: WEBSOCKET_REASONS.CANCEL
        }, WS_STATES.INIT);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.CancelCall({}));
      });

      it("should dispatch a cancel call action on 'terminate' for incoming calls if the previous state was 'alerting'", function() {
        store.setStoreState({
          outgoing: false
        });

        store._websocket.trigger("progress", {
          state: WS_STATES.TERMINATED,
          reason: WEBSOCKET_REASONS.CANCEL
        }, WS_STATES.ALERTING);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.CancelCall({}));
      });

      it("should dispatch a connection progress action on 'alerting'", function() {
        store._websocket.trigger("progress", {state: WS_STATES.ALERTING});

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.ConnectionProgress({
            wsState: WS_STATES.ALERTING
          }));
      });

      describe("Outgoing Calls, handling 'terminate'", function() {
        beforeEach(function() {
          store.setStoreState({
            outgoing: true
          });
        });

        it("should dispatch a connection failure action", function() {
          store._websocket.trigger("progress", {
            state: WS_STATES.TERMINATED,
            reason: WEBSOCKET_REASONS.MEDIA_FAIL
          });

          sinon.assert.calledOnce(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.ConnectionFailure({
              reason: WEBSOCKET_REASONS.MEDIA_FAIL
            }));
        });

        it("should dispatch an action with user unavailable if the websocket reports busy", function() {
          store._websocket.trigger("progress", {
            state: WS_STATES.TERMINATED,
            reason: WEBSOCKET_REASONS.BUSY
          });

          sinon.assert.calledOnce(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.ConnectionFailure({
              reason: FAILURE_DETAILS.USER_UNAVAILABLE
            }));
        });

        it("should dispatch an action with user unavailable if the websocket reports reject", function() {
          store._websocket.trigger("progress", {
            state: WS_STATES.TERMINATED,
            reason: WEBSOCKET_REASONS.REJECT
          });

          sinon.assert.calledOnce(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.ConnectionFailure({
              reason: FAILURE_DETAILS.USER_UNAVAILABLE
            }));
        });
      });
    });
  });
});
