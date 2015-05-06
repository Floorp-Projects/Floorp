/* global chai, loop */

var expect = chai.expect;
var sharedActions = loop.shared.actions;

describe("loop.store.ActiveRoomStore", function () {
  "use strict";

  var REST_ERRNOS = loop.shared.utils.REST_ERRNOS;
  var ROOM_STATES = loop.store.ROOM_STATES;
  var FAILURE_DETAILS = loop.shared.utils.FAILURE_DETAILS;
  var SCREEN_SHARE_STATES = loop.shared.utils.SCREEN_SHARE_STATES;
  var ROOM_INFO_FAILURES = loop.shared.utils.ROOM_INFO_FAILURES;
  var sandbox, dispatcher, store, fakeMozLoop, fakeSdkDriver;
  var fakeMultiplexGum;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    sandbox.useFakeTimers();

    dispatcher = new loop.Dispatcher();
    sandbox.stub(dispatcher, "dispatch");

    fakeMozLoop = {
      setLoopPref: sinon.stub(),
      addConversationContext: sinon.stub(),
      addBrowserSharingListener: sinon.stub(),
      removeBrowserSharingListener: sinon.stub(),
      rooms: {
        get: sinon.stub(),
        join: sinon.stub(),
        refreshMembership: sinon.stub(),
        leave: sinon.stub(),
        on: sinon.stub(),
        off: sinon.stub(),
        sendConnectionStatus: sinon.stub()
      },
      setScreenShareState: sinon.stub(),
      getActiveTabWindowId: sandbox.stub().callsArgWith(0, null, 42),
      isSocialShareButtonAvailable: sinon.stub().returns(false),
      getSocialShareProviders: sinon.stub().returns([])
    };

    fakeSdkDriver = {
      connectSession: sinon.stub(),
      disconnectSession: sinon.stub(),
      forceDisconnectAll: sinon.stub().callsArg(0),
      retryPublishWithoutVideo: sinon.stub(),
      startScreenShare: sinon.stub(),
      switchAcquiredWindow: sinon.stub(),
      endScreenShare: sinon.stub().returns(true)
    };

    fakeMultiplexGum = {
        reset: sandbox.spy()
    };

    loop.standaloneMedia = {
      multiplexGum: fakeMultiplexGum
    };

    store = new loop.store.ActiveRoomStore(dispatcher, {
      mozLoop: fakeMozLoop,
      sdkDriver: fakeSdkDriver
    });
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("#constructor", function() {
    it("should throw an error if mozLoop is missing", function() {
      expect(function() {
        new loop.store.ActiveRoomStore(dispatcher);
      }).to.Throw(/mozLoop/);
    });

    it("should throw an error if sdkDriver is missing", function() {
      expect(function() {
        new loop.store.ActiveRoomStore(dispatcher, {mozLoop: {}});
      }).to.Throw(/sdkDriver/);
    });
  });

  describe("#roomFailure", function() {
    var fakeError;

    beforeEach(function() {
      sandbox.stub(console, "error");

      fakeError = new Error("fake");

      store.setStoreState({
        roomState: ROOM_STATES.JOINED,
        roomToken: "fakeToken",
        sessionToken: "1627384950"
      });
    });

    it("should log the error", function() {
      store.roomFailure(new sharedActions.RoomFailure({
        error: fakeError,
        failedJoinRequest: false
      }));

      sinon.assert.calledOnce(console.error);
      sinon.assert.calledWith(console.error,
        sinon.match(ROOM_STATES.JOINED), fakeError);
    });

    it("should set the state to `FULL` on server error room full", function() {
      fakeError.errno = REST_ERRNOS.ROOM_FULL;

      store.roomFailure(new sharedActions.RoomFailure({
        error: fakeError,
        failedJoinRequest: false
      }));

      expect(store._storeState.roomState).eql(ROOM_STATES.FULL);
    });

    it("should set the state to `FAILED` on generic error", function() {
      store.roomFailure(new sharedActions.RoomFailure({
        error: fakeError,
        failedJoinRequest: false
      }));

      expect(store._storeState.roomState).eql(ROOM_STATES.FAILED);
      expect(store._storeState.failureReason).eql(FAILURE_DETAILS.UNKNOWN);
    });

    it("should set the failureReason to EXPIRED_OR_INVALID on server error: " +
      "invalid token", function() {
        fakeError.errno = REST_ERRNOS.INVALID_TOKEN;

        store.roomFailure(new sharedActions.RoomFailure({
          error: fakeError,
          failedJoinRequest: false
        }));

        expect(store._storeState.roomState).eql(ROOM_STATES.FAILED);
        expect(store._storeState.failureReason).eql(FAILURE_DETAILS.EXPIRED_OR_INVALID);
      });

    it("should set the failureReason to EXPIRED_OR_INVALID on server error: " +
      "expired", function() {
        fakeError.errno = REST_ERRNOS.EXPIRED;

        store.roomFailure(new sharedActions.RoomFailure({
          error: fakeError,
          failedJoinRequest: false
        }));

        expect(store._storeState.roomState).eql(ROOM_STATES.FAILED);
        expect(store._storeState.failureReason).eql(FAILURE_DETAILS.EXPIRED_OR_INVALID);
      });

    it("should reset the multiplexGum", function() {
      store.roomFailure(new sharedActions.RoomFailure({
        error: fakeError,
        failedJoinRequest: false
      }));

      sinon.assert.calledOnce(fakeMultiplexGum.reset);
    });

    it("should set screen sharing inactive", function() {
      store.setStoreState({windowId: "1234"});

      store.roomFailure(new sharedActions.RoomFailure({
        error: fakeError,
        failedJoinRequest: false
      }));

      sinon.assert.calledOnce(fakeMozLoop.setScreenShareState);
      sinon.assert.calledWithExactly(fakeMozLoop.setScreenShareState, "1234", false);
    });

    it("should disconnect from the servers via the sdk", function() {
      store.roomFailure(new sharedActions.RoomFailure({
        error: fakeError,
        failedJoinRequest: false
      }));

      sinon.assert.calledOnce(fakeSdkDriver.disconnectSession);
    });

    it("should clear any existing timeout", function() {
      sandbox.stub(window, "clearTimeout");
      store._timeout = {};

      store.roomFailure(new sharedActions.RoomFailure({
        error: fakeError,
        failedJoinRequest: false
      }));

      sinon.assert.calledOnce(clearTimeout);
    });

    it("should remove the sharing listener", function() {
      // Setup the listener.
      store.startScreenShare(new sharedActions.StartScreenShare({
        type: "browser"
      }));

      // Now simulate room failure.
      store.roomFailure(new sharedActions.RoomFailure({
        error: fakeError,
        failedJoinRequest: false
      }));

      sinon.assert.calledOnce(fakeMozLoop.removeBrowserSharingListener);
    });

    it("should call mozLoop.rooms.leave", function() {
      store.roomFailure(new sharedActions.RoomFailure({
        error: fakeError,
        failedJoinRequest: false
      }));

      sinon.assert.calledOnce(fakeMozLoop.rooms.leave);
      sinon.assert.calledWithExactly(fakeMozLoop.rooms.leave,
        "fakeToken", "1627384950");
    });

    it("should not call mozLoop.rooms.leave if failedJoinRequest is true", function() {
      store.roomFailure(new sharedActions.RoomFailure({
        error: fakeError,
        failedJoinRequest: true
      }));

      sinon.assert.notCalled(fakeMozLoop.rooms.leave);
    });
  });

  describe("#setupWindowData", function() {
    var fakeToken, fakeRoomData;

    beforeEach(function() {
      fakeToken = "337-ff-54";
      fakeRoomData = {
        decryptedContext: {
          roomName: "Monkeys"
        },
        roomOwner: "Alfred",
        roomUrl: "http://invalid"
      };

      store = new loop.store.ActiveRoomStore(dispatcher, {
        mozLoop: fakeMozLoop,
        sdkDriver: {}
      });
      fakeMozLoop.rooms.get.
        withArgs(fakeToken).
        callsArgOnWith(1, // index of callback argument
        store, // |this| to call it on
        null, // args to call the callback with...
        fakeRoomData
      );
    });

    it("should set the state to `GATHER`",
      function() {
        store.setupWindowData(new sharedActions.SetupWindowData({
          windowId: "42",
          type: "room",
          roomToken: fakeToken
        }));

        expect(store.getStoreState()).
          to.have.property('roomState', ROOM_STATES.GATHER);
      });

    it("should dispatch an SetupRoomInfo action if the get is successful",
      function() {
        store.setupWindowData(new sharedActions.SetupWindowData({
          windowId: "42",
          type: "room",
          roomToken: fakeToken
        }));

        sinon.assert.calledTwice(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.SetupRoomInfo({
            roomContextUrls: undefined,
            roomDescription: undefined,
            roomToken: fakeToken,
            roomName: fakeRoomData.decryptedContext.roomName,
            roomOwner: fakeRoomData.roomOwner,
            roomUrl: fakeRoomData.roomUrl,
            socialShareButtonAvailable: false,
            socialShareProviders: []
          }));
      });

    it("should dispatch a JoinRoom action if the get is successful",
      function() {
        store.setupWindowData(new sharedActions.SetupWindowData({
          windowId: "42",
          type: "room",
          roomToken: fakeToken
        }));

        sinon.assert.calledTwice(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.JoinRoom());
      });

    it("should dispatch a RoomFailure action if the get fails",
      function() {

        var fakeError = new Error("fake error");
        fakeMozLoop.rooms.get.
          withArgs(fakeToken).
          callsArgOnWith(1, // index of callback argument
          store, // |this| to call it on
          fakeError); // args to call the callback with...

        store.setupWindowData(new sharedActions.SetupWindowData({
          windowId: "42",
          type: "room",
          roomToken: fakeToken
        }));

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.RoomFailure({
            error: fakeError,
            failedJoinRequest: false
          }));
      });
  });

  describe("#fetchServerData", function() {
    var fetchServerAction;

    beforeEach(function() {
      fetchServerAction = new sharedActions.FetchServerData({
        windowType: "room",
        token: "fakeToken"
      });
    });

    it("should save the token", function() {
      store.fetchServerData(fetchServerAction);

      expect(store.getStoreState().roomToken).eql("fakeToken");
    });

    it("should set the state to `READY`", function() {
      store.fetchServerData(fetchServerAction);

      expect(store.getStoreState().roomState).eql(ROOM_STATES.READY);
    });

    it("should call mozLoop.rooms.get to get the room data", function() {
      store.fetchServerData(fetchServerAction);

      sinon.assert.calledOnce(fakeMozLoop.rooms.get);
    });

    it("should dispatch an UpdateRoomInfo message with 'no data' failure if neither roomName nor context are supplied", function() {
      fakeMozLoop.rooms.get.callsArgWith(1, null, {
        roomOwner: "Dan",
        roomUrl: "http://invalid"
      });

      store.fetchServerData(fetchServerAction);

      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWithExactly(dispatcher.dispatch,
        new sharedActions.UpdateRoomInfo({
          roomInfoFailure: ROOM_INFO_FAILURES.NO_DATA,
          roomOwner: "Dan",
          roomUrl: "http://invalid"
        }));
    });

    describe("mozLoop.rooms.get returns roomName as a separate field (no context)", function() {
      it("should dispatch UpdateRoomInfo if mozLoop.rooms.get is successful", function() {
        var roomDetails = {
          roomName: "fakeName",
          roomUrl: "http://invalid",
          roomOwner: "gavin"
        };

        fakeMozLoop.rooms.get.callsArgWith(1, null, roomDetails);

        store.fetchServerData(fetchServerAction);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.UpdateRoomInfo(roomDetails));
      });
    });

    describe("mozLoop.rooms.get returns encryptedContext", function() {
      var roomDetails, expectedDetails;

      beforeEach(function() {
        roomDetails = {
          context: {
            value: "fakeContext"
          },
          roomUrl: "http://invalid",
          roomOwner: "Mark"
        };
        expectedDetails = {
          roomUrl: "http://invalid",
          roomOwner: "Mark"
        };

        fakeMozLoop.rooms.get.callsArgWith(1, null, roomDetails);

        sandbox.stub(loop.crypto, "isSupported").returns(true);
      });

      it("should dispatch UpdateRoomInfo message with 'unsupported' failure if WebCrypto is unsupported", function() {
        loop.crypto.isSupported.returns(false);

        store.fetchServerData(fetchServerAction);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.UpdateRoomInfo(_.extend({
            roomInfoFailure: ROOM_INFO_FAILURES.WEB_CRYPTO_UNSUPPORTED
          }, expectedDetails)));
      });

      it("should dispatch UpdateRoomInfo message with 'no crypto key' failure if there is no crypto key", function() {
        store.fetchServerData(fetchServerAction);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.UpdateRoomInfo(_.extend({
            roomInfoFailure: ROOM_INFO_FAILURES.NO_CRYPTO_KEY
          }, expectedDetails)));
      });

      it("should dispatch UpdateRoomInfo message with 'decrypt failed' failure if decryption failed", function() {
        fetchServerAction.cryptoKey = "fakeKey";

        // This is a work around to turn promise into a sync action to make handling test failures
        // easier.
        sandbox.stub(loop.crypto, "decryptBytes", function() {
          return {
            then: function(resolve, reject) {
              reject(new Error("Operation unsupported"));
            }
          };
        });

        store.fetchServerData(fetchServerAction);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.UpdateRoomInfo(_.extend({
            roomInfoFailure: ROOM_INFO_FAILURES.DECRYPT_FAILED
          }, expectedDetails)));
      });

      it("should dispatch UpdateRoomInfo message with the context if decryption was successful", function() {
        fetchServerAction.cryptoKey = "fakeKey";

        var roomContext = {
          description: "Never gonna let you down. Never gonna give you up...",
          roomName: "The wonderful Loopy room",
          urls: [{
            description: "An invalid page",
            location: "http://invalid.com",
            thumbnail: "data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
          }]
        };

        // This is a work around to turn promise into a sync action to make handling test failures
        // easier.
        sandbox.stub(loop.crypto, "decryptBytes", function() {
          return {
            then: function(resolve, reject) {
              resolve(JSON.stringify(roomContext));
            }
          };
        });

        store.fetchServerData(fetchServerAction);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.UpdateRoomInfo(_.extend(roomContext, expectedDetails)));
      });
    });
  });

  describe("#feedbackComplete", function() {
    it("should reset the room store state", function() {
      var initialState = store.getInitialStoreState();
      store.setStoreState({
        roomState: ROOM_STATES.ENDED,
        audioMuted: true,
        videoMuted: true,
        failureReason: "foo"
      });

      store.feedbackComplete(new sharedActions.FeedbackComplete());

      expect(store.getStoreState()).eql(initialState);
    });
  });

  describe("#videoDimensionsChanged", function() {
    it("should not contain any video dimensions at the very start", function() {
      expect(store.getStoreState()).eql(store.getInitialStoreState());
    });

    it("should update the store with new video dimensions", function() {
      var actionData = {
        isLocal: true,
        videoType: "camera",
        dimensions: { width: 640, height: 480 }
      };

      store.videoDimensionsChanged(new sharedActions.VideoDimensionsChanged(actionData));

      expect(store.getStoreState().localVideoDimensions)
        .to.have.property(actionData.videoType, actionData.dimensions);

      actionData.isLocal = false;
      store.videoDimensionsChanged(new sharedActions.VideoDimensionsChanged(actionData));

      expect(store.getStoreState().remoteVideoDimensions)
        .to.have.property(actionData.videoType, actionData.dimensions);
    });
  });

  describe("#setupRoomInfo", function() {
    var fakeRoomInfo;

    beforeEach(function() {
      fakeRoomInfo = {
        roomName: "Its a room",
        roomOwner: "Me",
        roomToken: "fakeToken",
        roomUrl: "http://invalid",
        socialShareButtonAvailable: false,
        socialShareProviders: []
      };
    });

    it("should set the state to READY", function() {
      store.setupRoomInfo(new sharedActions.SetupRoomInfo(fakeRoomInfo));

      expect(store._storeState.roomState).eql(ROOM_STATES.READY);
    });

    it("should save the room information", function() {
      store.setupRoomInfo(new sharedActions.SetupRoomInfo(fakeRoomInfo));

      var state = store.getStoreState();
      expect(state.roomName).eql(fakeRoomInfo.roomName);
      expect(state.roomOwner).eql(fakeRoomInfo.roomOwner);
      expect(state.roomToken).eql(fakeRoomInfo.roomToken);
      expect(state.roomUrl).eql(fakeRoomInfo.roomUrl);
      expect(state.socialShareButtonAvailable).eql(false);
      expect(state.socialShareProviders).eql([]);
    });
  });

  describe("#updateRoomInfo", function() {
    var fakeRoomInfo;

    beforeEach(function() {
      fakeRoomInfo = {
        roomName: "Its a room",
        roomOwner: "Me",
        roomUrl: "http://invalid",
        urls: [{
          description: "fake site",
          location: "http://invalid.com",
          thumbnail: "data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
        }]
      };
    });

    it("should save the room information", function() {
      store.updateRoomInfo(new sharedActions.UpdateRoomInfo(fakeRoomInfo));

      var state = store.getStoreState();
      expect(state.roomName).eql(fakeRoomInfo.roomName);
      expect(state.roomOwner).eql(fakeRoomInfo.roomOwner);
      expect(state.roomUrl).eql(fakeRoomInfo.roomUrl);
      expect(state.roomContextUrls).eql(fakeRoomInfo.urls);
    });
  });

  describe("#updateSocialShareInfo", function() {
    var fakeSocialShareInfo;

    beforeEach(function() {
      fakeSocialShareInfo = {
        socialShareButtonAvailable: true,
        socialShareProviders: [{
          name: "foo",
          origin: "https://example.com",
          iconURL: "icon.png"
        }]
      };
    });

    it("should save the Social API information", function() {
      store.updateSocialShareInfo(new sharedActions.UpdateSocialShareInfo(fakeSocialShareInfo));

      var state = store.getStoreState();
      expect(state.socialShareButtonAvailable)
        .eql(fakeSocialShareInfo.socialShareButtonAvailable);
      expect(state.socialShareProviders)
        .eql(fakeSocialShareInfo.socialShareProviders);
    });
  });

  describe("#joinRoom", function() {
    it("should reset failureReason", function() {
      store.setStoreState({failureReason: "Test"});

      store.joinRoom();

      expect(store.getStoreState().failureReason).eql(undefined);
    });

    it("should set the state to MEDIA_WAIT", function() {
      store.setStoreState({roomState: ROOM_STATES.READY});

      store.joinRoom();

      expect(store.getStoreState().roomState).eql(ROOM_STATES.MEDIA_WAIT);
    });
  });

  describe("#gotMediaPermission", function() {
    beforeEach(function() {
      store.setStoreState({roomToken: "tokenFake"});
    });

    it("should set the room state to JOINING", function() {
      store.gotMediaPermission();

      expect(store.getStoreState().roomState).eql(ROOM_STATES.JOINING);
    });

    it("should call rooms.join on mozLoop", function() {
      store.gotMediaPermission();

      sinon.assert.calledOnce(fakeMozLoop.rooms.join);
      sinon.assert.calledWith(fakeMozLoop.rooms.join, "tokenFake");
    });

    it("should dispatch `JoinedRoom` on success", function() {
      var responseData = {
        apiKey: "keyFake",
        sessionToken: "14327659860",
        sessionId: "1357924680",
        expires: 8
      };

      fakeMozLoop.rooms.join.callsArgWith(1, null, responseData);

      store.gotMediaPermission();

      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWith(dispatcher.dispatch,
        new sharedActions.JoinedRoom(responseData));
    });

    it("should dispatch `RoomFailure` on error", function() {
      var fakeError = new Error("fake");

      fakeMozLoop.rooms.join.callsArgWith(1, fakeError);

      store.gotMediaPermission();

      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWith(dispatcher.dispatch,
        new sharedActions.RoomFailure({
          error: fakeError,
          failedJoinRequest: true
        }));
    });
  });

  describe("#joinedRoom", function() {
    var fakeJoinedData;

    beforeEach(function() {
      fakeJoinedData = {
        apiKey: "9876543210",
        sessionToken: "12563478",
        sessionId: "15263748",
        windowId: "42",
        expires: 20
      };

      store.setStoreState({
        roomToken: "fakeToken"
      });
    });

    it("should set the state to `JOINED`", function() {
      store.joinedRoom(new sharedActions.JoinedRoom(fakeJoinedData));

      expect(store._storeState.roomState).eql(ROOM_STATES.JOINED);
    });

    it("should store the session and api values", function() {
      store.joinedRoom(new sharedActions.JoinedRoom(fakeJoinedData));

      var state = store.getStoreState();
      expect(state.apiKey).eql(fakeJoinedData.apiKey);
      expect(state.sessionToken).eql(fakeJoinedData.sessionToken);
      expect(state.sessionId).eql(fakeJoinedData.sessionId);
    });

    it("should start the session connection with the sdk", function() {
      var actionData = new sharedActions.JoinedRoom(fakeJoinedData);

      store.joinedRoom(actionData);

      sinon.assert.calledOnce(fakeSdkDriver.connectSession);
      sinon.assert.calledWithExactly(fakeSdkDriver.connectSession,
        actionData);
    });

    it("should pass 'sendTwoWayMediaTelemetry' as true to connectSession if " +
       "store._isDesktop is true", function() {
      store._isDesktop = true;

      store.joinedRoom(new sharedActions.JoinedRoom(fakeJoinedData));

      sinon.assert.calledOnce(fakeSdkDriver.connectSession);
      sinon.assert.calledWithMatch(fakeSdkDriver.connectSession,
        sinon.match.has("sendTwoWayMediaTelemetry", true));
    });

    it("should pass 'sendTwoWayTelemetry' as false to connectionSession if " +
       "store._isDesktop is false", function() {
      store._isDesktop = false;

      store.joinedRoom(new sharedActions.JoinedRoom(fakeJoinedData));

      sinon.assert.calledOnce(fakeSdkDriver.connectSession);
      sinon.assert.calledWithMatch(fakeSdkDriver.connectSession,
        sinon.match.has("sendTwoWayMediaTelemetry", false));
    });

    it("should call mozLoop.addConversationContext", function() {
      var actionData = new sharedActions.JoinedRoom(fakeJoinedData);

      store.setupWindowData(new sharedActions.SetupWindowData({
        windowId: "42",
        type: "room"
      }));

      store.joinedRoom(actionData);

      sinon.assert.calledOnce(fakeMozLoop.addConversationContext);
      sinon.assert.calledWithExactly(fakeMozLoop.addConversationContext,
                                     "42", "15263748", "");
    });

    it("should call mozLoop.rooms.refreshMembership before the expiresTime",
      function() {
        store.joinedRoom(new sharedActions.JoinedRoom(fakeJoinedData));

        sandbox.clock.tick(fakeJoinedData.expires * 1000);

        sinon.assert.calledOnce(fakeMozLoop.rooms.refreshMembership);
        sinon.assert.calledWith(fakeMozLoop.rooms.refreshMembership,
          "fakeToken", "12563478");
    });

    it("should call mozLoop.rooms.refreshMembership before the next expiresTime",
      function() {
        fakeMozLoop.rooms.refreshMembership.callsArgWith(2,
          null, {expires: 40});

        store.joinedRoom(new sharedActions.JoinedRoom(fakeJoinedData));

        // Clock tick for the first expiry time (which
        // sets up the refreshMembership).
        sandbox.clock.tick(fakeJoinedData.expires * 1000);

        // Clock tick for expiry time in the refresh membership response.
        sandbox.clock.tick(40000);

        sinon.assert.calledTwice(fakeMozLoop.rooms.refreshMembership);
        sinon.assert.calledWith(fakeMozLoop.rooms.refreshMembership,
          "fakeToken", "12563478");
    });

    it("should dispatch `RoomFailure` if the refreshMembership call failed",
      function() {
        var fakeError = new Error("fake");
        fakeMozLoop.rooms.refreshMembership.callsArgWith(2, fakeError);

        store.joinedRoom(new sharedActions.JoinedRoom(fakeJoinedData));

        // Clock tick for the first expiry time (which
        // sets up the refreshMembership).
        sandbox.clock.tick(fakeJoinedData.expires * 1000);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWith(dispatcher.dispatch,
          new sharedActions.RoomFailure({
            error: fakeError,
            failedJoinRequest: false
          }));
    });
  });

  describe("#connectedToSdkServers", function() {
    it("should set the state to `SESSION_CONNECTED`", function() {
      store.connectedToSdkServers(new sharedActions.ConnectedToSdkServers());

      expect(store.getStoreState().roomState).eql(ROOM_STATES.SESSION_CONNECTED);
    });
  });

  describe("#connectionFailure", function() {
    var connectionFailureAction;

    beforeEach(function() {
      store.setStoreState({
        roomState: ROOM_STATES.JOINED,
        roomToken: "fakeToken",
        sessionToken: "1627384950"
      });

      connectionFailureAction = new sharedActions.ConnectionFailure({
        reason: "FAIL"
      });
    });

    it("should retry publishing if on desktop, and in the videoMuted state", function() {
      store._isDesktop = true;

      store.connectionFailure(new sharedActions.ConnectionFailure({
        reason: FAILURE_DETAILS.UNABLE_TO_PUBLISH_MEDIA
      }));

      sinon.assert.calledOnce(fakeSdkDriver.retryPublishWithoutVideo);
    });

    it("should set videoMuted to try when retrying publishing", function() {
      store._isDesktop = true;

      store.connectionFailure(new sharedActions.ConnectionFailure({
        reason: FAILURE_DETAILS.UNABLE_TO_PUBLISH_MEDIA
      }));

      expect(store.getStoreState().videoMuted).eql(true);
    });

    it("should store the failure reason", function() {
      store.connectionFailure(connectionFailureAction);

      expect(store.getStoreState().failureReason).eql("FAIL");
    });

    it("should reset the multiplexGum", function() {
      store.connectionFailure(connectionFailureAction);

      sinon.assert.calledOnce(fakeMultiplexGum.reset);
    });

    it("should set screen sharing inactive", function() {
      store.setStoreState({windowId: "1234"});

      store.connectionFailure(connectionFailureAction);

      sinon.assert.calledOnce(fakeMozLoop.setScreenShareState);
      sinon.assert.calledWithExactly(fakeMozLoop.setScreenShareState, "1234", false);
    });

    it("should disconnect from the servers via the sdk", function() {
      store.connectionFailure(connectionFailureAction);

      sinon.assert.calledOnce(fakeSdkDriver.disconnectSession);
    });

    it("should clear any existing timeout", function() {
      sandbox.stub(window, "clearTimeout");
      store._timeout = {};

      store.connectionFailure(connectionFailureAction);

      sinon.assert.calledOnce(clearTimeout);
    });

    it("should call mozLoop.rooms.leave", function() {
      store.connectionFailure(connectionFailureAction);

      sinon.assert.calledOnce(fakeMozLoop.rooms.leave);
      sinon.assert.calledWithExactly(fakeMozLoop.rooms.leave,
        "fakeToken", "1627384950");
    });

    it("should remove the sharing listener", function() {
      // Setup the listener.
      store.startScreenShare(new sharedActions.StartScreenShare({
        type: "browser"
      }));

      // Now simulate connection failure.
      store.connectionFailure(connectionFailureAction);

      sinon.assert.calledOnce(fakeMozLoop.removeBrowserSharingListener);
    });

    it("should set the state to `FAILED`", function() {
      store.connectionFailure(connectionFailureAction);

      expect(store.getStoreState().roomState).eql(ROOM_STATES.FAILED);
    });
  });

  describe("#setMute", function() {
    it("should save the mute state for the audio stream", function() {
      store.setStoreState({audioMuted: false});

      store.setMute(new sharedActions.SetMute({
        type: "audio",
        enabled: true
      }));

      expect(store.getStoreState().audioMuted).eql(false);
    });

    it("should save the mute state for the video stream", function() {
      store.setStoreState({videoMuted: true});

      store.setMute(new sharedActions.SetMute({
        type: "video",
        enabled: false
      }));

      expect(store.getStoreState().videoMuted).eql(true);
    });
  });

  describe("#screenSharingState", function() {
    beforeEach(function() {
      store.setStoreState({windowId: "1234"});
    });

    it("should save the state", function() {
      store.screenSharingState(new sharedActions.ScreenSharingState({
        state: SCREEN_SHARE_STATES.ACTIVE
      }));

      expect(store.getStoreState().screenSharingState).eql(SCREEN_SHARE_STATES.ACTIVE);
    });

    it("should set screen sharing active when the state is active", function() {
      store.screenSharingState(new sharedActions.ScreenSharingState({
        state: SCREEN_SHARE_STATES.ACTIVE
      }));

      sinon.assert.calledOnce(fakeMozLoop.setScreenShareState);
      sinon.assert.calledWithExactly(fakeMozLoop.setScreenShareState, "1234", true);
    });

    it("should set screen sharing inactive when the state is inactive", function() {
      store.screenSharingState(new sharedActions.ScreenSharingState({
        state: SCREEN_SHARE_STATES.INACTIVE
      }));

      sinon.assert.calledOnce(fakeMozLoop.setScreenShareState);
      sinon.assert.calledWithExactly(fakeMozLoop.setScreenShareState, "1234", false);
    });
  });

  describe("#receivingScreenShare", function() {
    it("should save the state", function() {
      store.receivingScreenShare(new sharedActions.ReceivingScreenShare({
        receiving: true
      }));

      expect(store.getStoreState().receivingScreenShare).eql(true);
    });
  });

  describe("#startScreenShare", function() {
    it("should set the state to 'pending'", function() {
      store.startScreenShare(new sharedActions.StartScreenShare({
        type: "window"
      }));

      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWith(dispatcher.dispatch,
        new sharedActions.ScreenSharingState({
          state: SCREEN_SHARE_STATES.PENDING
        }));
    });

    it("should invoke the SDK driver with the correct options for window sharing", function() {
      store.startScreenShare(new sharedActions.StartScreenShare({
        type: "window"
      }));

      sinon.assert.calledOnce(fakeSdkDriver.startScreenShare);
      sinon.assert.calledWith(fakeSdkDriver.startScreenShare, {
        videoSource: "window"
      });
    });

    it("should add a browser sharing listener for tab sharing", function() {
      store.startScreenShare(new sharedActions.StartScreenShare({
        type: "browser"
      }));

      sinon.assert.calledOnce(fakeMozLoop.addBrowserSharingListener);
    });

    it("should invoke the SDK driver with the correct options for tab sharing", function() {
      fakeMozLoop.addBrowserSharingListener.callsArgWith(0, null, 42);

      store.startScreenShare(new sharedActions.StartScreenShare({
        type: "browser"
      }));

      sinon.assert.calledOnce(fakeSdkDriver.startScreenShare);
      sinon.assert.calledWith(fakeSdkDriver.startScreenShare, {
        videoSource: "browser",
        constraints: {
          browserWindow: 42,
          scrollWithPage: true
        }
      });
    });
  });

  describe("Screen share Events", function() {
    var listener;

    beforeEach(function() {
      store.startScreenShare(new sharedActions.StartScreenShare({
        type: "browser"
      }));

      // Listener is the first argument of the first call.
      listener = fakeMozLoop.addBrowserSharingListener.args[0][0];

      store.setStoreState({
        screenSharingState: SCREEN_SHARE_STATES.ACTIVE
      });
    });

    it("should update the SDK driver when a new window id is received", function() {
      listener(null, 72);

      sinon.assert.calledOnce(fakeSdkDriver.switchAcquiredWindow);
      sinon.assert.calledWithExactly(fakeSdkDriver.switchAcquiredWindow, 72);
    });

    it("should end the screen sharing session when the listener receives an error", function() {
      listener(new Error("foo"));

      // The dispatcher was already called once in beforeEach().
      sinon.assert.calledTwice(dispatcher.dispatch);
      sinon.assert.calledWith(dispatcher.dispatch,
        new sharedActions.ScreenSharingState({
          state: SCREEN_SHARE_STATES.INACTIVE
        }));
      sinon.assert.notCalled(fakeSdkDriver.switchAcquiredWindow);
    });
  });

  describe("#endScreenShare", function() {
    it("should set the state to 'inactive'", function() {
      store.endScreenShare();

      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWith(dispatcher.dispatch,
        new sharedActions.ScreenSharingState({
          state: SCREEN_SHARE_STATES.INACTIVE
        }));
    });

    it("should remove the sharing listener", function() {
      // Setup the listener.
      store.startScreenShare(new sharedActions.StartScreenShare({
        type: "browser"
      }));

      // Now stop the screen share.
      store.endScreenShare();

      sinon.assert.calledOnce(fakeMozLoop.removeBrowserSharingListener);
    });
  });

  describe("#remotePeerConnected", function() {
    it("should set the state to `HAS_PARTICIPANTS`", function() {
      store.remotePeerConnected();

      expect(store.getStoreState().roomState).eql(ROOM_STATES.HAS_PARTICIPANTS);
    });

    it("should set the pref for ToS to `seen`", function() {
      store.remotePeerConnected();

      sinon.assert.calledOnce(fakeMozLoop.setLoopPref);
      sinon.assert.calledWithExactly(fakeMozLoop.setLoopPref,
        "seenToS", "seen");
    });
  });

  describe("#remotePeerDisconnected", function() {
    it("should set the state to `SESSION_CONNECTED`", function() {
      store.remotePeerDisconnected();

      expect(store.getStoreState().roomState).eql(ROOM_STATES.SESSION_CONNECTED);
    });
  });

  describe("#connectionStatus", function() {
    it("should call rooms.sendConnectionStatus on mozLoop", function() {
      store.setStoreState({
        roomToken: "fakeToken",
        sessionToken: "9876543210"
      });

      var data = new sharedActions.ConnectionStatus({
        event: "Publisher.streamCreated",
        state: "sendrecv",
        connections: 2,
        recvStreams: 1,
        sendStreams: 2
      });

      store.connectionStatus(data);

      sinon.assert.calledOnce(fakeMozLoop.rooms.sendConnectionStatus);
      sinon.assert.calledWith(fakeMozLoop.rooms.sendConnectionStatus,
        "fakeToken", "9876543210", data);
    });
  });

  describe("#windowUnload", function() {
    beforeEach(function() {
      store.setStoreState({
        roomState: ROOM_STATES.JOINED,
        roomToken: "fakeToken",
        sessionToken: "1627384950",
        windowId: "1234"
      });
    });

    it("should set screen sharing inactive", function() {
      store.screenSharingState(new sharedActions.ScreenSharingState({
        state: SCREEN_SHARE_STATES.INACTIVE
      }));

      sinon.assert.calledOnce(fakeMozLoop.setScreenShareState);
      sinon.assert.calledWithExactly(fakeMozLoop.setScreenShareState, "1234", false);
    });

    it("should reset the multiplexGum", function() {
      store.windowUnload();

      sinon.assert.calledOnce(fakeMultiplexGum.reset);
    });

    it("should disconnect from the servers via the sdk", function() {
      store.windowUnload();

      sinon.assert.calledOnce(fakeSdkDriver.disconnectSession);
    });

    it("should clear any existing timeout", function() {
      sandbox.stub(window, "clearTimeout");
      store._timeout = {};

      store.windowUnload();

      sinon.assert.calledOnce(clearTimeout);
    });

    it("should call mozLoop.rooms.leave", function() {
      store.windowUnload();

      sinon.assert.calledOnce(fakeMozLoop.rooms.leave);
      sinon.assert.calledWithExactly(fakeMozLoop.rooms.leave,
        "fakeToken", "1627384950");
    });

    it("should call mozLoop.rooms.leave if the room state is JOINING",
      function() {
        store.setStoreState({roomState: ROOM_STATES.JOINING});

        store.windowUnload();

        sinon.assert.calledOnce(fakeMozLoop.rooms.leave);
        sinon.assert.calledWithExactly(fakeMozLoop.rooms.leave,
          "fakeToken", "1627384950");
      });

    it("should remove the sharing listener", function() {
      // Setup the listener.
      store.startScreenShare(new sharedActions.StartScreenShare({
        type: "browser"
      }));

      // Now unload the window.
      store.windowUnload();

      sinon.assert.calledOnce(fakeMozLoop.removeBrowserSharingListener);
    });

    it("should set the state to CLOSING", function() {
      store.windowUnload();

      expect(store._storeState.roomState).eql(ROOM_STATES.CLOSING);
    });
  });

  describe("#leaveRoom", function() {
    beforeEach(function() {
      store.setStoreState({
        roomState: ROOM_STATES.JOINED,
        roomToken: "fakeToken",
        sessionToken: "1627384950"
      });
    });

    it("should reset the multiplexGum", function() {
      store.leaveRoom();

      sinon.assert.calledOnce(fakeMultiplexGum.reset);
    });

    it("should disconnect from the servers via the sdk", function() {
      store.leaveRoom();

      sinon.assert.calledOnce(fakeSdkDriver.disconnectSession);
    });

    it("should clear any existing timeout", function() {
      sandbox.stub(window, "clearTimeout");
      store._timeout = {};

      store.leaveRoom();

      sinon.assert.calledOnce(clearTimeout);
    });

    it("should call mozLoop.rooms.leave", function() {
      store.leaveRoom();

      sinon.assert.calledOnce(fakeMozLoop.rooms.leave);
      sinon.assert.calledWithExactly(fakeMozLoop.rooms.leave,
        "fakeToken", "1627384950");
    });

    it("should remove the sharing listener", function() {
      // Setup the listener.
      store.startScreenShare(new sharedActions.StartScreenShare({
        type: "browser"
      }));

      // Now leave the room.
      store.leaveRoom();

      sinon.assert.calledOnce(fakeMozLoop.removeBrowserSharingListener);
    });

    it("should set the state to ENDED", function() {
      store.leaveRoom();

      expect(store._storeState.roomState).eql(ROOM_STATES.ENDED);
    });
  });

  describe("#_handleSocialShareUpdate", function() {
    it("should dispatch an UpdateRoomInfo action", function() {
      store._handleSocialShareUpdate();

      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWithExactly(dispatcher.dispatch,
        new sharedActions.UpdateSocialShareInfo({
          socialShareButtonAvailable: false,
          socialShareProviders: []
        }));
    });

    it("should call respective mozLoop methods", function() {
      store._handleSocialShareUpdate();

      sinon.assert.calledOnce(fakeMozLoop.isSocialShareButtonAvailable);
      sinon.assert.calledOnce(fakeMozLoop.getSocialShareProviders);
    });
  });

  describe("Events", function() {
    describe("update:{roomToken}", function() {
      beforeEach(function() {
        store.setupRoomInfo(new sharedActions.SetupRoomInfo({
          roomName: "Its a room",
          roomOwner: "Me",
          roomToken: "fakeToken",
          roomUrl: "http://invalid",
          socialShareButtonAvailable: false,
          socialShareProviders: []
        }));
      });

      it("should dispatch an UpdateRoomInfo action", function() {
        sinon.assert.calledTwice(fakeMozLoop.rooms.on);

        var fakeRoomData = {
          decryptedContext: {
            roomName: "fakeName"
          },
          roomOwner: "you",
          roomUrl: "original"
        };

        fakeMozLoop.rooms.on.callArgWith(1, "update", fakeRoomData);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.UpdateRoomInfo({
            roomName: fakeRoomData.decryptedContext.roomName,
            roomOwner: fakeRoomData.roomOwner,
            roomUrl: fakeRoomData.roomUrl
          }));
      });
    });

    describe("delete:{roomToken}", function() {
      var fakeRoomData = {
        decryptedContext: {
          roomName: "Its a room"
        },
        roomOwner: "Me",
        roomToken: "fakeToken",
        roomUrl: "http://invalid"
      };

      beforeEach(function() {
        store.setupRoomInfo(new sharedActions.SetupRoomInfo(
          _.extend(fakeRoomData, {
            socialShareButtonAvailable: false,
            socialShareProviders: []
          })
        ));
      });

      it("should disconnect all room connections", function() {
        fakeMozLoop.rooms.on.callArgWith(1, "delete:" + fakeRoomData.roomToken, fakeRoomData);

        sinon.assert.calledOnce(fakeSdkDriver.forceDisconnectAll);
      });

      it("should not disconnect anything when another room is deleted", function() {
        fakeMozLoop.rooms.on.callArgWith(1, "delete:invalidToken", fakeRoomData);

        sinon.assert.calledOnce(fakeSdkDriver.forceDisconnectAll);
      });
    });
  });
});
