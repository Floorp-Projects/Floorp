/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

describe("loop.store.ActiveRoomStore", function() {
  "use strict";

  var expect = chai.expect;
  var sharedActions = loop.shared.actions;
  var sharedUtils = loop.shared.utils;
  var REST_ERRNOS = loop.shared.utils.REST_ERRNOS;
  var ROOM_STATES = loop.store.ROOM_STATES;
  var CHAT_CONTENT_TYPES = loop.shared.utils.CHAT_CONTENT_TYPES;
  var FAILURE_DETAILS = loop.shared.utils.FAILURE_DETAILS;
  var SCREEN_SHARE_STATES = loop.shared.utils.SCREEN_SHARE_STATES;
  var ROOM_INFO_FAILURES = loop.shared.utils.ROOM_INFO_FAILURES;
  var sandbox, dispatcher, store, requestStubs, fakeSdkDriver, fakeMultiplexGum;
  var standaloneMediaRestore;
  var clock;

  beforeEach(function() {
    sandbox = LoopMochaUtils.createSandbox();
    clock = sandbox.useFakeTimers();

    LoopMochaUtils.stubLoopRequest(requestStubs = {
      GetLoopPref: sinon.stub(),
      GetSelectedTabMetadata: sinon.stub(),
      SetLoopPref: sinon.stub(),
      AddConversationContext: sinon.stub(),
      AddBrowserSharingListener: sinon.stub().returns(42),
      HangupNow: sinon.stub(),
      RemoveBrowserSharingListener: sinon.stub(),
      "Rooms:Get": sinon.stub().returns({
        roomUrl: "http://invalid"
      }),
      "Rooms:Join": sinon.stub().returns({}),
      "Rooms:RefreshMembership": sinon.stub().returns({ expires: 42 }),
      "Rooms:SendConnectionStatus": sinon.stub(),
      "Rooms:PushSubscription": sinon.stub(),
      SetScreenShareState: sinon.stub(),
      GetActiveTabWindowId: sandbox.stub().returns(42),
      GetSocialShareProviders: sinon.stub().returns([]),
      TelemetryAddValue: sinon.stub()
    });

    dispatcher = new loop.Dispatcher();
    sandbox.stub(dispatcher, "dispatch");
    sandbox.stub(window, "close");

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

    standaloneMediaRestore = loop.standaloneMedia;
    loop.standaloneMedia = {
      multiplexGum: fakeMultiplexGum
    };

    store = new loop.store.ActiveRoomStore(dispatcher, {
      sdkDriver: fakeSdkDriver
    });

    sandbox.stub(document.mozL10n ? document.mozL10n : navigator.mozL10n, "get", function(x) {
      return x;
    });
  });

  afterEach(function() {
    sandbox.restore();
    LoopMochaUtils.restore();
    loop.standaloneMedia = standaloneMediaRestore;
  });

  describe("#constructor", function() {
    it("should throw an error if sdkDriver is missing", function() {
      expect(function() {
        new loop.store.ActiveRoomStore(dispatcher, { mozLoop: {} });
      }).to.Throw(/sdkDriver/);
    });
  });

  describe("#getInitialStoreState", function() {
    it("should return an object with roomContextUrls set to null", function() {
      var initialState = store.getInitialStoreState();

      expect(initialState).to.have.a.property("roomContextUrls", null);
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
        sessionToken: "1627384950",
        windowId: "42"
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
      // errno !== undefined
      fakeError.errno = 999;
      store.roomFailure(new sharedActions.RoomFailure({
        error: fakeError,
        failedJoinRequest: false
      }));

      expect(store._storeState.roomState).eql(ROOM_STATES.FAILED);
      expect(store._storeState.failureReason).eql(FAILURE_DETAILS.UNKNOWN);
    });

    it("should set the state to `COULD_NOT_CONNECT` on undefined errno", function() {
      store.roomFailure(new sharedActions.RoomFailure({
        error: fakeError,
        failedJoinRequest: false
      }));

      expect(store._storeState.roomState).eql(ROOM_STATES.FAILED);
      expect(store._storeState.failureReason).eql(FAILURE_DETAILS.COULD_NOT_CONNECT);
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
      sandbox.stub(loop, "unsubscribe");

      // Setup the listener.
      store.startBrowserShare(new sharedActions.StartBrowserShare());

      // Now simulate room failure.
      store.roomFailure(new sharedActions.RoomFailure({
        error: fakeError,
        failedJoinRequest: false
      }));

      sinon.assert.calledOnce(loop.unsubscribe);
      sinon.assert.calledWith(loop.unsubscribe, "BrowserSwitch");
    });

    it("should call 'HangupNow' Loop API", function() {
      store.roomFailure(new sharedActions.RoomFailure({
        error: fakeError,
        failedJoinRequest: false
      }));

      sinon.assert.calledOnce(requestStubs["HangupNow"]);
      sinon.assert.calledWithExactly(requestStubs["HangupNow"],
        "fakeToken", "1627384950", "42");
    });

    it("should not call 'HangupNow' Loop API if failedJoinRequest is true", function() {
      store.roomFailure(new sharedActions.RoomFailure({
        error: fakeError,
        failedJoinRequest: true
      }));

      sinon.assert.notCalled(requestStubs["HangupNow"]);
    });
  });

  describe("#retryAfterRoomFailure", function() {
    beforeEach(function() {
      sandbox.stub(console, "error");
    });

    it("should reject attempts to retry for invalid/expired urls", function() {
      store.setStoreState({
        failureReason: FAILURE_DETAILS.EXPIRED_OR_INVALID
      });

      store.retryAfterRoomFailure();

      sinon.assert.calledOnce(console.error);
      sinon.assert.calledWithMatch(console.error, "Invalid");
      sinon.assert.notCalled(dispatcher.dispatch);
    });

    it("should reject attempts if the failure exit state is not expected", function() {
      store.setStoreState({
        failureReason: FAILURE_DETAILS.UNKNOWN,
        failureExitState: ROOM_STATES.INIT
      });

      store.retryAfterRoomFailure();

      sinon.assert.calledOnce(console.error);
      sinon.assert.calledWithMatch(console.error, "Unexpected");
      sinon.assert.notCalled(dispatcher.dispatch);
    });

    it("should dispatch a FetchServerData action when the exit state is GATHER", function() {
      store.setStoreState({
        failureReason: FAILURE_DETAILS.UNKNOWN,
        failureExitState: ROOM_STATES.GATHER,
        roomCryptoKey: "fakeKey",
        roomToken: "fakeToken"
      });

      store.retryAfterRoomFailure();

      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWithExactly(dispatcher.dispatch,
        new sharedActions.FetchServerData({
          cryptoKey: "fakeKey",
          token: "fakeToken",
          windowType: "room"
        }));
    });

    it("should join the room for other states", function() {
      sandbox.stub(store, "joinRoom");

      store.setStoreState({
        failureReason: FAILURE_DETAILS.UNKNOWN,
        failureExitState: ROOM_STATES.MEDIA_WAIT,
        roomCryptoKey: "fakeKey",
        roomToken: "fakeToken"
      });

      store.retryAfterRoomFailure();

      sinon.assert.calledOnce(store.joinRoom);
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
        participants: [],
        roomUrl: "http://invalid"
      };

      store = new loop.store.ActiveRoomStore(dispatcher, {
        sdkDriver: {}
      });
      requestStubs["Rooms:Get"].withArgs(fakeToken).returns(fakeRoomData);
    });

    it("should set the state to `GATHER`", function() {
      return store.setupWindowData(new sharedActions.SetupWindowData({
        windowId: "42",
        type: "room",
        roomToken: fakeToken
      })).then(function() {
        expect(store.getStoreState()).to.have.property(
          "roomState", ROOM_STATES.GATHER);
      });
    });

    it("should store the room token and window id", function() {
      return store.setupWindowData(new sharedActions.SetupWindowData({
        windowId: "42",
        type: "room",
        roomToken: fakeToken
      })).then(function() {
        expect(store.getStoreState().windowId).eql("42");
        expect(store.getStoreState().roomToken).eql(fakeToken);
      });
    });

    it("should dispatch an UpdateRoomInfo action if the get is successful",
      function() {
        return store.setupWindowData(new sharedActions.SetupWindowData({
          windowId: "42",
          type: "room",
          roomToken: fakeToken
        })).then(function() {
          sinon.assert.calledTwice(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.UpdateRoomInfo({
              roomContextUrls: undefined,
              roomDescription: undefined,
              participants: [],
              roomName: fakeRoomData.decryptedContext.roomName,
              roomState: ROOM_STATES.READY,
              roomUrl: fakeRoomData.roomUrl,
              socialShareProviders: []
            }));
        });
      });

    it("should dispatch a JoinRoom action if the get is successful",
      function() {
        return store.setupWindowData(new sharedActions.SetupWindowData({
          windowId: "42",
          type: "room",
          roomToken: fakeToken
        })).then(function() {
          sinon.assert.calledTwice(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.JoinRoom());
        });
      });

    it("should dispatch a RoomFailure action if the get fails",
      function() {
        var fakeError = new Error("fake error");
        fakeError.isError = true;
        requestStubs["Rooms:Get"].withArgs(fakeToken).returns(fakeError);

        return store.setupWindowData(new sharedActions.SetupWindowData({
          windowId: "42",
          type: "room",
          roomToken: fakeToken
        })).then(function() {
          sinon.assert.calledOnce(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.RoomFailure({
              error: fakeError,
              failedJoinRequest: false
            }));
        });
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
      return store.fetchServerData(fetchServerAction).then(function() {
        expect(store.getStoreState().roomToken).eql("fakeToken");
      });
    });

    it("should set the state to `GATHER`", function() {
      return store.fetchServerData(fetchServerAction).then(function() {
        expect(store.getStoreState().roomState).eql(ROOM_STATES.GATHER);
      });
    });

    it("should call mozLoop.rooms.get to get the room data", function() {
      return store.fetchServerData(fetchServerAction).then(function() {
        sinon.assert.calledOnce(requestStubs["Rooms:Get"]);
      });
    });

    it("should dispatch an UpdateRoomInfo message with failure if neither roomName nor context are supplied", function() {
      return store.fetchServerData(fetchServerAction).then(function() {
        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.UpdateRoomInfo({
            roomInfoFailure: ROOM_INFO_FAILURES.NO_DATA,
            roomState: ROOM_STATES.READY,
            roomUrl: "http://invalid"
          }));
      });
    });

    describe("mozLoop.rooms.get returns roomName as a separate field (no context)", function() {
      it("should dispatch UpdateRoomInfo if mozLoop.rooms.get is successful", function() {
        var roomDetails = {
          roomName: "fakeName",
          roomUrl: "http://invalid"
        };

        requestStubs["Rooms:Get"].returns(roomDetails);

        return store.fetchServerData(fetchServerAction).then(function() {
          sinon.assert.called(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.UpdateRoomInfo(_.extend({
              roomState: ROOM_STATES.READY
            }, roomDetails)));
        });
      });
    });

    describe("mozLoop.rooms.get returns encryptedContext", function() {
      var roomDetails, expectedDetails;

      beforeEach(function() {
        roomDetails = {
          context: {
            value: "fakeContext"
          },
          roomUrl: "http://invalid"
        };
        expectedDetails = {
          roomUrl: "http://invalid"
        };

        requestStubs["Rooms:Get"].returns(roomDetails);

        sandbox.stub(loop.crypto, "isSupported").returns(true);
      });

      it("should dispatch UpdateRoomInfo message with 'unsupported' failure if WebCrypto is unsupported", function() {
        loop.crypto.isSupported.returns(false);

        return store.fetchServerData(fetchServerAction).then(function() {
          sinon.assert.called(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.UpdateRoomInfo(_.extend({
              roomInfoFailure: ROOM_INFO_FAILURES.WEB_CRYPTO_UNSUPPORTED,
              roomState: ROOM_STATES.READY
            }, expectedDetails)));
        });
      });

      it("should dispatch UpdateRoomInfo message with 'no crypto key' failure if there is no crypto key", function() {
        return store.fetchServerData(fetchServerAction).then(function() {
          sinon.assert.called(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.UpdateRoomInfo(_.extend({
              roomInfoFailure: ROOM_INFO_FAILURES.NO_CRYPTO_KEY,
              roomState: ROOM_STATES.READY
            }, expectedDetails)));
        });
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

        return store.fetchServerData(fetchServerAction).then(function() {
          sinon.assert.called(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.UpdateRoomInfo(_.extend({
              roomInfoFailure: ROOM_INFO_FAILURES.DECRYPT_FAILED,
              roomState: ROOM_STATES.READY
            }, expectedDetails)));
        });
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
            then: function(resolve) {
              resolve(JSON.stringify(roomContext));
            }
          };
        });

        return store.fetchServerData(fetchServerAction).then(function() {
          var expectedData = _.extend({
            roomContextUrls: roomContext.urls,
            roomDescription: roomContext.description,
            roomName: roomContext.roomName,
            roomState: ROOM_STATES.READY
          }, expectedDetails);

          sinon.assert.called(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.UpdateRoomInfo(expectedData));
        });
      });
    });

    describe("User Agent Room Handling", function() {
      var channelListener, roomDetails;

      beforeEach(function() {
        sandbox.stub(sharedUtils, "isFirefox").returns(true);

        roomDetails = {
          roomName: "fakeName",
          roomUrl: "http://invalid"
        };
        requestStubs["Rooms:Get"].returns(roomDetails);

        sandbox.stub(window, "addEventListener", function(eventName, listener) {
          if (eventName === "WebChannelMessageToContent") {
            channelListener = listener;
          }
        });
        sandbox.stub(window, "removeEventListener", function(eventName, listener) {
          if (eventName === "WebChannelMessageToContent" &&
              listener === channelListener) {
            channelListener = null;
          }
        });
      });

      it("should dispatch UserAgentHandlesRoom with false if the user agent is not Firefox", function() {
        sharedUtils.isFirefox.returns(false);

        return store.fetchServerData(fetchServerAction).then(function() {
          sinon.assert.called(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.UserAgentHandlesRoom({
              handlesRoom: false
            }));
        });
      });

      it("should dispatch with false after a timeout if there is no response from the channel", function() {
        // When the dispatchEvent is called, we know the setup code has run, so
        // advance the timer.
        sandbox.stub(window, "dispatchEvent", function() {
          sandbox.clock.tick(250);
        });

        return store.fetchServerData(fetchServerAction).then(function() {
          sinon.assert.called(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.UserAgentHandlesRoom({
              handlesRoom: false
            }));
        });
      });

      it("should not dispatch if a message is returned not for the link-clicker", function() {
        // When the dispatchEvent is called, we know the setup code has run, so
        // advance the timer.
        sandbox.stub(window, "dispatchEvent", function() {
          // We call the listener twice, but the first time with an invalid id.
          // Hence we should only get the dispatch once.
          channelListener({
            detail: {
              id: "invalid-id",
              message: null
            }
          });
          channelListener({
            detail: {
              id: "loop-link-clicker",
              message: null
            }
          });
        });

        return store.fetchServerData(fetchServerAction).then(function() {
          // Although this is only called once for the UserAgentHandlesRoom,
          // it gets called twice due to the UpdateRoomInfo. Therefore,
          // we test both results here.
          sinon.assert.calledTwice(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.UserAgentHandlesRoom({
              handlesRoom: false
            }));
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.UpdateRoomInfo(_.extend({
              roomState: ROOM_STATES.READY
            }, roomDetails)));
        });
      });

      it("should dispatch with false if the user agent does not understand the message", function() {
        // When the dispatchEvent is called, we know the setup code has run, so
        // advance the timer.
        sandbox.stub(window, "dispatchEvent", function() {
          channelListener({
            detail: {
              id: "loop-link-clicker",
              message: null
            }
          });
        });

        return store.fetchServerData(fetchServerAction).then(function() {
          sinon.assert.called(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.UserAgentHandlesRoom({
              handlesRoom: false
            }));
        });
      });

      it("should dispatch with false if the user agent cannot handle the message", function() {
        // When the dispatchEvent is called, we know the setup code has run, so
        // advance the timer.
        sandbox.stub(window, "dispatchEvent", function() {
          channelListener({
            detail: {
              id: "loop-link-clicker",
              message: {
                response: false
              }
            }
          });
        });

        return store.fetchServerData(fetchServerAction).then(function() {
          sinon.assert.called(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.UserAgentHandlesRoom({
              handlesRoom: false
            }));
        });
      });

      it("should dispatch with true if the user agent can handle the message", function() {
        // When the dispatchEvent is called, we know the setup code has run, so
        // advance the timer.
        sandbox.stub(window, "dispatchEvent", function() {
          channelListener({
            detail: {
              id: "loop-link-clicker",
              message: {
                response: true
              }
            }
          });
        });

        return store.fetchServerData(fetchServerAction).then(function() {
          sinon.assert.called(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.UserAgentHandlesRoom({
              handlesRoom: true
            }));
        });
      });
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

  describe("#videoScreenStreamChanged", function() {
    it("should set streamPaused if screen stream has no video", function() {
      var actionData = {
        hasVideo: false
      };

      store.videoScreenStreamChanged(new sharedActions.VideoScreenStreamChanged(actionData));
      expect(store.getStoreState().streamPaused).eql(true);
    });
  });

  describe("#updateRoomInfo", function() {
    var fakeRoomInfo;

    beforeEach(function() {
      fakeRoomInfo = {
        roomContextUrls: [{
          description: "fake site",
          location: "http://invalid.com",
          thumbnail: "data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
        }],
        roomName: "Its a room",
        roomUrl: "http://invalid"
      };
    });

    it("should save the room information", function() {
      store.updateRoomInfo(new sharedActions.UpdateRoomInfo(fakeRoomInfo));

      var state = store.getStoreState();
      expect(state.roomName).eql(fakeRoomInfo.roomName);
      expect(state.roomUrl).eql(fakeRoomInfo.roomUrl);
      expect(state.roomContextUrls).eql(fakeRoomInfo.roomContextUrls);
    });
  });

  describe("#userAgentHandlesRoom", function() {
    it("should update the store state", function() {
      store.setStoreState({
        UserAgentHandlesRoom: false
      });

      store.userAgentHandlesRoom(new sharedActions.UserAgentHandlesRoom({
        handlesRoom: true
      }));

      expect(store.getStoreState().userAgentHandlesRoom).eql(true);
    });
  });

  describe("#updateSocialShareInfo", function() {
    var fakeSocialShareInfo;

    beforeEach(function() {
      fakeSocialShareInfo = {
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
      expect(state.socialShareProviders)
        .eql(fakeSocialShareInfo.socialShareProviders);
    });
  });

  describe("#joinRoom", function() {
    var hasDevicesStub;

    beforeEach(function() {
      store.setStoreState({ roomState: ROOM_STATES.READY });
      hasDevicesStub = sandbox.stub(loop.shared.utils, "hasAudioOrVideoDevices");
    });

    it("should reset failureReason", function() {
      store.setStoreState({ failureReason: "Test" });

      store.joinRoom();

      expect(store.getStoreState().failureReason).eql(undefined);
    });

    describe("Standalone Handles Room", function() {
      it("should dispatch a MetricsLogJoinRoom action", function() {
        store.joinRoom();

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.MetricsLogJoinRoom({
            userAgentHandledRoom: false
          }));
      });

      it("should set the state to MEDIA_WAIT if media devices are present", function() {
        hasDevicesStub.callsArgWith(0, true);

        store.joinRoom();

        expect(store.getStoreState().roomState).eql(ROOM_STATES.MEDIA_WAIT);
      });

      it("should not set the state to MEDIA_WAIT if no media devices are present", function() {
        hasDevicesStub.callsArgWith(0, false);

        store.joinRoom();

        expect(store.getStoreState().roomState).eql(ROOM_STATES.READY);
      });

      it("should dispatch `ConnectionFailure` if no media devices are present", function() {
        hasDevicesStub.callsArgWith(0, false);

        store.joinRoom();

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.ConnectionFailure({
            reason: FAILURE_DETAILS.NO_MEDIA
          }));
      });
    });

    describe("User Agent Handles Room", function() {
      var channelListener;

      beforeEach(function() {
        store.setStoreState({
          userAgentHandlesRoom: true,
          roomToken: "fakeToken",
          standalone: true
        });

        sandbox.stub(window, "addEventListener", function(eventName, listener) {
          if (eventName === "WebChannelMessageToContent") {
            channelListener = listener;
          }
        });
        sandbox.stub(window, "removeEventListener", function(eventName, listener) {
          if (eventName === "WebChannelMessageToContent" &&
              listener === channelListener) {
            channelListener = null;
          }
        });

        sandbox.stub(console, "error");
      });

      it("should dispatch a MetricsLogJoinRoom action", function() {
        store.joinRoom();

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.MetricsLogJoinRoom({
            userAgentHandledRoom: true,
            ownRoom: true
          }));
      });

      it("should dispatch an event to Firefox", function() {
        sandbox.stub(window, "dispatchEvent");

        store.joinRoom();

        sinon.assert.calledOnce(window.dispatchEvent);
        sinon.assert.calledWithExactly(window.dispatchEvent, new window.CustomEvent(
          "WebChannelMessageToChrome", {
          detail: {
            id: "loop-link-clicker",
            message: {
              command: "openRoom",
              roomToken: "fakeToken"
            }
          }
        }));
      });

      it("should log an error if Firefox doesn't handle the room", function() {
        // Start the join.
        store.joinRoom();

        // Pretend Firefox calls back.
        channelListener({
          detail: {
            id: "loop-link-clicker",
            message: null
          }
        });

        sinon.assert.calledOnce(console.error);
      });

      it("should dispatch a JoinedRoom action if the room was successfully opened", function() {
        // Start the join.
        store.joinRoom();

        // Pretend Firefox calls back.
        channelListener({
          detail: {
            id: "loop-link-clicker",
            message: {
              response: true,
              alreadyOpen: false
            }
          }
        });

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.JoinedRoom({
            apiKey: "",
            sessionToken: "",
            sessionId: "",
            expires: 0
          }));
      });

      it("should dispatch a ConnectionFailure action if the room was already opened", function() {
        // Start the join.
        store.joinRoom();

        // Pretend Firefox calls back.
        channelListener({
          detail: {
            id: "loop-link-clicker",
            message: {
              response: true,
              alreadyOpen: true
            }
          }
        });

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.ConnectionFailure({
            reason: FAILURE_DETAILS.ROOM_ALREADY_OPEN
          }));
      });
    });
  });

  describe("#gotMediaPermission", function() {
    var responseData;

    beforeEach(function() {
      responseData = {
        apiKey: "keyFake",
        sessionToken: "14327659860",
        sessionId: "1357924680",
        expires: 8
      };
      requestStubs["Rooms:Join"].returns(responseData);
      store.setStoreState({ roomToken: "tokenFake" });
    });

    it("should set the room state to JOINING", function() {
      store.gotMediaPermission();

      expect(store.getStoreState().roomState).eql(ROOM_STATES.JOINING);
    });

    it("should call rooms.join on mozLoop", function() {
      store.gotMediaPermission();

      sinon.assert.calledOnce(requestStubs["Rooms:Join"]);
      sinon.assert.calledWith(requestStubs["Rooms:Join"], "tokenFake", "display_name_guest");
    });

    it("should dispatch `JoinedRoom` on success", function() {
      store.gotMediaPermission();

      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWith(dispatcher.dispatch,
        new sharedActions.JoinedRoom(responseData));
    });

    it("should dispatch `RoomFailure` on error", function() {
      var fakeError = new Error("fake");
      fakeError.isError = true;
      requestStubs["Rooms:Join"].returns(fakeError);

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
    var fakeJoinedData, fakeRoomData;

    beforeEach(function() {
      fakeJoinedData = {
        apiKey: "9876543210",
        sessionToken: "12563478",
        sessionId: "15263748",
        windowId: "42",
        expires: 20
      };
      fakeRoomData = {
        decryptedContext: {
          roomName: "Monkeys"
        },
        participants: [],
        roomUrl: "http://invalid"
      };
      requestStubs["Rooms:Get"].returns(fakeRoomData);

      store.setStoreState({
        roomToken: "fakeToken"
      });
    });

    it("should set the state to `JOINED`", function() {
      store.joinedRoom(new sharedActions.JoinedRoom(fakeJoinedData));

      expect(store._storeState.roomState).eql(ROOM_STATES.JOINED);
    });

    it("should set the state to `JOINED` when Firefox handles the room", function() {
      store.setStoreState({
        userAgentHandlesRoom: true,
        standalone: true
      });

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

    it("should not store the session and api values when Firefox handles the room", function() {
      store.setStoreState({
        userAgentHandlesRoom: true,
        standalone: true
      });

      store.joinedRoom(new sharedActions.JoinedRoom(fakeJoinedData));

      var state = store.getStoreState();
      expect(state.apiKey).eql(undefined);
      expect(state.sessionToken).eql(undefined);
      expect(state.sessionId).eql(undefined);
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

    it("should call LoopAPI.AddConversationContext", function() {
      var actionData = new sharedActions.JoinedRoom(fakeJoinedData);

      return store.setupWindowData(new sharedActions.SetupWindowData({
        windowId: "42",
        type: "room"
      })).then(function() {
        store.joinedRoom(actionData);

        sinon.assert.calledOnce(requestStubs.AddConversationContext);
        sinon.assert.calledWithExactly(requestStubs.AddConversationContext,
                                       "42", "15263748", "");
      });
    });

    it("should call Rooms:RefreshMembership before the expiresTime",
      function() {
        store.joinedRoom(new sharedActions.JoinedRoom(fakeJoinedData));

        sandbox.clock.tick(fakeJoinedData.expires * 1000);

        sinon.assert.calledOnce(requestStubs["Rooms:RefreshMembership"]);
        sinon.assert.calledWith(requestStubs["Rooms:RefreshMembership"],
          "fakeToken", "12563478");
    });

    it("should call mozLoop.rooms.refreshMembership before the next expiresTime",
      function() {
        requestStubs["Rooms:RefreshMembership"].returns({ expires: 40 });

        store.joinedRoom(new sharedActions.JoinedRoom(fakeJoinedData));

        // Clock tick for the first expiry time (which
        // sets up the refreshMembership).
        sandbox.clock.tick(fakeJoinedData.expires * 1000);

        // Clock tick for expiry time in the refresh membership response.
        sandbox.clock.tick(40000);

        sinon.assert.calledTwice(requestStubs["Rooms:RefreshMembership"]);
        sinon.assert.calledWith(requestStubs["Rooms:RefreshMembership"],
          "fakeToken", "12563478");
    });

    it("should dispatch `RoomFailure` if the refreshMembership call failed",
      function() {
        var fakeError = new Error("fake");
        fakeError.isError = true;
        requestStubs["Rooms:RefreshMembership"].returns(fakeError);

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
        sessionToken: "1627384950",
        windowId: "42"
      });

      connectionFailureAction = new sharedActions.ConnectionFailure({
        reason: "FAIL"
      });
    });

    it("should store the failure reason", function() {
      store.connectionFailure(connectionFailureAction);

      expect(store.getStoreState().failureReason).eql("FAIL");
    });

    it("should reset the multiplexGum", function() {
      store.connectionFailure(connectionFailureAction);

      sinon.assert.calledOnce(fakeMultiplexGum.reset);
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

    it("should call 'HangupNow' Loop API", function() {
      store.connectionFailure(connectionFailureAction);

      sinon.assert.calledOnce(requestStubs["HangupNow"]);
      sinon.assert.calledWithExactly(requestStubs["HangupNow"],
        "fakeToken", "1627384950", "42");
    });

    it("should remove the sharing listener", function() {
      sandbox.stub(loop, "unsubscribe");

      // Setup the listener.
      store.startBrowserShare(new sharedActions.StartBrowserShare());

      // Now simulate connection failure.
      store.connectionFailure(connectionFailureAction);

      sinon.assert.calledOnce(loop.unsubscribe);
      sinon.assert.calledWith(loop.unsubscribe, "BrowserSwitch");
    });

    it("should set the state to `FAILED`", function() {
      store.connectionFailure(connectionFailureAction);

      expect(store.getStoreState().roomState).eql(ROOM_STATES.FAILED);
    });

    it("should set the state to `FAILED` if the user agent is handling the room", function() {
      store.setStoreState({
        standalone: true,
        userAgentHandlesRoom: true
      });

      store.connectionFailure(connectionFailureAction);

      expect(store.getStoreState().roomState).eql(ROOM_STATES.FAILED);
    });

    it("should not do any other cleanup if the user agent is handling the room", function() {
      store.setStoreState({
        standalone: true,
        userAgentHandlesRoom: true
      });

      store.connectionFailure(connectionFailureAction);

      sinon.assert.notCalled(fakeMultiplexGum.reset);
      sinon.assert.notCalled(fakeSdkDriver.disconnectSession);
    });
  });

  describe("#setMute", function() {
    it("should save the mute state for the audio stream", function() {
      store.setStoreState({ audioMuted: false });

      store.setMute(new sharedActions.SetMute({
        type: "audio",
        enabled: true
      }));

      expect(store.getStoreState().audioMuted).eql(false);
    });

    it("should save the mute state for the video stream", function() {
      store.setStoreState({ videoMuted: true });

      store.setMute(new sharedActions.SetMute({
        type: "video",
        enabled: false
      }));

      expect(store.getStoreState().videoMuted).eql(true);
    });
  });

  describe("#mediaStreamCreated", function() {
    var fakeStreamElement;

    beforeEach(function() {
      fakeStreamElement = { name: "fakeStreamElement" };
    });

    it("should add a local video object to the store", function() {
      expect(store.getStoreState()).to.not.have.property("localSrcMediaElement");

      store.mediaStreamCreated(new sharedActions.MediaStreamCreated({
        hasVideo: false,
        isLocal: true,
        srcMediaElement: fakeStreamElement
      }));

      expect(store.getStoreState().localSrcMediaElement).eql(fakeStreamElement);
      expect(store.getStoreState()).to.not.have.property("remoteSrcMediaElement");
    });

    it("should set the local video enabled", function() {
      store.setStoreState({
        localVideoEnabled: false,
        remoteVideoEnabled: false
      });

      store.mediaStreamCreated(new sharedActions.MediaStreamCreated({
        hasVideo: true,
        isLocal: true,
        srcMediaElement: fakeStreamElement
      }));

      expect(store.getStoreState().localVideoEnabled).eql(true);
      expect(store.getStoreState().remoteVideoEnabled).eql(false);
    });

    it("should add a remote video object to the store", function() {
      expect(store.getStoreState()).to.not.have.property("remoteSrcMediaElement");

      store.mediaStreamCreated(new sharedActions.MediaStreamCreated({
        hasVideo: false,
        isLocal: false,
        srcMediaElement: fakeStreamElement
      }));

      expect(store.getStoreState()).not.have.property("localSrcMediaElement");
      expect(store.getStoreState().remoteSrcMediaElement).eql(fakeStreamElement);
    });

    it("should set the remote video enabled", function() {
      store.setStoreState({
        localVideoEnabled: false,
        remoteVideoEnabled: false
      });

      store.mediaStreamCreated(new sharedActions.MediaStreamCreated({
        hasVideo: true,
        isLocal: false,
        srcMediaElement: fakeStreamElement
      }));

      expect(store.getStoreState().localVideoEnabled).eql(false);
      expect(store.getStoreState().remoteVideoEnabled).eql(true);
    });
  });

  describe("#mediaStreamDestroyed", function() {
    var fakeStreamElement;

    beforeEach(function() {
      fakeStreamElement = { name: "fakeStreamElement" };

      store.setStoreState({
        localSrcMediaElement: fakeStreamElement,
        remoteSrcMediaElement: fakeStreamElement
      });
    });

    it("should clear the local video object", function() {
      store.mediaStreamDestroyed(new sharedActions.MediaStreamDestroyed({
        isLocal: true
      }));

      expect(store.getStoreState().localSrcMediaElement).eql(null);
      expect(store.getStoreState().remoteSrcMediaElement).eql(fakeStreamElement);
    });

    it("should clear the remote video object", function() {
      store.mediaStreamDestroyed(new sharedActions.MediaStreamDestroyed({
        isLocal: false
      }));

      expect(store.getStoreState().localSrcMediaElement).eql(fakeStreamElement);
      expect(store.getStoreState().remoteSrcMediaElement).eql(null);
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

  describe("#mediaConnected", function() {
    it("should set mediaConnected to true", function() {
      store.mediaConnected();

      expect(store.getStoreState().mediaConnected).eql(true);
    });
  });

  describe("#screenSharingState", function() {
    beforeEach(function() {
      store.setStoreState({ windowId: "1234" });
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

      sinon.assert.calledOnce(requestStubs.SetScreenShareState);
      sinon.assert.calledWithExactly(requestStubs.SetScreenShareState, "1234", true);
    });

    it("should set screen sharing inactive when the state is inactive", function() {
      store.screenSharingState(new sharedActions.ScreenSharingState({
        state: SCREEN_SHARE_STATES.INACTIVE
      }));

      sinon.assert.calledOnce(requestStubs.SetScreenShareState);
      sinon.assert.calledWithExactly(requestStubs.SetScreenShareState, "1234", false);
    });
  });

  describe("#receivingScreenShare", function() {
    it("should save the state", function() {
      store.receivingScreenShare(new sharedActions.ReceivingScreenShare({
        receiving: true
      }));

      expect(store.getStoreState().receivingScreenShare).eql(true);
    });

    it("should add a screenShareMediaElement to the store when sharing is active", function() {
      var fakeStreamElement = { name: "fakeStreamElement" };
      expect(store.getStoreState()).to.not.have.property("screenShareMediaElement");

      store.receivingScreenShare(new sharedActions.ReceivingScreenShare({
        receiving: true,
        srcMediaElement: fakeStreamElement
      }));

      expect(store.getStoreState()).to.have.property("screenShareMediaElement",
        fakeStreamElement);
    });

    it("should clear the screenShareMediaElement from the store when sharing is inactive", function() {
      store.setStoreState({
        screenShareMediaElement: {
          name: "fakeStreamElement"
        }
      });

      store.receivingScreenShare(new sharedActions.ReceivingScreenShare({
        receiving: false,
        srcMediaElement: null
      }));

      expect(store.getStoreState().screenShareMediaElement).eql(null);
    });

    it("should delete the screen remote video dimensions if screen sharing is not active", function() {
      store.setStoreState({
        remoteVideoDimensions: {
          screen: { fake: 10 },
          camera: { fake: 20 }
        }
      });

      store.receivingScreenShare(new sharedActions.ReceivingScreenShare({
        receiving: false
      }));

      expect(store.getStoreState().remoteVideoDimensions).eql({
        camera: { fake: 20 }
      });
    });
  });

  describe("#startBrowserShare", function() {
    var getSelectedTabMetadataStub;

    beforeEach(function() {
      getSelectedTabMetadataStub = sinon.stub();
      LoopMochaUtils.stubLoopRequest({
        GetSelectedTabMetadata: getSelectedTabMetadataStub.returns({
          title: "fakeTitle",
          favicon: "fakeFavicon",
          url: "http://www.fakeurl.com"
        })
      });

      store.setStoreState({
        roomState: ROOM_STATES.JOINED,
        roomToken: "fakeToken",
        sessionToken: "1627384950",
        participants: [{
          displayName: "Owner",
          owner: true
        }, {
          displayName: "Guest",
          owner: false
        }]
      });

      sandbox.stub(console, "error");
    });

    afterEach(function() {
      store.endScreenShare();
    });

    it("should log an error if the state is not inactive", function() {
      store.setStoreState({
        screenSharingState: SCREEN_SHARE_STATES.PENDING
      });

      store.startBrowserShare(new sharedActions.StartBrowserShare());

      sinon.assert.calledOnce(console.error);
    });

    it("should not do anything if the state is not inactive", function() {
      store.setStoreState({
        screenSharingState: SCREEN_SHARE_STATES.PENDING
      });

      store.startBrowserShare(new sharedActions.StartBrowserShare());

      sinon.assert.notCalled(requestStubs.AddBrowserSharingListener);
      sinon.assert.notCalled(fakeSdkDriver.startScreenShare);
    });

    it("should set the state to 'pending'", function() {
      store.startBrowserShare(new sharedActions.StartBrowserShare());
      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWith(dispatcher.dispatch,
        new sharedActions.ScreenSharingState({
          state: SCREEN_SHARE_STATES.PENDING
        }));
    });

    it("should add a browser sharing listener for tab sharing", function() {
      store.startBrowserShare(new sharedActions.StartBrowserShare());
      sinon.assert.calledOnce(requestStubs.AddBrowserSharingListener);
    });

    it("should invoke the SDK driver with the correct options for tab sharing", function() {
      store.startBrowserShare(new sharedActions.StartBrowserShare());
      sinon.assert.calledOnce(fakeSdkDriver.startScreenShare);
      sinon.assert.calledWith(fakeSdkDriver.startScreenShare, {
        videoSource: "browser",
        constraints: {
          browserWindow: 42,
          scrollWithPage: true
        }
      });
    });

    it("should request the new metadata when the browser being shared change", function() {
      store.startBrowserShare(new sharedActions.StartBrowserShare());
      clock.tick(500);

      sinon.assert.calledOnce(getSelectedTabMetadataStub);
      sinon.assert.calledTwice(dispatcher.dispatch);
      sinon.assert.calledWith(dispatcher.dispatch.getCall(1),
        new sharedActions.UpdateRoomContext({
          newRoomDescription: "fakeTitle",
          newRoomThumbnail: "fakeFavicon",
          newRoomURL: "http://www.fakeurl.com",
          roomToken: store.getStoreState().roomToken
      }));
    });

    it("should process only one request", function() {
      store.startBrowserShare(new sharedActions.StartBrowserShare());
      // Simulates multiple requests.
      LoopMochaUtils.publish("BrowserSwitch", 72);
      LoopMochaUtils.publish("BrowserSwitch", 72);

      clock.tick(500);
      sinon.assert.calledThrice(getSelectedTabMetadataStub);
      sinon.assert.calledTwice(dispatcher.dispatch);
      sinon.assert.calledWith(dispatcher.dispatch.getCall(1),
        new sharedActions.UpdateRoomContext({
          newRoomDescription: "fakeTitle",
          newRoomThumbnail: "fakeFavicon",
          newRoomURL: "http://www.fakeurl.com",
          roomToken: store.getStoreState().roomToken
      }));
    });

    it("should not process a request without url", function() {
      getSelectedTabMetadataStub.returns({
        title: "fakeTitle",
        favicon: "fakeFavicon"
      });

      store.startBrowserShare(new sharedActions.StartBrowserShare());
      clock.tick(500);

      sinon.assert.calledOnce(getSelectedTabMetadataStub);
      sinon.assert.calledOnce(dispatcher.dispatch);
    });

    it("should not process a request if sharing is paused", function() {
      store.setStoreState({
        sharingPaused: true
      });

      store.startBrowserShare(new sharedActions.StartBrowserShare());
      clock.tick(500);

      sinon.assert.notCalled(getSelectedTabMetadataStub);
      sinon.assert.calledOnce(dispatcher.dispatch);
    });

    it("should not process a request if no-one is in the room", function() {
      store.setStoreState({
        participants: [{
          displayName: "Owner",
          owner: true
        }]
      });

      store.startBrowserShare(new sharedActions.StartBrowserShare());
      clock.tick(500);

      sinon.assert.notCalled(getSelectedTabMetadataStub);
      sinon.assert.calledOnce(dispatcher.dispatch);
    });
  });

  describe("Screen share Events", function() {
    beforeEach(function() {
      store.startBrowserShare(new sharedActions.StartBrowserShare());

      store.setStoreState({
        screenSharingState: SCREEN_SHARE_STATES.ACTIVE
      });

      // Stub to prevent errors surfacing in the console.
      sandbox.stub(window.console, "error");
    });

    afterEach(function() {
      store.endScreenShare();
    });

    it("should log an error in the console", function() {
      var err = new Error("foo");
      err.isError = true;
      LoopMochaUtils.publish("BrowserSwitch", err);

      sinon.assert.calledOnce(console.error);
    });

    it("should update the SDK driver when a new window id is received", function() {
      LoopMochaUtils.publish("BrowserSwitch", 72);

      sinon.assert.calledOnce(fakeSdkDriver.switchAcquiredWindow);
      sinon.assert.calledWithExactly(fakeSdkDriver.switchAcquiredWindow, 72);
    });

    it("should end the screen sharing session when the listener receives an error", function() {
      var err = new Error("foo");
      err.isError = true;
      LoopMochaUtils.publish("BrowserSwitch", err);

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
      store.startBrowserShare(new sharedActions.StartBrowserShare());

      // Now stop the screen share.
      store.endScreenShare();

      sinon.assert.calledOnce(requestStubs.RemoveBrowserSharingListener);
    });
  });

  describe("#toggleBrowserSharing", function() {
    it("should set paused to false when enabled", function() {
      store.toggleBrowserSharing(new sharedActions.ToggleBrowserSharing({
        enabled: true
      }));

      expect(store.getStoreState().sharingPaused).eql(false);
    });

    it("should set paused to true when not enabled", function() {
      store.toggleBrowserSharing(new sharedActions.ToggleBrowserSharing({
        enabled: false
      }));

      expect(store.getStoreState().sharingPaused).eql(true);
    });

    it("should update context when enabled", function() {
      var getSelectedTabMetadataStub = sinon.stub();
      LoopMochaUtils.stubLoopRequest({
        GetSelectedTabMetadata: getSelectedTabMetadataStub.returns({
          title: "fakeTitle",
          favicon: "fakeFavicon",
          url: "http://www.fakeurl.com"
        })
      });
      store.setStoreState({
        roomState: ROOM_STATES.JOINED,
        roomToken: "fakeToken"
      });

      store.toggleBrowserSharing(new sharedActions.ToggleBrowserSharing({
        enabled: true
      }));
      clock.tick(500);

      sinon.assert.calledOnce(getSelectedTabMetadataStub);
      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWith(dispatcher.dispatch,
        new sharedActions.UpdateRoomContext({
          newRoomDescription: "fakeTitle",
          newRoomThumbnail: "fakeFavicon",
          newRoomURL: "http://www.fakeurl.com",
          roomToken: "fakeToken"
      }));
    });
  });

  describe("#remotePeerConnected", function() {
    it("should set the state to `HAS_PARTICIPANTS`", function() {
      store.remotePeerConnected();

      expect(store.getStoreState().roomState).eql(ROOM_STATES.HAS_PARTICIPANTS);
    });
  });

  describe("#remotePeerDisconnected", function() {
    it("should set the state to `SESSION_CONNECTED`", function() {
      store.remotePeerDisconnected();

      expect(store.getStoreState().roomState).eql(ROOM_STATES.SESSION_CONNECTED);
    });

    it("should clear the mediaConnected state", function() {
      store.setStoreState({
        mediaConnected: true
      });

      store.remotePeerDisconnected();

      expect(store.getStoreState().mediaConnected).eql(false);
    });

    it("should clear the remoteSrcMediaElement", function() {
      store.setStoreState({
        remoteSrcMediaElement: { name: "fakeStreamElement" }
      });

      store.remotePeerDisconnected();

      expect(store.getStoreState().remoteSrcMediaElement).eql(null);
    });

    it("should remove non-owner participants", function() {
      store.setStoreState({
        participants: [{ owner: true }, {}]
      });

      store.remotePeerDisconnected();

      var participants = store.getStoreState().participants;
      expect(participants).to.have.length.of(1);
      expect(participants[0].owner).eql(true);
    });

    it("should keep the owner participant", function() {
      store.setStoreState({
        participants: [{ owner: true }]
      });

      store.remotePeerDisconnected();

      var participants = store.getStoreState().participants;
      expect(participants).to.have.length.of(1);
      expect(participants[0].owner).eql(true);
    });

    it("should clear the streamPaused state", function() {
      store.setStoreState({
        streamPaused: true
      });

      store.remotePeerDisconnected();

      expect(store.getStoreState().streamPaused).eql(false);
    });

    it("should set the remotePeerDisconnected to `true", function() {
      store.setStoreState({
        remotePeerDisconnected: false
      });

      store.remotePeerDisconnected();

      expect(store.getStoreState().remotePeerDisconnected).eql(true);
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

      sinon.assert.calledOnce(requestStubs["Rooms:SendConnectionStatus"]);
      sinon.assert.calledWith(requestStubs["Rooms:SendConnectionStatus"],
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

      sinon.assert.calledOnce(requestStubs.SetScreenShareState);
      sinon.assert.calledWithExactly(requestStubs.SetScreenShareState, "1234", false);
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

    it("should call 'HangupNow' Loop API", function() {
      store.windowUnload();

      sinon.assert.calledOnce(requestStubs["HangupNow"]);
      sinon.assert.calledWith(requestStubs["HangupNow"], "fakeToken",
        "1627384950", "1234");
    });

    it("should call 'HangupNow' Loop API if the room state is JOINING",
      function() {
        store.setStoreState({ roomState: ROOM_STATES.JOINING });

        store.windowUnload();

        sinon.assert.calledOnce(requestStubs["HangupNow"]);
        sinon.assert.calledWith(requestStubs["HangupNow"], "fakeToken",
          "1627384950", "1234");
      });

    it("should remove the sharing listener", function() {
      sandbox.stub(loop, "unsubscribe");

      // Setup the listener.
      store.startBrowserShare(new sharedActions.StartBrowserShare());

      // Now unload the window.
      store.windowUnload();

      sinon.assert.calledOnce(loop.unsubscribe);
      sinon.assert.calledWith(loop.unsubscribe, "BrowserSwitch");
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

    it("should call 'HangupNow' Loop API", function() {
      store.leaveRoom();

      sinon.assert.calledOnce(requestStubs["HangupNow"]);
      sinon.assert.calledWith(requestStubs["HangupNow"], "fakeToken", "1627384950");
    });

    it("should call 'HangupNow' when _isDesktop is true and windowStayingOpen", function() {
      store._isDesktop = true;

      store.leaveRoom({
        windowStayingOpen: true
      });

      sinon.assert.calledOnce(requestStubs["HangupNow"]);
    });

    it("should not call 'HangupNow' Loop API when _isDesktop is true", function() {
      store._isDesktop = true;

      store.leaveRoom();

      sinon.assert.notCalled(requestStubs["HangupNow"]);
    });

    it("should remove the sharing listener", function() {
      sandbox.stub(loop, "unsubscribe");

      // Setup the listener.
      store.startBrowserShare(new sharedActions.StartBrowserShare());

      // Now leave the room.
      store.leaveRoom();

      sinon.assert.calledOnce(loop.unsubscribe);
      sinon.assert.calledWith(loop.unsubscribe, "BrowserSwitch");
    });

    it("should set the state to ENDED", function() {
      store.leaveRoom();

      expect(store._storeState.roomState).eql(ROOM_STATES.ENDED);
    });

    it("should reset various store states", function() {
      store.setStoreState({
        audioMuted: true,
        localVideoDimensions: { x: 10 },
        receivingScreenShare: true,
        remotePeerDisconnected: true,
        remoteVideoDimensions: { y: 10 },
        screenSharingState: true,
        videoMuted: true,
        chatMessageExchanged: false
      });

      store.leaveRoom();

      expect(store._storeState.audioMuted).eql(false);
      expect(store._storeState.localVideoDimensions).eql({});
      expect(store._storeState.receivingScreenShare).eql(false);
      expect(store._storeState.remotePeerDisconnected).eql(false);
      expect(store._storeState.remoteVideoDimensions).eql({});
      expect(store._storeState.screenSharingState).eql(SCREEN_SHARE_STATES.INACTIVE);
      expect(store._storeState.videoMuted).eql(false);
      expect(store._storeState.chatMessageExchanged).eql(false);
    });

    it("should not reset the room context", function() {
      store.setStoreState({
        roomContextUrls: [{ fake: 1 }],
        roomName: "fred"
      });

      store.leaveRoom();

      expect(store._storeState.roomName).eql("fred");
      expect(store._storeState.roomContextUrls).eql([{ fake: 1 }]);
    });
  });

  describe("#_handleSocialShareUpdate", function() {
    it("should dispatch an UpdateRoomInfo action", function() {
      store._handleSocialShareUpdate();

      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWithExactly(dispatcher.dispatch,
        new sharedActions.UpdateSocialShareInfo({
          socialShareProviders: []
        }));
    });

    it("should call respective mozLoop methods", function() {
      store._handleSocialShareUpdate();

      sinon.assert.calledOnce(requestStubs.GetSocialShareProviders);
    });
  });

  describe("#_handleTextChatMessage", function() {
    beforeEach(function() {
      var fakeRoomData = {
        decryptedContext: {
          roomName: "Monkeys"
        },
        participants: [],
        roomUrl: "http://invalid"
      };
      requestStubs["Rooms:Get"].returns(fakeRoomData);

      store._isDesktop = true;
      store.setupWindowData(new sharedActions.SetupWindowData({
        windowId: "42",
        type: "room",
        roomToken: "fakeToken"
      }));
    });

    function assertWeDidNothing() {
      expect(dispatcher._eventData.receivedTextChatMessage.length).eql(1);
      expect(dispatcher._eventData.sendTextChatMessage.length).eql(1);
      expect(store.getStoreState().chatMessageExchanged).eql(false);
      sinon.assert.notCalled(requestStubs.TelemetryAddValue);
    }

    it("should not do anything for the link clicker side", function() {
      store._isDesktop = false;

      store._handleTextChatMessage(new sharedActions.SendTextChatMessage({
        contentType: CHAT_CONTENT_TYPES.TEXT,
        message: "Hello!",
        sentTimestamp: "1970-01-01T00:00:00.000Z"
      }));

      assertWeDidNothing();
    });

    it("should not do anything when a chat message has arrived before", function() {
      store.setStoreState({ chatMessageExchanged: true });

      store._handleTextChatMessage(new sharedActions.ReceivedTextChatMessage({
        contentType: CHAT_CONTENT_TYPES.TEXT,
        message: "Hello!",
        receivedTimestamp: "1970-01-01T00:00:00.000Z"
      }));

      sinon.assert.notCalled(requestStubs.TelemetryAddValue);
    });

    it("should not do anything for non-chat messages", function() {
      store._handleTextChatMessage(new sharedActions.SendTextChatMessage({
        contentType: CHAT_CONTENT_TYPES.CONTEXT,
        message: "Hello!",
        sentTimestamp: "1970-01-01T00:00:00.000Z"
      }));

      assertWeDidNothing();
    });

    it("should ping telemetry when a chat message arrived or is to be sent", function() {
      store._handleTextChatMessage(new sharedActions.ReceivedTextChatMessage({
        contentType: CHAT_CONTENT_TYPES.TEXT,
        message: "Hello!",
        receivedTimestamp: "1970-01-01T00:00:00.000Z"
      }));

      sinon.assert.calledOnce(requestStubs.TelemetryAddValue);
      sinon.assert.calledWithExactly(requestStubs.TelemetryAddValue,
        "LOOP_ROOM_SESSION_WITHCHAT", 1);
      expect(store.getStoreState().chatMessageExchanged).eql(true);
      expect(dispatcher._eventData.hasOwnProperty("receivedTextChatMessage")).eql(false);
      expect(dispatcher._eventData.hasOwnProperty("sendTextChatMessage")).eql(false);
    });
  });

  describe("Events", function() {
    describe("update:{roomToken}", function() {
      beforeEach(function() {
        var fakeRoomData = {
          decryptedContext: {
            roomName: "Monkeys"
          },
          participants: [],
          roomUrl: "http://invalid"
        };
        requestStubs["Rooms:Get"].returns(fakeRoomData);

        store.setupWindowData(new sharedActions.SetupWindowData({
          roomToken: "fakeToken",
          type: "room",
          windowId: "42"
        }));
      });

      it("should dispatch an UpdateRoomInfo action", function() {
        sinon.assert.calledOnce(requestStubs["Rooms:PushSubscription"]);
        sinon.assert.calledWithExactly(requestStubs["Rooms:PushSubscription"],
          ["delete:fakeToken", "update:fakeToken"]);

        var fakeRoomData = {
          decryptedContext: {
            description: "fakeDescription",
            roomName: "fakeName",
            urls: {
              fake: "url"
            }
          },
          roomUrl: "original"
        };

        LoopMochaUtils.publish("Rooms:Update:fakeToken", fakeRoomData);

        sinon.assert.calledThrice(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.UpdateRoomInfo({
            roomDescription: "fakeDescription",
            participants: undefined,
            roomName: fakeRoomData.decryptedContext.roomName,
            roomUrl: fakeRoomData.roomUrl,
            roomContextUrls: {
              fake: "url"
            }
          }));
      });

      it("should not update anything when another room is updated", function() {
        var fakeRoomData = {
          decryptedContext: {
            description: "fakeDescription",
            roomName: "fakeName",
            urls: {
              fake: "url"
            }
          },
          roomUrl: "original"
        };

        LoopMochaUtils.publish("Rooms:Update:invalidToken", fakeRoomData);

        sinon.assert.calledTwice(dispatcher.dispatch);
      });
    });

    describe("delete:{roomToken}", function() {
      var fakeRoomData = {
        decryptedContext: {
          roomName: "Its a room"
        },
        roomToken: "fakeToken",
        roomUrl: "http://invalid"
      };

      beforeEach(function() {
        requestStubs["Rooms:Get"].returns(fakeRoomData);
        store.setupWindowData(new sharedActions.SetupWindowData({
          roomToken: "fakeToken",
          type: "room",
          windowId: "42"
        }));
      });

      it("should disconnect all room connections", function() {
        LoopMochaUtils.publish("Rooms:Delete:" + fakeRoomData.roomToken, fakeRoomData);

        sinon.assert.calledOnce(fakeSdkDriver.forceDisconnectAll);
      });

      it("should not disconnect anything when another room is deleted", function() {
        LoopMochaUtils.publish("Rooms:Delete:invalidToken", fakeRoomData);

        sinon.assert.notCalled(fakeSdkDriver.forceDisconnectAll);
      });
    });
  });
});
