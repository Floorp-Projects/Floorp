/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var expect = chai.expect;

describe("loop.OTSdkDriver", function () {
  "use strict";

  var sharedActions = loop.shared.actions;
  var FAILURE_DETAILS = loop.shared.utils.FAILURE_DETAILS;
  var STREAM_PROPERTIES = loop.shared.utils.STREAM_PROPERTIES;
  var SCREEN_SHARE_STATES = loop.shared.utils.SCREEN_SHARE_STATES;

  var sandbox;
  var dispatcher, driver, mozLoop, publisher, sdk, session, sessionData;
  var fakeLocalElement, fakeRemoteElement, fakeScreenElement;
  var publisherConfig, fakeEvent;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();

    fakeLocalElement = {fake: 1};
    fakeRemoteElement = {fake: 2};
    fakeScreenElement = {fake: 3};
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
    session = _.extend({
      connect: sinon.stub(),
      disconnect: sinon.stub(),
      publish: sinon.stub(),
      unpublish: sinon.stub(),
      subscribe: sinon.stub(),
      forceDisconnect: sinon.stub()
    }, Backbone.Events);

    publisher = _.extend({
      destroy: sinon.stub(),
      publishAudio: sinon.stub(),
      publishVideo: sinon.stub(),
      _: {
        switchAcquiredWindow: sinon.stub()
      }
    }, Backbone.Events);

    sdk = _.extend({
      initPublisher: sinon.stub().returns(publisher),
      initSession: sinon.stub().returns(session)
    }, Backbone.Events);

    window.OT = {
      ExceptionCodes: {
        UNABLE_TO_PUBLISH: 1500
      }
    };

    mozLoop = {
      telemetryAddValue: sinon.stub(),
      TWO_WAY_MEDIA_CONN_LENGTH: {
        SHORTER_THAN_10S: "SHORTER_THAN_10S",
        BETWEEN_10S_AND_30S: "BETWEEN_10S_AND_30S",
        BETWEEN_30S_AND_5M: "BETWEEN_30S_AND_5M",
        MORE_THAN_5M: "MORE_THAN_5M"
      },
      SHARING_STATE_CHANGE: {
        WINDOW_ENABLED: "WINDOW_ENABLED",
        WINDOW_DISABLED: "WINDOW_DISABLED",
        BROWSER_ENABLED: "BROWSER_ENABLED",
        BROWSER_DISABLED: "BROWSER_DISABLED"
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
        new loop.OTSdkDriver({sdk: sdk});
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
      dispatcher.dispatch(new sharedActions.SetupStreamElements({
        getLocalElementFunc: function() {return fakeLocalElement;},
        getRemoteElementFunc: function() {return fakeRemoteElement;},
        publisherConfig: publisherConfig
      }));

      sinon.assert.calledOnce(sdk.initPublisher);
      sinon.assert.calledWith(sdk.initPublisher, fakeLocalElement, publisherConfig);
    });
  });

  describe("#retryPublishWithoutVideo", function() {
    beforeEach(function() {
      sdk.initPublisher.returns(publisher);

      driver.setupStreamElements(new sharedActions.SetupStreamElements({
        getLocalElementFunc: function() {return fakeLocalElement;},
        getRemoteElementFunc: function() {return fakeRemoteElement;},
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

      sinon.assert.calledTwice(sdk.initPublisher);
      sinon.assert.calledWith(sdk.initPublisher, fakeLocalElement, publisherConfig);
    });
  });

  describe("#setMute", function() {
    beforeEach(function() {
      sdk.initPublisher.returns(publisher);

      dispatcher.dispatch(new sharedActions.SetupStreamElements({
        getLocalElementFunc: function() {return fakeLocalElement;},
        getRemoteElementFunc: function() {return fakeRemoteElement;},
        publisherConfig: publisherConfig
      }));
    });

    it("should publishAudio with the correct enabled value", function() {
      dispatcher.dispatch(new sharedActions.SetMute({
        type: "audio",
        enabled: false
      }));

      sinon.assert.calledOnce(publisher.publishAudio);
      sinon.assert.calledWithExactly(publisher.publishAudio, false);
    });

    it("should publishVideo with the correct enabled value", function() {
      dispatcher.dispatch(new sharedActions.SetMute({
        type: "video",
        enabled: true
      }));

      sinon.assert.calledOnce(publisher.publishVideo);
      sinon.assert.calledWithExactly(publisher.publishVideo, true);
    });
  });

  describe("#startScreenShare", function() {
    var fakeElement;

    beforeEach(function() {
      sandbox.stub(dispatcher, "dispatch");
      sandbox.stub(driver, "_noteSharingState");

      fakeElement = {
        className: "fakeVideo"
      };

      driver.getScreenShareElementFunc = function() {
        return fakeElement;
      };
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
      sinon.assert.calledWithMatch(sdk.initPublisher, fakeElement, options);
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
      driver.getScreenShareElementFunc = function() {
        return fakeScreenElement;
      };
      sandbox.stub(dispatcher, "dispatch");

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
      driver.getScreenShareElementFunc = function() {};

      sandbox.stub(dispatcher, "dispatch");
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
      it("should publish the stream if the publisher is ready", function() {
        driver._publisherReady = true;
        session.connect.callsArg(2);

        driver.connectSession(sessionData);

        sinon.assert.calledOnce(session.publish);
      });

      it("should dispatch connectionFailure if connecting failed", function() {
        session.connect.callsArgWith(2, new Error("Failure"));
        sandbox.stub(dispatcher, "dispatch");

        driver.connectSession(sessionData);

        sinon.assert.calledOnce(dispatcher.dispatch);
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
        "LOOP_TWO_WAY_MEDIA_CONN_LENGTH",
        mozLoop.TWO_WAY_MEDIA_CONN_LENGTH.SHORTER_THAN_10S);
    });

    it("should call mozLoop.noteConnectionLength with BETWEEN_10S_AND_30S for 15s calls",
      function() {
        var endTimeMS = 15000;

        driver._noteConnectionLengthIfNeeded(startTimeMS, endTimeMS);

        sinon.assert.calledOnce(mozLoop.telemetryAddValue);
        sinon.assert.calledWith(mozLoop.telemetryAddValue,
          "LOOP_TWO_WAY_MEDIA_CONN_LENGTH",
          mozLoop.TWO_WAY_MEDIA_CONN_LENGTH.BETWEEN_10S_AND_30S);
      });

    it("should call mozLoop.noteConnectionLength with BETWEEN_30S_AND_5M for 60s calls",
      function() {
        var endTimeMS = 60 * 1000;

        driver._noteConnectionLengthIfNeeded(startTimeMS, endTimeMS);

        sinon.assert.calledOnce(mozLoop.telemetryAddValue);
        sinon.assert.calledWith(mozLoop.telemetryAddValue,
          "LOOP_TWO_WAY_MEDIA_CONN_LENGTH",
          mozLoop.TWO_WAY_MEDIA_CONN_LENGTH.BETWEEN_30S_AND_5M);
      });

    it("should call mozLoop.noteConnectionLength with MORE_THAN_5M for 10m calls", function() {
      var endTimeMS = 10 * 60 * 1000;

      driver._noteConnectionLengthIfNeeded(startTimeMS, endTimeMS);

      sinon.assert.calledOnce(mozLoop.telemetryAddValue);
      sinon.assert.calledWith(mozLoop.telemetryAddValue,
        "LOOP_TWO_WAY_MEDIA_CONN_LENGTH",
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
        "LOOP_SHARING_STATE_CHANGE",
        mozLoop.SHARING_STATE_CHANGE.WINDOW_ENABLED);
    });

    it("should record enabled sharing states for browser", function() {
      driver._noteSharingState("browser", true);

      sinon.assert.calledOnce(mozLoop.telemetryAddValue);
      sinon.assert.calledWithExactly(mozLoop.telemetryAddValue,
        "LOOP_SHARING_STATE_CHANGE",
        mozLoop.SHARING_STATE_CHANGE.BROWSER_ENABLED);
    });

    it("should record disabled sharing states for window", function() {
      driver._noteSharingState("window", false);

      sinon.assert.calledOnce(mozLoop.telemetryAddValue);
      sinon.assert.calledWithExactly(mozLoop.telemetryAddValue,
        "LOOP_SHARING_STATE_CHANGE",
        mozLoop.SHARING_STATE_CHANGE.WINDOW_DISABLED);
    });

    it("should record disabled sharing states for browser", function() {
      driver._noteSharingState("browser", false);

      sinon.assert.calledOnce(mozLoop.telemetryAddValue);
      sinon.assert.calledWithExactly(mozLoop.telemetryAddValue,
        "LOOP_SHARING_STATE_CHANGE",
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
  });

  describe("Events (general media)", function() {
    beforeEach(function() {
      driver.connectSession(sessionData);

      dispatcher.dispatch(new sharedActions.SetupStreamElements({
        getLocalElementFunc: function() {return fakeLocalElement;},
        getScreenShareElementFunc: function() {return fakeScreenElement;},
        getRemoteElementFunc: function() {return fakeRemoteElement;},
        publisherConfig: publisherConfig
      }));

      sandbox.stub(dispatcher, "dispatch");
    });

    describe("connectionDestroyed", function() {
      it("should dispatch a remotePeerDisconnected action if the client" +
        "disconnected", function() {
          session.trigger("connectionDestroyed", {
            reason: "clientDisconnected"
          });

          sinon.assert.calledOnce(dispatcher.dispatch);
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

          sinon.assert.calledOnce(dispatcher.dispatch);
          sinon.assert.calledWithMatch(dispatcher.dispatch,
            sinon.match.hasOwn("name", "remotePeerDisconnected"));
          sinon.assert.calledWithMatch(dispatcher.dispatch,
            sinon.match.hasOwn("peerHungup", false));
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
      it("should dispatch a connectionFailure action if the session was disconnected",
        function() {
          session.trigger("sessionDisconnected", {
            reason: "networkDisconnected"
          });

          sinon.assert.calledOnce(dispatcher.dispatch);
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

          sinon.assert.calledOnce(dispatcher.dispatch);
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
      it("should dispatch a VideoDimensionsChanged action", function() {
        var fakeStream = {
          hasVideo: true,
          videoType: "camera",
          videoDimensions: {width: 1, height: 2}
        };

        publisher.trigger("streamCreated", {stream: fakeStream});

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.VideoDimensionsChanged({
            isLocal: true,
            videoType: "camera",
            dimensions: {width: 1, height: 2}
          }));
      });
    });

    describe("streamCreated (session/remote)", function() {
      var fakeStream;

      beforeEach(function() {
        fakeStream = {
          hasVideo: true,
          videoType: "camera",
          videoDimensions: {width: 1, height: 2}
        };
      });

      it("should dispatch a VideoDimensionsChanged action", function() {
        session.trigger("streamCreated", {stream: fakeStream});

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.VideoDimensionsChanged({
            isLocal: false,
            videoType: "camera",
            dimensions: {width: 1, height: 2}
          }));
      });

      it("should subscribe to a camera stream", function() {
        session.trigger("streamCreated", {stream: fakeStream});

        sinon.assert.calledOnce(session.subscribe);
        sinon.assert.calledWithExactly(session.subscribe,
          fakeStream, fakeRemoteElement, publisherConfig);
      });

      it("should subscribe to a screen sharing stream", function() {
        fakeStream.videoType = "screen";

        session.trigger("streamCreated", {stream: fakeStream});

        sinon.assert.calledOnce(session.subscribe);
        sinon.assert.calledWithExactly(session.subscribe,
          fakeStream, fakeScreenElement, publisherConfig);
      });

      it("should dispatch a mediaConnected action if both streams are up", function() {
        driver._publishedLocalStream = true;

        session.trigger("streamCreated", {stream: fakeStream});

        // Called twice due to the VideoDimensionsChanged above.
        sinon.assert.calledTwice(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "mediaConnected"));
      });

      it("should store the start time when both streams are up and" +
      " driver._sendTwoWayMediaTelemetry is true", function() {
        driver._sendTwoWayMediaTelemetry = true;
        driver._publishedLocalStream = true;
        var startTime = 1;
        sandbox.stub(performance, "now").returns(startTime);

        session.trigger("streamCreated", {stream: fakeStream});

        expect(driver._getTwoWayMediaStartTime()).to.eql(startTime);
      });

      it("should not store the start time when both streams are up and" +
         " driver._isDesktop is false", function() {
        driver._isDesktop = false ;
        driver._publishedLocalStream = true;
        var startTime = 73;
        sandbox.stub(performance, "now").returns(startTime);

        session.trigger("streamCreated", {stream: fakeStream});

        expect(driver._getTwoWayMediaStartTime()).to.not.eql(startTime);
      });


      it("should not dispatch a mediaConnected action for screen sharing streams",
        function() {
          driver._publishedLocalStream = true;
          fakeStream.videoType = "screen";

          session.trigger("streamCreated", {stream: fakeStream});

          sinon.assert.neverCalledWithMatch(dispatcher.dispatch,
            sinon.match.hasOwn("name", "mediaConnected"));
        });

      it("should not dispatch a ReceivingScreenShare action for camera streams",
        function() {
          session.trigger("streamCreated", {stream: fakeStream});

          sinon.assert.neverCalledWithMatch(dispatcher.dispatch,
            new sharedActions.ReceivingScreenShare({receiving: true}));
        });

      it("should dispatch a ReceivingScreenShare action for screen sharing streams",
        function() {
          fakeStream.videoType = "screen";

          session.trigger("streamCreated", {stream: fakeStream});

          // Called twice due to the VideoDimensionsChanged above.
          sinon.assert.calledTwice(dispatcher.dispatch);
          sinon.assert.calledWithMatch(dispatcher.dispatch,
            new sharedActions.ReceivingScreenShare({receiving: true}));
        });
    });

    describe("streamDestroyed", function() {
      var fakeStream;

      beforeEach(function() {
        fakeStream = {
          videoType: "screen"
        };
      });

      it("should dispatch a ReceivingScreenShare action", function() {
        session.trigger("streamDestroyed", {stream: fakeStream});

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.ReceivingScreenShare({
            receiving: false
          }));
      });

      it("should not dispatch an action if the videoType is camera", function() {
        fakeStream.videoType = "camera";

        session.trigger("streamDestroyed", {stream: fakeStream});

        sinon.assert.notCalled(dispatcher.dispatch);
      });
    });

    describe("streamPropertyChanged", function() {
      var fakeStream = {
        connection: { id: "fake" },
        videoType: "screen",
        videoDimensions: {
          width: 320,
          height: 160
        }
      };

      it("should not dispatch a VideoDimensionsChanged action for other properties", function() {
        session.trigger("streamPropertyChanged", {
          stream: fakeStream,
          changedProperty: STREAM_PROPERTIES.HAS_AUDIO
        });
        session.trigger("streamPropertyChanged", {
          stream: fakeStream,
          changedProperty: STREAM_PROPERTIES.HAS_VIDEO
        });

        sinon.assert.notCalled(dispatcher.dispatch);
      });

      it("should dispatch a VideoDimensionsChanged action", function() {
        session.connection = {
          id: "localUser"
        };
        session.trigger("streamPropertyChanged", {
          stream: fakeStream,
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
            connection: {id: "remoteUser"}
          });

          sinon.assert.calledOnce(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.RemotePeerConnected());
          it("should store the connection details for a remote user", function() {
            expect(driver.connections).to.include.keys("remoteUser");
          });
        });

      it("should not dispatch an action if this is for a local user",
        function() {
          session.trigger("connectionCreated", {
            connection: {id: "localUser"}
          });

          sinon.assert.notCalled(dispatcher.dispatch);
          it("should not store the connection details for a local user", function() {
            expect(driver.connections).to.not.include.keys("localUser");
          });
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

    describe("exception", function() {
      describe("Unable to publish (GetUserMedia)", function() {
        it("should destroy the publisher", function() {
          sdk.trigger("exception", {
            code: OT.ExceptionCodes.UNABLE_TO_PUBLISH,
            message: "GetUserMedia"
          });

          sinon.assert.calledOnce(publisher.destroy);
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

  describe("Events (screenshare)", function() {
    beforeEach(function() {
      driver.connectSession(sessionData);

      driver.getScreenShareElementFunc = function() {};

      driver.startScreenShare({
        videoSource: "window"
      });

      sandbox.stub(dispatcher, "dispatch");
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
  });
});
