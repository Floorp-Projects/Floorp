/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

describe("loop.OTSdkDriver", function () {
  "use strict";

  var expect = chai.expect;
  var sharedActions = loop.shared.actions;
  var FAILURE_DETAILS = loop.shared.utils.FAILURE_DETAILS;
  var STREAM_PROPERTIES = loop.shared.utils.STREAM_PROPERTIES;
  var SCREEN_SHARE_STATES = loop.shared.utils.SCREEN_SHARE_STATES;
  var CHAT_CONTENT_TYPES = loop.store.CHAT_CONTENT_TYPES;

  var sandbox;
  var dispatcher, driver, mozLoop, publisher, sdk, session, sessionData, subscriber;
  var publisherConfig, fakeEvent;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();

    fakeEvent = {
      preventDefault: sinon.stub()
    };
    publisherConfig = {
      fake: "config"
    };
    sessionData = {
      apiKey: "1234567890",
      sessionId: "3216549870",
      sessionToken: "1357924680"
    };

    dispatcher = new loop.Dispatcher();

    sandbox.stub(dispatcher, "dispatch");

    session = _.extend({
      connect: sinon.stub(),
      disconnect: sinon.stub(),
      publish: sinon.stub(),
      unpublish: sinon.stub(),
      signal: sinon.stub(),
      subscribe: sinon.stub(),
      forceDisconnect: sinon.stub()
    }, Backbone.Events);

    publisher = _.extend({
      destroy: sinon.stub(),
      publishAudio: sinon.stub(),
      publishVideo: sinon.stub(),
      _: {
        getDataChannel: sinon.stub(),
        switchAcquiredWindow: sinon.stub()
      }
    }, Backbone.Events);

    subscriber = _.extend({
      _: {
        getDataChannel: sinon.stub()
      }
    });

    sdk = _.extend({
      initPublisher: sinon.stub().returns(publisher),
      initSession: sinon.stub().returns(session)
    }, Backbone.Events);

    window.OT = {
      ExceptionCodes: {
        CONNECT_FAILED: 1006,
        UNABLE_TO_PUBLISH: 1500
      }
    };

    mozLoop = {
      telemetryAddValue: sinon.stub(),
      TWO_WAY_MEDIA_CONN_LENGTH: {
        SHORTER_THAN_10S: 0,
        BETWEEN_10S_AND_30S: 1,
        BETWEEN_30S_AND_5M: 2,
        MORE_THAN_5M: 3
      },
      SHARING_STATE_CHANGE: {
        WINDOW_ENABLED: 0,
        WINDOW_DISABLED: 1,
        BROWSER_ENABLED: 2,
        BROWSER_DISABLED: 3
      }
    };

    driver = new loop.OTSdkDriver({
      dispatcher: dispatcher,
      sdk: sdk,
      mozLoop: mozLoop,
      isDesktop: true
    });
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("Constructor", function() {
    it("should throw an error if the dispatcher is missing", function() {
      expect(function() {
        new loop.OTSdkDriver({ sdk: sdk });
      }).to.Throw(/dispatcher/);
    });

    it("should throw an error if the sdk is missing", function() {
      expect(function() {
        new loop.OTSdkDriver({dispatcher: dispatcher});
      }).to.Throw(/sdk/);
    });
  });

  describe("#setupStreamElements", function() {
    it("should call initPublisher", function() {
      driver.setupStreamElements(new sharedActions.SetupStreamElements({
        publisherConfig: publisherConfig
      }));

      var expectedConfig = _.extend({
        channels: {
          text: {}
        }
      }, publisherConfig);

      sinon.assert.calledOnce(sdk.initPublisher);
      sinon.assert.calledWith(sdk.initPublisher,
        sinon.match.instanceOf(HTMLDivElement),
        expectedConfig);
    });
  });

  describe("#retryPublishWithoutVideo", function() {
    beforeEach(function() {
      sdk.initPublisher.returns(publisher);

      driver.setupStreamElements(new sharedActions.SetupStreamElements({
        publisherConfig: publisherConfig
      }));
    });

    it("should make MediaStreamTrack.getSources return without a video source", function(done) {
      driver.retryPublishWithoutVideo();

      window.MediaStreamTrack.getSources(function(sources) {
        expect(sources.some(function(src) {
          return src.kind === "video";
        })).eql(false);

        done();
      });
    });

    it("should call initPublisher", function() {
      driver.retryPublishWithoutVideo();

      var expectedConfig = _.extend({
        channels: {
          text: {}
        }
      }, publisherConfig);

      sinon.assert.calledTwice(sdk.initPublisher);
      sinon.assert.calledWith(sdk.initPublisher,
        sinon.match.instanceOf(HTMLDivElement),
        expectedConfig);
    });
  });

  describe("#setMute", function() {
    beforeEach(function() {
      sdk.initPublisher.returns(publisher);

      driver.setupStreamElements(new sharedActions.SetupStreamElements({
        publisherConfig: publisherConfig
      }));
    });

    it("should publishAudio with the correct enabled value", function() {
      driver.setMute(new sharedActions.SetMute({
        type: "audio",
        enabled: false
      }));

      sinon.assert.calledOnce(publisher.publishAudio);
      sinon.assert.calledWithExactly(publisher.publishAudio, false);
    });

    it("should publishVideo with the correct enabled value", function() {
      driver.setMute(new sharedActions.SetMute({
        type: "video",
        enabled: true
      }));

      sinon.assert.calledOnce(publisher.publishVideo);
      sinon.assert.calledWithExactly(publisher.publishVideo, true);
    });
  });

  describe("#startScreenShare", function() {
    beforeEach(function() {
      sandbox.stub(driver, "_noteSharingState");
    });

    it("should initialize a publisher", function() {
      // We're testing with `videoSource` set to 'browser', not 'window', as it
      // has multiple options.
      var options = {
        videoSource: "browser",
        constraints: {
          browserWindow: 42,
          scrollWithPage: true
        }
      };
      driver.startScreenShare(options);

      sinon.assert.calledOnce(sdk.initPublisher);
      sinon.assert.calledWithMatch(sdk.initPublisher,
        sinon.match.instanceOf(HTMLDivElement), options);
    });

    it("should log a telemetry action", function() {
      var options = {
        videoSource: "browser",
        constraints: {
          browserWindow: 42,
          scrollWithPage: true
        }
      };
      driver.startScreenShare(options);

      sinon.assert.calledWithExactly(driver._noteSharingState, "browser", true);
    });
  });

  describe("#switchAcquiredWindow", function() {
    beforeEach(function() {
      var options = {
        videoSource: "browser",
        constraints: {
          browserWindow: 42,
          scrollWithPage: true
        }
      };
      driver.startScreenShare(options);
    });

    it("should switch to the acquired window", function() {
      driver.switchAcquiredWindow(72);

      sinon.assert.calledOnce(publisher._.switchAcquiredWindow);
      sinon.assert.calledWithExactly(publisher._.switchAcquiredWindow, 72);
    });

    it("should not switch if the window is the same as the currently selected one", function() {
      driver.switchAcquiredWindow(42);

      sinon.assert.notCalled(publisher._.switchAcquiredWindow);
    });
  });

  describe("#endScreenShare", function() {
    beforeEach(function() {
      sandbox.stub(driver, "_noteSharingState");
    });

    it("should unpublish the share", function() {
      driver.startScreenShare({
        videoSource: "window"
      });
      driver.session = session;

      driver.endScreenShare(new sharedActions.EndScreenShare());

      sinon.assert.calledOnce(session.unpublish);
    });

    it("should log a telemetry action", function() {
      driver.startScreenShare({
        videoSource: "window"
      });
      driver.session = session;

      driver.endScreenShare(new sharedActions.EndScreenShare());

      sinon.assert.calledWithExactly(driver._noteSharingState, "window", false);
    });

    it("should destroy the share", function() {
      driver.startScreenShare({
        videoSource: "window"
      });
      driver.session = session;

      expect(driver.endScreenShare()).to.equal(true);

      sinon.assert.calledOnce(publisher.destroy);
    });

    it("should unpublish the share too when type is 'browser'", function() {
      driver.startScreenShare({
        videoSource: "browser",
        constraints: {
          browserWindow: 42
        }
      });
      driver.session = session;

      driver.endScreenShare(new sharedActions.EndScreenShare());

      sinon.assert.calledOnce(session.unpublish);
    });

    it("should log a telemetry action too when type is 'browser'", function() {
      driver.startScreenShare({
        videoSource: "browser",
        constraints: {
          browserWindow: 42
        }
      });
      driver.session = session;

      driver.endScreenShare(new sharedActions.EndScreenShare());

      sinon.assert.calledWithExactly(driver._noteSharingState, "browser", false);
    });

    it("should dispatch a ConnectionStatus action", function() {
      driver.startScreenShare({
        videoSource: "browser",
        constraints: {
          browserWindow: 42
        }
      });
      driver.session = session;

      driver._metrics.connections = 2;
      driver._metrics.recvStreams = 1;
      driver._metrics.sendStreams = 2;

      driver.endScreenShare(new sharedActions.EndScreenShare());

      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWithExactly(dispatcher.dispatch,
        new sharedActions.ConnectionStatus({
          event: "Publisher.streamDestroyed",
          state: "sendrecv",
          connections: 2,
          sendStreams: 1,
          recvStreams: 1
        }));
    });
  });

  describe("#connectSession", function() {
    it("should initialise a new session", function() {
      driver.connectSession(sessionData);

      sinon.assert.calledOnce(sdk.initSession);
      sinon.assert.calledWithExactly(sdk.initSession, "3216549870");
    });

    it("should connect the session", function () {
      driver.connectSession(sessionData);

      sinon.assert.calledOnce(session.connect);
      sinon.assert.calledWith(session.connect, "1234567890", "1357924680");
    });

    it("should set the two-way media start time to 'uninitialized' " +
       "when sessionData.sendTwoWayMediaTelemetry is true'", function() {
      driver.connectSession(_.extend(sessionData,
                                     {sendTwoWayMediaTelemetry: true}));

      expect(driver._getTwoWayMediaStartTime()).to.
        eql(driver.CONNECTION_START_TIME_UNINITIALIZED);
    });

    describe("On connection complete", function() {
      beforeEach(function() {
        sandbox.stub(window.console, "error");
      });

      it("should publish the stream if the publisher is ready", function() {
        driver._publisherReady = true;
        session.connect.callsArg(2);

        driver.connectSession(sessionData);

        sinon.assert.calledOnce(session.publish);
      });

      it("should notify metrics", function() {
        session.connect.callsArgWith(2, {
          title: "Fake",
          code: OT.ExceptionCodes.CONNECT_FAILED
        });

        driver.connectSession(sessionData);

        sinon.assert.calledTwice(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.ConnectionStatus({
            event: "sdk.exception." + OT.ExceptionCodes.CONNECT_FAILED,
            state: "starting",
            connections: 0,
            sendStreams: 0,
            recvStreams: 0
          }));
      });

      it("should log an error message and error object", function() {
        session.connect.callsArgWith(2, {
          title: "Fake",
          code: OT.ExceptionCodes.CONNECT_FAILED
        });

        driver.connectSession(sessionData);

        sinon.assert.calledOnce(console.error);
        sinon.assert.calledWithExactly(console.error, sinon.match.string, {
          title: "Fake",
          code: OT.ExceptionCodes.CONNECT_FAILED
        });
      });

      it("should dispatch connectionFailure if connecting failed", function() {
        session.connect.callsArgWith(2, new Error("Failure"));

        driver.connectSession(sessionData);

        sinon.assert.calledTwice(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "connectionFailure"));
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("reason", FAILURE_DETAILS.COULD_NOT_CONNECT));
      });
    });
  });

  describe("#disconnectSession", function() {
    it("should disconnect the session", function() {
      driver.session = session;

      driver.disconnectSession();

      sinon.assert.calledOnce(session.disconnect);
    });

    it("should dispatch a DataChannelsAvailable action with available = false", function() {
      driver.disconnectSession();

      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWithExactly(dispatcher.dispatch,
        new sharedActions.DataChannelsAvailable({
          available: false
        }));
    });

    it("should destroy the publisher", function() {
      driver.publisher = publisher;

      driver.disconnectSession();

      sinon.assert.calledOnce(publisher.destroy);
    });

    it("should call _noteConnectionLengthIfNeeded with connection duration", function() {
      driver.session = session;
      var startTime = 1;
      var endTime = 3;
      driver._sendTwoWayMediaTelemetry = true;
      driver._setTwoWayMediaStartTime(startTime);
      sandbox.stub(performance, "now").returns(endTime);
      sandbox.stub(driver, "_noteConnectionLengthIfNeeded");

      driver.disconnectSession();

      sinon.assert.calledWith(driver._noteConnectionLengthIfNeeded, startTime,
                              endTime);
    });

    it("should reset the two-way media connection start time", function() {
      driver.session = session;
      var startTime = 1;
      driver._sendTwoWayMediaTelemetry = true;
      driver._setTwoWayMediaStartTime(startTime);
      sandbox.stub(performance, "now");
      sandbox.stub(driver, "_noteConnectionLengthIfNeeded");

      driver.disconnectSession();

      expect(driver._getTwoWayMediaStartTime()).to.
        eql(driver.CONNECTION_START_TIME_UNINITIALIZED);
    });
  });

  describe("#_noteConnectionLengthIfNeeded", function() {
    var startTimeMS;
    beforeEach(function() {
      startTimeMS = 1;
      driver._sendTwoWayMediaTelemetry = true;
      driver._setTwoWayMediaStartTime(startTimeMS);
    });

    it("should set two-way media start time to CONNECTION_START_TIME_ALREADY_NOTED", function() {
      var endTimeMS = 3;
      driver._noteConnectionLengthIfNeeded(startTimeMS, endTimeMS);

      expect(driver._getTwoWayMediaStartTime()).to.
        eql(driver.CONNECTION_START_TIME_ALREADY_NOTED);
    });

    it("should call mozLoop.noteConnectionLength with SHORTER_THAN_10S for calls less than 10s", function() {
      var endTimeMS = 9000;

      driver._noteConnectionLengthIfNeeded(startTimeMS, endTimeMS);

      sinon.assert.calledOnce(mozLoop.telemetryAddValue);
      sinon.assert.calledWith(mozLoop.telemetryAddValue,
        "LOOP_TWO_WAY_MEDIA_CONN_LENGTH_1",
        mozLoop.TWO_WAY_MEDIA_CONN_LENGTH.SHORTER_THAN_10S);
    });

    it("should call mozLoop.noteConnectionLength with BETWEEN_10S_AND_30S for 15s calls",
      function() {
        var endTimeMS = 15000;

        driver._noteConnectionLengthIfNeeded(startTimeMS, endTimeMS);

        sinon.assert.calledOnce(mozLoop.telemetryAddValue);
        sinon.assert.calledWith(mozLoop.telemetryAddValue,
          "LOOP_TWO_WAY_MEDIA_CONN_LENGTH_1",
          mozLoop.TWO_WAY_MEDIA_CONN_LENGTH.BETWEEN_10S_AND_30S);
      });

    it("should call mozLoop.noteConnectionLength with BETWEEN_30S_AND_5M for 60s calls",
      function() {
        var endTimeMS = 60 * 1000;

        driver._noteConnectionLengthIfNeeded(startTimeMS, endTimeMS);

        sinon.assert.calledOnce(mozLoop.telemetryAddValue);
        sinon.assert.calledWith(mozLoop.telemetryAddValue,
          "LOOP_TWO_WAY_MEDIA_CONN_LENGTH_1",
          mozLoop.TWO_WAY_MEDIA_CONN_LENGTH.BETWEEN_30S_AND_5M);
      });

    it("should call mozLoop.noteConnectionLength with MORE_THAN_5M for 10m calls", function() {
      var endTimeMS = 10 * 60 * 1000;

      driver._noteConnectionLengthIfNeeded(startTimeMS, endTimeMS);

      sinon.assert.calledOnce(mozLoop.telemetryAddValue);
      sinon.assert.calledWith(mozLoop.telemetryAddValue,
        "LOOP_TWO_WAY_MEDIA_CONN_LENGTH_1",
        mozLoop.TWO_WAY_MEDIA_CONN_LENGTH.MORE_THAN_5M);
    });

    it("should not call mozLoop.noteConnectionLength if" +
       " driver._sendTwoWayMediaTelemetry is false",
      function() {
        var endTimeMS = 10 * 60 * 1000;
        driver._sendTwoWayMediaTelemetry = false;

        driver._noteConnectionLengthIfNeeded(startTimeMS, endTimeMS);

        sinon.assert.notCalled(mozLoop.telemetryAddValue);
      });
  });

  describe("#_noteSharingState", function() {
    it("should record enabled sharing states for window", function() {
      driver._noteSharingState("window", true);

      sinon.assert.calledOnce(mozLoop.telemetryAddValue);
      sinon.assert.calledWithExactly(mozLoop.telemetryAddValue,
        "LOOP_SHARING_STATE_CHANGE_1",
        mozLoop.SHARING_STATE_CHANGE.WINDOW_ENABLED);
    });

    it("should record enabled sharing states for browser", function() {
      driver._noteSharingState("browser", true);

      sinon.assert.calledOnce(mozLoop.telemetryAddValue);
      sinon.assert.calledWithExactly(mozLoop.telemetryAddValue,
        "LOOP_SHARING_STATE_CHANGE_1",
        mozLoop.SHARING_STATE_CHANGE.BROWSER_ENABLED);
    });

    it("should record disabled sharing states for window", function() {
      driver._noteSharingState("window", false);

      sinon.assert.calledOnce(mozLoop.telemetryAddValue);
      sinon.assert.calledWithExactly(mozLoop.telemetryAddValue,
        "LOOP_SHARING_STATE_CHANGE_1",
        mozLoop.SHARING_STATE_CHANGE.WINDOW_DISABLED);
    });

    it("should record disabled sharing states for browser", function() {
      driver._noteSharingState("browser", false);

      sinon.assert.calledOnce(mozLoop.telemetryAddValue);
      sinon.assert.calledWithExactly(mozLoop.telemetryAddValue,
        "LOOP_SHARING_STATE_CHANGE_1",
        mozLoop.SHARING_STATE_CHANGE.BROWSER_DISABLED);
    });
  });

  describe("#forceDisconnectAll", function() {
    it("should not disconnect anything when not connected", function() {
      driver.session = session;
      driver.forceDisconnectAll(function() {});

      sinon.assert.notCalled(session.forceDisconnect);
    });

    it("should disconnect all remote connections when called", function() {
      driver.connectSession(sessionData);
      sinon.assert.calledOnce(session.connect);
      driver._sessionConnected = true;

      // Setup the right state in the driver to make `forceDisconnectAll` do
      // something.
      session.connection = {
        id: "localUser"
      };
      session.trigger("connectionCreated", {
        connection: {id: "remoteUser"}
      });
      expect(driver.connections).to.include.keys("remoteUser");

      driver.forceDisconnectAll(function() {});
      sinon.assert.calledOnce(session.forceDisconnect);

      // Add another remote connection.
      session.trigger("connectionCreated", {
        connection: {id: "remoteUser2"}
      });
      expect(driver.connections).to.include.keys("remoteUser", "remoteUser2");

      driver.forceDisconnectAll(function() {});
      sinon.assert.calledThrice(session.forceDisconnect);
    });

    describe("#sendTextChatMessage", function() {
      it("should send a message on the publisher data channel", function() {
        driver._publisherChannel = {
          send: sinon.stub()
        };

        var message = {
          contentType: CHAT_CONTENT_TYPES.TEXT,
          message: "Help!"
        };

        driver.sendTextChatMessage(message);

        sinon.assert.calledOnce(driver._publisherChannel.send);
        sinon.assert.calledWithExactly(driver._publisherChannel.send,
          JSON.stringify(message));
      });
    });
  });

  describe("Events: general media", function() {
    var fakeConnection, fakeStream, fakeSubscriberObject,
      fakeSdkContainerWithVideo, videoElement;

    beforeEach(function() {
      fakeConnection = "fakeConnection";
      fakeStream = {
        hasVideo: true,
        videoType: "camera",
        videoDimensions: {width: 1, height: 2}
      };

      fakeSubscriberObject = _.extend({
        session: { connection: fakeConnection },
        stream: fakeStream
      }, Backbone.Events);

      fakeSdkContainerWithVideo = {
        querySelector: sinon.stub().returns(videoElement)
      };

      // use a real video element so that these tests correctly reflect
      // test behavior when run in firefox or chrome
      videoElement = document.createElement("video");

      driver.connectSession(sessionData);

      driver.setupStreamElements(new sharedActions.SetupStreamElements({
        publisherConfig: publisherConfig
      }));
    });

    describe("connectionDestroyed", function() {
      it("should dispatch a remotePeerDisconnected action if the client" +
        "disconnected", function() {
          session.trigger("connectionDestroyed", {
            reason: "clientDisconnected"
          });

          sinon.assert.called(dispatcher.dispatch);
          sinon.assert.calledWithMatch(dispatcher.dispatch,
            sinon.match.hasOwn("name", "remotePeerDisconnected"));
          sinon.assert.calledWithMatch(dispatcher.dispatch,
            sinon.match.hasOwn("peerHungup", true));
        });

      it("should dispatch a remotePeerDisconnected action if the connection" +
        "failed", function() {
          session.trigger("connectionDestroyed", {
            reason: "networkDisconnected"
          });

          sinon.assert.called(dispatcher.dispatch);
          sinon.assert.calledWithMatch(dispatcher.dispatch,
            sinon.match.hasOwn("name", "remotePeerDisconnected"));
          sinon.assert.calledWithMatch(dispatcher.dispatch,
            sinon.match.hasOwn("peerHungup", false));
      });

      it("should dispatch a ConnectionStatus action", function() {
        driver._metrics.connections = 1;

        session.trigger("connectionDestroyed", {
          reason: "clientDisconnected"
        });

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.ConnectionStatus({
            event: "Session.connectionDestroyed",
            state: "waiting",
            connections: 0,
            sendStreams: 0,
            recvStreams: 0
          }));
      });

      it("should call _noteConnectionLengthIfNeeded with connection duration", function() {
        driver.session = session;
        var startTime = 1;
        var endTime = 3;
        driver._sendTwoWayMediaTelemetry = true;
        driver._setTwoWayMediaStartTime(startTime);
        sandbox.stub(performance, "now").returns(endTime);
        sandbox.stub(driver, "_noteConnectionLengthIfNeeded");

        session.trigger("connectionDestroyed", {
          reason: "clientDisconnected"
        });

        sinon.assert.calledWith(driver._noteConnectionLengthIfNeeded, startTime,
          endTime);
      });
    });

    describe("sessionDisconnected", function() {
      it("should notify metrics", function() {
        session.trigger("sessionDisconnected", {
          reason: "networkDisconnected"
        });

        sinon.assert.calledTwice(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.ConnectionStatus({
            event: "Session.networkDisconnected",
            state: "starting",
            connections: 0,
            sendStreams: 0,
            recvStreams: 0
          }));
      });

      it("should dispatch a connectionFailure action if the session was disconnected",
        function() {
          session.trigger("sessionDisconnected", {
            reason: "networkDisconnected"
          });

          sinon.assert.calledTwice(dispatcher.dispatch);
          sinon.assert.calledWithMatch(dispatcher.dispatch,
            sinon.match.hasOwn("name", "connectionFailure"));
          sinon.assert.calledWithMatch(dispatcher.dispatch,
            sinon.match.hasOwn("reason", FAILURE_DETAILS.NETWORK_DISCONNECTED));
        });

      it("should dispatch a connectionFailure action if the session was " +
         "forcibly disconnected", function() {
          session.trigger("sessionDisconnected", {
            reason: "forceDisconnected"
          });

          sinon.assert.calledTwice(dispatcher.dispatch);
          sinon.assert.calledWithMatch(dispatcher.dispatch,
            sinon.match.hasOwn("name", "connectionFailure"));
          sinon.assert.calledWithMatch(dispatcher.dispatch,
            sinon.match.hasOwn("reason", FAILURE_DETAILS.EXPIRED_OR_INVALID));
        });

      it("should call _noteConnectionLengthIfNeeded with connection duration", function() {
        driver.session = session;
        var startTime = 1;
        var endTime = 3;
        driver._sendTwoWayMediaTelemetry = true;
        driver._setTwoWayMediaStartTime(startTime);
        sandbox.stub(performance, "now").returns(endTime);
        sandbox.stub(driver, "_noteConnectionLengthIfNeeded");

        session.trigger("sessionDisconnected", {
          reason: "networkDisconnected"
        });

        sinon.assert.calledWith(driver._noteConnectionLengthIfNeeded, startTime,
          endTime);
      });

    });

    describe("streamCreated (publisher/local)", function() {
      var stream, fakeMockVideo;

      beforeEach(function() {
        driver._mockPublisherEl = document.createElement("div");
        fakeMockVideo = document.createElement("video");

        driver._mockPublisherEl.appendChild(fakeMockVideo);
        stream = {
          hasVideo: true,
          videoType: "camera",
          videoDimensions: {width: 1, height: 2}
        };
      });

      it("should dispatch a VideoDimensionsChanged action", function() {
        publisher.trigger("streamCreated", { stream: stream });

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.VideoDimensionsChanged({
            isLocal: true,
            videoType: "camera",
            dimensions: {width: 1, height: 2}
          }));
      });

      it("should dispatch a LocalVideoEnabled action", function() {
        publisher.trigger("streamCreated", { stream: stream });

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.LocalVideoEnabled({
            srcVideoObject: fakeMockVideo
          }));
      });

      it("should dispatch a ConnectionStatus action", function() {
        driver._metrics.recvStreams = 1;
        driver._metrics.connections = 2;

        publisher.trigger("streamCreated", {stream: stream});

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.ConnectionStatus({
            event: "Publisher.streamCreated",
            state: "sendrecv",
            connections: 2,
            recvStreams: 1,
            sendStreams: 1
          }));
      });
    });

    describe("streamCreated: session/remote", function() {

      it("should dispatch a VideoDimensionsChanged action", function() {
        session.trigger("streamCreated", { stream: fakeStream });

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.VideoDimensionsChanged({
            isLocal: false,
            videoType: "camera",
            dimensions: { width: 1, height: 2 }
          }));
      });

      it("should dispatch a ConnectionStatus action", function() {
        driver._metrics.connections = 1;

        session.trigger("streamCreated", { stream: fakeStream });

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.ConnectionStatus({
            event: "Session.streamCreated",
            state: "receiving",
            connections: 1,
            recvStreams: 1,
            sendStreams: 0
          }));
      });

      it("should subscribe to a camera stream", function() {
        session.trigger("streamCreated", { stream: fakeStream });

        sinon.assert.calledOnce(session.subscribe);
        sinon.assert.calledWithExactly(session.subscribe,
          fakeStream, sinon.match.instanceOf(HTMLDivElement), publisherConfig,
          sinon.match.func);
      });

      it("should dispatch RemoteVideoEnabled if the stream has video" +
        " after subscribe is complete", function() {
        session.subscribe.yieldsOn(driver, null, fakeSubscriberObject,
          videoElement).returns(this.fakeSubscriberObject);
        driver.session = session;
        fakeStream.connection = fakeConnection;
        fakeStream.hasVideo = true;

        session.trigger("streamCreated", { stream: fakeStream });

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.RemoteVideoEnabled({
            srcVideoObject: videoElement
          }));
      });

      it("should not dispatch RemoteVideoEnabled if the stream is audio-only", function() {
        session.subscribe.yieldsOn(driver, null, fakeSubscriberObject,
          videoElement);
        fakeStream.connection = fakeConnection;
        fakeStream.hasVideo = false;

        session.trigger("streamCreated", { stream: fakeStream });

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.neverCalledWith(dispatcher.dispatch,
          new sharedActions.RemoteVideoEnabled({
            srcVideoObject: videoElement
          }));
      });

      it("should trigger a readyForDataChannel signal after subscribe is complete", function() {
        session.subscribe.yieldsOn(driver, null, fakeSubscriberObject,
          document.createElement("video"));
        driver._useDataChannels = true;
        fakeStream.connection = fakeConnection;

        session.trigger("streamCreated", { stream: fakeStream });

        sinon.assert.calledOnce(session.signal);
        sinon.assert.calledWith(session.signal, {
          type: "readyForDataChannel",
          to: fakeConnection
        });
      });

      it("should not trigger readyForDataChannel signal if data channels are not wanted", function() {
        session.subscribe.yieldsOn(driver, null, fakeSubscriberObject,
          document.createElement("video"));
        driver._useDataChannels = false;
        fakeStream.connection = fakeConnection;

        session.trigger("streamCreated", { stream: fakeStream });

        sinon.assert.notCalled(session.signal);
      });

      it("should subscribe to a screen sharing stream", function() {
        fakeStream.videoType = "screen";

        session.trigger("streamCreated", { stream: fakeStream });

        sinon.assert.calledOnce(session.subscribe);
        sinon.assert.calledWithExactly(session.subscribe,
          fakeStream, sinon.match.instanceOf(HTMLDivElement), publisherConfig,
          sinon.match.func);
      });

      it("should dispatch a mediaConnected action if both streams are up", function() {
        session.subscribe.yieldsOn(driver, null, fakeSubscriberObject,
          videoElement);
        driver._publishedLocalStream = true;

        session.trigger("streamCreated", { stream: fakeStream });

        // Called twice due to the VideoDimensionsChanged above.
        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          new sharedActions.MediaConnected({}));
      });

      it("should store the start time when both streams are up and" +
      " driver._sendTwoWayMediaTelemetry is true", function() {
        session.subscribe.yieldsOn(driver, null, fakeSubscriberObject,
          videoElement);
        driver._sendTwoWayMediaTelemetry = true;
        driver._publishedLocalStream = true;
        var startTime = 1;
        sandbox.stub(performance, "now").returns(startTime);

        session.trigger("streamCreated", { stream: fakeStream });

        expect(driver._getTwoWayMediaStartTime()).to.eql(startTime);
      });

      it("should not store the start time when both streams are up and" +
         " driver._isDesktop is false", function() {
        session.subscribe.yieldsOn(driver, null, fakeSubscriberObject,
          videoElement);
        driver._isDesktop = false;
        driver._publishedLocalStream = true;
        var startTime = 73;
        sandbox.stub(performance, "now").returns(startTime);

        session.trigger("streamCreated", { stream: fakeStream });

        expect(driver._getTwoWayMediaStartTime()).to.not.eql(startTime);
      });


      it("should not dispatch a mediaConnected action for screen sharing streams",
        function() {
          driver._publishedLocalStream = true;
          fakeStream.videoType = "screen";

          session.trigger("streamCreated", { stream: fakeStream });

          sinon.assert.neverCalledWithMatch(dispatcher.dispatch,
            sinon.match.hasOwn("name", "mediaConnected"));
        });

      it("should not dispatch a ReceivingScreenShare action for camera streams",
        function() {
          session.trigger("streamCreated", {stream: fakeStream});

          sinon.assert.neverCalledWithMatch(dispatcher.dispatch,
            new sharedActions.ReceivingScreenShare({receiving: true}));
        });

      it("should dispatch a ReceivingScreenShare action for screen" +
        " sharing streams", function() {
          fakeStream.videoType = "screen";

          session.trigger("streamCreated", { stream: fakeStream });

          // Called twice due to the VideoDimensionsChanged above.
          sinon.assert.called(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.ReceivingScreenShare({ receiving: true }));
        });
    });

    describe("streamDestroyed: publisher/local", function() {
      it("should dispatch a ConnectionStatus action", function() {
        driver._metrics.sendStreams = 1;
        driver._metrics.recvStreams = 1;
        driver._metrics.connections = 2;

        publisher.trigger("streamDestroyed");

        sinon.assert.calledTwice(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.ConnectionStatus({
            event: "Publisher.streamDestroyed",
            state: "receiving",
            connections: 2,
            recvStreams: 1,
            sendStreams: 0
          }));
      });

      it("should dispatch a DataChannelsAvailable action", function() {
        publisher.trigger("streamDestroyed");

        sinon.assert.calledTwice(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.DataChannelsAvailable({
            available: false
          }));
      });
    });

    describe("streamDestroyed: session/remote", function() {
      var stream;

      beforeEach(function() {
        stream = {
          videoType: "screen"
        };
      });

      it("should dispatch a ReceivingScreenShare action", function() {
        session.trigger("streamDestroyed", { stream: stream });

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.ReceivingScreenShare({
            receiving: false
          }));
      });

      it("should dispatch a ConnectionStatus action", function() {
        driver._metrics.connections = 2;
        driver._metrics.sendStreams = 1;
        driver._metrics.recvStreams = 1;

        session.trigger("streamDestroyed", {stream: stream});

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.ConnectionStatus({
            event: "Session.streamDestroyed",
            state: "sending",
            connections: 2,
            recvStreams: 0,
            sendStreams: 1
          }));
      });

      it("should not dispatch a ConnectionStatus action if the videoType is camera", function() {
        stream.videoType = "camera";

        session.trigger("streamDestroyed", { stream: stream });

        sinon.assert.neverCalledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "receivingScreenShare"));
      });

      it("should dispatch a DataChannelsAvailable action for videoType = camera", function() {
        stream.videoType = "camera";

        session.trigger("streamDestroyed", { stream: stream });

        sinon.assert.calledTwice(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.DataChannelsAvailable({
            available: false
          }));
      });

      it("should not dispatch a DataChannelsAvailable action for videoType = screen", function() {
        session.trigger("streamDestroyed", { stream: stream });

        sinon.assert.neverCalledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "dataChannelsAvailable"));
      });
    });

    describe("streamPropertyChanged", function() {
      var stream = {
        connection: { id: "fake" },
        videoType: "screen",
        videoDimensions: {
          width: 320,
          height: 160
        }
      };

      it("should not dispatch a VideoDimensionsChanged action for other properties", function() {
        session.trigger("streamPropertyChanged", {
          stream: stream,
          changedProperty: STREAM_PROPERTIES.HAS_AUDIO
        });
        session.trigger("streamPropertyChanged", {
          stream: stream,
          changedProperty: STREAM_PROPERTIES.HAS_VIDEO
        });

        sinon.assert.notCalled(dispatcher.dispatch);
      });

      it("should dispatch a VideoDimensionsChanged action", function() {
        session.connection = {
          id: "localUser"
        };
        session.trigger("streamPropertyChanged", {
          stream: stream,
          changedProperty: STREAM_PROPERTIES.VIDEO_DIMENSIONS
        });

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "videoDimensionsChanged"));
      });
    });

    describe("connectionCreated", function() {
      beforeEach(function() {
        session.connection = {
          id: "localUser"
        };
      });

      it("should dispatch a RemotePeerConnected action if this is for a remote user",
        function() {
          session.trigger("connectionCreated", {
            connection: { id: "remoteUser" }
          });

          sinon.assert.called(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.RemotePeerConnected());
        });

      it("should store the connection details for a remote user", function() {
        session.trigger("connectionCreated", {
          connection: { id: "remoteUser" }
        });

        expect(driver.connections).to.include.keys("remoteUser");
      });

      it("should dispatch a ConnectionStatus action for a remote user", function() {
        driver._metrics.connections = 1;
        driver._metrics.sendStreams = 1;

        session.trigger("connectionCreated", {
          connection: { id: "remoteUser" }
        });

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.ConnectionStatus({
          event: "Session.connectionCreated",
          state: "sending",
          connections: 2,
          recvStreams: 0,
          sendStreams: 1
        }));
      });

      it("should not dispatch an RemotePeerConnected action if this is for a local user",
        function() {
          session.trigger("connectionCreated", {
            connection: { id: "localUser" }
          });

          sinon.assert.neverCalledWithMatch(dispatcher.dispatch,
            sinon.match.hasOwn("name", "remotePeerConnected"));
        });

      it("should not store the connection details for a local user", function() {
        session.trigger("connectionCreated", {
          connection: { id: "localUser" }
        });

        expect(driver.connections).to.not.include.keys("localUser");
      });

      it("should dispatch a ConnectionStatus action for a remote user", function() {
        driver._metrics.connections = 0;
        driver._metrics.sendStreams = 0;

        session.trigger("connectionCreated", {
          connection: { id: "localUser" }
        });

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.ConnectionStatus({
            event: "Session.connectionCreated",
            state: "waiting",
            connections: 1,
            recvStreams: 0,
            sendStreams: 0
          }));
      });

    });

    describe("accessAllowed", function() {
      it("should publish the stream if the connection is ready", function() {
        driver._sessionConnected = true;

        publisher.trigger("accessAllowed", fakeEvent);

        sinon.assert.calledOnce(session.publish);
      });

      it("should dispatch a GotMediaPermission action", function() {
        publisher.trigger("accessAllowed", fakeEvent);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.GotMediaPermission());
      });
    });

    describe("accessDenied", function() {
      it("should prevent the default event behavior", function() {
        publisher.trigger("accessDenied", fakeEvent);

        sinon.assert.calledOnce(fakeEvent.preventDefault);
      });

      it("should dispatch connectionFailure", function() {
        publisher.trigger("accessDenied", fakeEvent);

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "connectionFailure"));
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("reason", FAILURE_DETAILS.MEDIA_DENIED));
      });
    });

    describe("accessDialogOpened", function() {
      it("should prevent the default event behavior", function() {
        publisher.trigger("accessDialogOpened", fakeEvent);

        sinon.assert.calledOnce(fakeEvent.preventDefault);
      });
    });

    describe("videoEnabled", function() {
      it("should dispatch RemoteVideoEnabled", function() {
        session.subscribe.yieldsOn(driver, null, fakeSubscriberObject,
          videoElement).returns(this.fakeSubscriberObject);
        session.trigger("streamCreated", {stream: fakeSubscriberObject.stream});
        driver._mockSubscribeEl.appendChild(videoElement);

        fakeSubscriberObject.trigger("videoEnabled");

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWith(dispatcher.dispatch,
          new sharedActions.RemoteVideoEnabled({srcVideoObject: videoElement}));
      });
    });

    describe("videoDisabled", function() {
      it("should dispatch RemoteVideoDisabled", function() {
        session.subscribe.yieldsOn(driver, null, fakeSubscriberObject,
          videoElement).returns(this.fakeSubscriberObject);
        session.trigger("streamCreated", {stream: fakeSubscriberObject.stream});


        fakeSubscriberObject.trigger("videoDisabled");

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.RemoteVideoDisabled({}));
      });
    });

    describe("signal:readyForDataChannel", function() {
      beforeEach(function() {
        driver.subscriber = subscriber;
        driver._useDataChannels = true;
      });

      it("should not do anything if data channels are not wanted", function() {
        driver._useDataChannels = false;

        session.trigger("signal:readyForDataChannel");

        sinon.assert.notCalled(publisher._.getDataChannel);
        sinon.assert.notCalled(subscriber._.getDataChannel);
      });

      it("should get the data channel for the publisher", function() {
        session.trigger("signal:readyForDataChannel");

        sinon.assert.calledOnce(publisher._.getDataChannel);
      });

      it("should get the data channel for the subscriber", function() {
        session.trigger("signal:readyForDataChannel");

        sinon.assert.calledOnce(subscriber._.getDataChannel);
      });

      it("should dispatch `DataChannelsAvailable` once both data channels have been obtained", function() {
        var fakeChannel = _.extend({}, Backbone.Events);

        subscriber._.getDataChannel.callsArgWith(2, null, fakeChannel);
        publisher._.getDataChannel.callsArgWith(2, null, fakeChannel);

        session.trigger("signal:readyForDataChannel");

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.DataChannelsAvailable({
            available: true
          }));
      });

      it("should dispatch `ReceivedTextChatMessage` when a text message is received", function() {
        var fakeChannel = _.extend({}, Backbone.Events);
        var data = '{"contentType":"' + CHAT_CONTENT_TYPES.TEXT +
                   '","message":"Are you there?","receivedTimestamp": "2015-06-25T00:29:14.197Z"}';
        var clock = sinon.useFakeTimers();

        subscriber._.getDataChannel.callsArgWith(2, null, fakeChannel);

        session.trigger("signal:readyForDataChannel");

        // Now send the message.
        fakeChannel.trigger("message", {
          data: data
        });

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.ReceivedTextChatMessage({
            contentType: CHAT_CONTENT_TYPES.TEXT,
            message: "Are you there?",
            receivedTimestamp: "1970-01-01T00:00:00.000Z"
          }));

        /* Restore the time. */
        clock.restore();
      });
    });

    describe("exception", function() {
      it("should notify metrics", function() {
        sdk.trigger("exception", {
          code: OT.ExceptionCodes.CONNECT_FAILED,
          message: "Fake",
          title: "Connect Failed"
        });

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.ConnectionStatus({
            event: "sdk.exception." + OT.ExceptionCodes.CONNECT_FAILED,
            state: "starting",
            connections: 0,
            sendStreams: 0,
            recvStreams: 0
          }));
      });

      describe("Unable to publish (GetUserMedia)", function() {
        it("should destroy the publisher", function() {
          sdk.trigger("exception", {
            code: OT.ExceptionCodes.UNABLE_TO_PUBLISH,
            message: "GetUserMedia"
          });

          sinon.assert.calledOnce(publisher.destroy);
        });

        // XXX We should remove this when we stop being unable to publish as a
        // workaround for knowing if the user has video as well as audio devices
        // installed (bug 1138851).
        it("should not notify metrics", function() {
          sdk.trigger("exception", {
            code: OT.ExceptionCodes.UNABLE_TO_PUBLISH,
            message: "GetUserMedia"
          });

          sinon.assert.neverCalledWith(dispatcher.dispatch,
            new sharedActions.ConnectionStatus({
              event: "sdk.exception." + OT.ExceptionCodes.UNABLE_TO_PUBLISH,
              state: "starting",
              connections: 0,
              sendStreams: 0,
              recvStreams: 0
            }));
        });

        it("should dispatch a ConnectionFailure action", function() {
          sdk.trigger("exception", {
            code: OT.ExceptionCodes.UNABLE_TO_PUBLISH,
            message: "GetUserMedia"
          });

          sinon.assert.calledOnce(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.ConnectionFailure({
              reason: FAILURE_DETAILS.UNABLE_TO_PUBLISH_MEDIA
            }));
        });
      });
    });
  });

  describe("Events: screenshare:", function() {
    var videoElement;

    beforeEach(function() {
      driver.connectSession(sessionData);

      driver.startScreenShare({
        videoSource: "window"
      });

      // use a real video element so that these tests correctly reflect
      // code behavior when run in whatever browser
      videoElement = document.createElement("video");
    });

    describe("accessAllowed", function() {
      it("should publish the stream", function() {
        publisher.trigger("accessAllowed", fakeEvent);

        sinon.assert.calledOnce(session.publish);
      });

      it("should dispatch a `ScreenSharingState` action", function() {
        publisher.trigger("accessAllowed", fakeEvent);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.ScreenSharingState({
            state: SCREEN_SHARE_STATES.ACTIVE
          }));
      });
    });

    describe("accessDenied", function() {
      it("should dispatch a `ScreenShareState` action", function() {
        publisher.trigger("accessDenied", fakeEvent);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.ScreenSharingState({
            state: SCREEN_SHARE_STATES.INACTIVE
          }));
      });
    });

    describe("streamCreated", function() {
      it("should dispatch a ConnectionStatus action", function() {
        driver._metrics.connections = 2;
        driver._metrics.recvStreams = 1;
        driver._metrics.sendStreams = 1;

        publisher.trigger("streamCreated", fakeEvent);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.ConnectionStatus({
            event: "Publisher.streamCreated",
            state: "sendrecv",
            connections: 2,
            recvStreams: 1,
            sendStreams: 2
          }));
      });
    });
  });
});
