"use strict"; /* Any copyright is dedicated to the Public Domain.
               * http://creativecommons.org/publicdomain/zero/1.0/ */

describe("loop.OTSdkDriver", function () {
  "use strict";

  var expect = chai.expect;
  var sharedActions = loop.shared.actions;
  var FAILURE_DETAILS = loop.shared.utils.FAILURE_DETAILS;
  var STREAM_PROPERTIES = loop.shared.utils.STREAM_PROPERTIES;
  var SCREEN_SHARE_STATES = loop.shared.utils.SCREEN_SHARE_STATES;
  var CHAT_CONTENT_TYPES = loop.shared.utils.CHAT_CONTENT_TYPES;
  var CURSOR_MESSAGE_TYPES = loop.shared.utils.CURSOR_MESSAGE_TYPES;

  var sandbox, constants;
  var dispatcher, driver, requestStubs, publisher, screenshare, sdk, session;
  var sessionData, subscriber, publisherConfig, fakeEvent;

  beforeEach(function () {
    sandbox = LoopMochaUtils.createSandbox();

    fakeEvent = { 
      preventDefault: sinon.stub() };

    publisherConfig = { 
      fake: "config" };

    sessionData = { 
      apiKey: "1234567890", 
      sessionId: "3216549870", 
      sessionToken: "1357924680" };


    LoopMochaUtils.stubLoopRequest(requestStubs = { 
      TelemetryAddValue: sinon.stub(), 
      GetLoopPref: sinon.stub() });


    dispatcher = new loop.Dispatcher();

    sandbox.stub(dispatcher, "dispatch");

    session = _.extend({ 
      connect: sinon.stub(), 
      disconnect: sinon.stub(), 
      publish: sinon.stub(), 
      unpublish: sinon.stub(), 
      signal: sinon.stub(), 
      subscribe: sinon.stub(), 
      forceDisconnect: sinon.stub() }, 
    Backbone.Events);

    publisher = _.extend({ 
      destroy: sinon.stub(), 
      publishAudio: sinon.stub(), 
      publishVideo: sinon.stub(), 
      _: { 
        getDataChannel: sinon.stub(), 
        switchAcquiredWindow: sinon.stub() } }, 

    Backbone.Events);

    screenshare = publisher;

    subscriber = _.extend({ 
      _: { 
        getDataChannel: sinon.stub() } });



    sdk = _.extend({ 
      DEBUG: "fakeDebug", 
      initPublisher: sinon.stub().returns(publisher), 
      initSession: sinon.stub().returns(session), 
      setLogLevel: sinon.stub() }, 
    Backbone.Events);

    window.OT = { 
      ExceptionCodes: { 
        CONNECT_FAILED: 1006, 
        TERMS_OF_SERVICE_FAILURE: 1026, 
        UNABLE_TO_PUBLISH: 1500 } };



    constants = { 
      TWO_WAY_MEDIA_CONN_LENGTH: { 
        SHORTER_THAN_10S: 0, 
        BETWEEN_10S_AND_30S: 1, 
        BETWEEN_30S_AND_5M: 2, 
        MORE_THAN_5M: 3 }, 

      SHARING_STATE_CHANGE: { 
        WINDOW_ENABLED: 0, 
        WINDOW_DISABLED: 1, 
        BROWSER_ENABLED: 2, 
        BROWSER_DISABLED: 3 } };



    driver = new loop.OTSdkDriver({ 
      constants: constants, 
      dispatcher: dispatcher, 
      sdk: sdk, 
      isDesktop: true });});



  afterEach(function () {
    sandbox.restore();
    LoopMochaUtils.restore();});


  describe("Constructor", function () {
    it("should throw an error if the constants are missing", function () {
      expect(function () {
        new loop.OTSdkDriver({ dispatcher: dispatcher, sdk: sdk });}).
      to.Throw(/constants/);});


    it("should throw an error if the dispatcher is missing", function () {
      expect(function () {
        new loop.OTSdkDriver({ constants: constants, sdk: sdk });}).
      to.Throw(/dispatcher/);});


    it("should throw an error if the sdk is missing", function () {
      expect(function () {
        new loop.OTSdkDriver({ constants: constants, dispatcher: dispatcher });}).
      to.Throw(/sdk/);});


    it("should set the metrics to zero", function () {
      driver = new loop.OTSdkDriver({ 
        constants: constants, 
        dispatcher: dispatcher, 
        sdk: sdk });


      expect(driver._metrics).eql({ 
        connections: 0, 
        sendStreams: 0, 
        recvStreams: 0 });});



    it("should enable debug for two way media telemetry if required", function () {
      // Simulate the pref being enabled.
      sandbox.stub(loop.shared.utils, "getBoolPreference", function (prefName, callback) {
        if (prefName === "debug.twoWayMediaTelemetry") {
          callback(true);}});



      driver = new loop.OTSdkDriver({ 
        constants: constants, 
        dispatcher: dispatcher, 
        sdk: sdk });


      expect(driver._debugTwoWayMediaTelemetry).eql(true);});


    it("should enable debug on the sdk if required", function () {
      // Simulate the pref being enabled.
      sandbox.stub(loop.shared.utils, "getBoolPreference", function (prefName, callback) {
        if (prefName === "debug.sdk") {
          callback(true);}});



      driver = new loop.OTSdkDriver({ 
        constants: constants, 
        dispatcher: dispatcher, 
        sdk: sdk });


      sinon.assert.calledOnce(sdk.setLogLevel);
      sinon.assert.calledWithExactly(sdk.setLogLevel, sdk.DEBUG);});});



  describe("#setupStreamElements", function () {
    beforeEach(function () {
      sandbox.stub(publisher, "off");
      driver.setupStreamElements(new sharedActions.SetupStreamElements({ 
        publisherConfig: publisherConfig }));});



    it("should call initPublisher", function () {
      var expectedConfig = _.extend({ 
        channels: { 
          text: {}, 
          cursor: { 
            reliable: true } } }, 


      publisherConfig);

      sinon.assert.calledOnce(sdk.initPublisher);
      sinon.assert.calledWith(sdk.initPublisher, 
      sinon.match.instanceOf(HTMLDivElement), 
      expectedConfig);});


    it("should not do anything if publisher completed successfully", function () {
      sdk.initPublisher.callArg(2);

      sinon.assert.notCalled(publisher.off);
      sinon.assert.notCalled(dispatcher.dispatch);});


    it("should clean up publisher if an error occurred", function () {
      sdk.initPublisher.callArgWith(2, { message: "FAKE" });

      sinon.assert.calledOnce(publisher.off);
      sinon.assert.calledOnce(publisher.destroy);
      expect(driver.publisher).to.equal(undefined);
      expect(driver._mockPublisherEl).to.equal(undefined);});


    it("should dispatch ConnectionFailure if an error occurred", function () {
      sdk.initPublisher.callArgWith(2, { message: "FAKE" });

      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWithExactly(dispatcher.dispatch, 
      new sharedActions.ConnectionFailure({ 
        reason: FAILURE_DETAILS.UNABLE_TO_PUBLISH_MEDIA }));});});




  describe("#setMute", function () {
    beforeEach(function () {
      sdk.initPublisher.returns(publisher);

      driver.setupStreamElements(new sharedActions.SetupStreamElements({ 
        publisherConfig: publisherConfig }));});



    it("should publishAudio with the correct enabled value", function () {
      driver.setMute(new sharedActions.SetMute({ 
        type: "audio", 
        enabled: false }));


      sinon.assert.calledOnce(publisher.publishAudio);
      sinon.assert.calledWithExactly(publisher.publishAudio, false);});


    it("should publishVideo with the correct enabled value", function () {
      driver.setMute(new sharedActions.SetMute({ 
        type: "video", 
        enabled: true }));


      sinon.assert.calledOnce(publisher.publishVideo);
      sinon.assert.calledWithExactly(publisher.publishVideo, true);});});



  describe("#startScreenShare", function () {
    var options = {};

    beforeEach(function () {
      sandbox.stub(screenshare, "off");
      options = { 
        videoSource: "browser", 
        constraints: { 
          browserWindow: 42, 
          scrollWithPage: true } };



      driver.startScreenShare(options);});


    it("should initialize a publisher", function () {
      sinon.assert.calledOnce(sdk.initPublisher);
      sinon.assert.calledWithMatch(sdk.initPublisher, 
      sinon.match.instanceOf(HTMLDivElement), options);});


    it("should not do anything if publisher completed successfully", function () {
      sdk.initPublisher.callArg(2);

      sinon.assert.notCalled(screenshare.off);
      sinon.assert.notCalled(dispatcher.dispatch);});


    it("should clean up publisher if an error occurred", function () {
      sdk.initPublisher.callArgWith(2, { code: 123, message: "FAKE" });

      sinon.assert.calledOnce(screenshare.off);
      sinon.assert.calledOnce(screenshare.destroy);
      expect(driver.screenshare).to.equal(undefined);
      expect(driver._mockScreenSharePreviewEl).to.equal(undefined);});


    it("should dispatch ConnectionFailure if an error occurred", function () {
      sdk.initPublisher.callArgWith(2, { code: 123, message: "FAKE" });

      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWithExactly(dispatcher.dispatch, 
      new sharedActions.ScreenSharingState({ 
        state: SCREEN_SHARE_STATES.INACTIVE }));});});




  describe("Screenshare Access Denied", function () {
    beforeEach(function () {
      sandbox.stub(screenshare, "off");
      var options = { 
        videoSource: "browser", 
        constraints: { 
          browserWindow: 42, 
          scrollWithPage: true } };


      driver.startScreenShare(options);
      sdk.initPublisher.callArg(2);
      driver.screenshare.trigger("accessDenied");});


    it("should clean up publisher", function () {
      sinon.assert.calledOnce(screenshare.off);
      sinon.assert.calledOnce(screenshare.destroy);
      expect(driver.screenshare).to.equal(undefined);
      expect(driver._mockScreenSharePreviewEl).to.equal(undefined);});


    it("should dispatch ConnectionFailure", function () {
      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWithExactly(dispatcher.dispatch, 
      new sharedActions.ScreenSharingState({ 
        state: SCREEN_SHARE_STATES.INACTIVE }));});});




  describe("#switchAcquiredWindow", function () {
    beforeEach(function () {
      var options = { 
        videoSource: "browser", 
        constraints: { 
          browserWindow: 42, 
          scrollWithPage: true } };


      driver.startScreenShare(options);});


    it("should switch to the acquired window", function () {
      driver.switchAcquiredWindow(72);

      sinon.assert.calledOnce(publisher._.switchAcquiredWindow);
      sinon.assert.calledWithExactly(publisher._.switchAcquiredWindow, 72);});


    it("should not switch if the window is the same as the currently selected one", function () {
      driver.switchAcquiredWindow(42);

      sinon.assert.notCalled(publisher._.switchAcquiredWindow);});});



  describe("#endScreenShare", function () {
    it("should unpublish the share", function () {
      driver.startScreenShare({ 
        videoSource: "window" });

      driver.session = session;

      driver.endScreenShare(new sharedActions.EndScreenShare());

      sinon.assert.calledOnce(session.unpublish);});


    it("should destroy the share", function () {
      driver.startScreenShare({ 
        videoSource: "window" });

      driver.session = session;

      expect(driver.endScreenShare()).to.equal(true);

      sinon.assert.calledOnce(publisher.destroy);});


    it("should unpublish the share too when type is 'browser'", function () {
      driver.startScreenShare({ 
        videoSource: "browser", 
        constraints: { 
          browserWindow: 42 } });


      driver.session = session;

      driver.endScreenShare(new sharedActions.EndScreenShare());

      sinon.assert.calledOnce(session.unpublish);});


    it("should dispatch a ConnectionStatus action", function () {
      driver.startScreenShare({ 
        videoSource: "browser", 
        constraints: { 
          browserWindow: 42 } });


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
        recvStreams: 1 }));});});




  describe("#connectSession", function () {
    it("should initialise a new session", function () {
      driver.connectSession(sessionData);

      sinon.assert.calledOnce(sdk.initSession);
      sinon.assert.calledWithExactly(sdk.initSession, "3216549870");});


    it("should connect the session", function () {
      driver.connectSession(sessionData);

      sinon.assert.calledOnce(session.connect);
      sinon.assert.calledWith(session.connect, "1234567890", "1357924680");});


    it("should set the two-way media start time to 'uninitialized' " + 
    "when sessionData.sendTwoWayMediaTelemetry is true'", function () {
      driver.connectSession(_.extend(sessionData, 
      { sendTwoWayMediaTelemetry: true }));

      expect(driver._getTwoWayMediaStartTime()).to.eql(
      driver.CONNECTION_START_TIME_UNINITIALIZED);});


    describe("On connection complete", function () {
      beforeEach(function () {
        sandbox.stub(window.console, "error");});


      it("should publish the stream if the publisher is ready", function () {
        driver._publisherReady = true;
        session.connect.callsArg(2);

        driver.connectSession(sessionData);

        sinon.assert.calledOnce(session.publish);});


      it("should notify metrics", function () {
        session.connect.callsArgWith(2, { 
          title: "Fake", 
          code: OT.ExceptionCodes.CONNECT_FAILED });


        driver.connectSession(sessionData);

        sinon.assert.calledTwice(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.ConnectionStatus({ 
          event: "sdk.exception." + OT.ExceptionCodes.CONNECT_FAILED, 
          state: "starting", 
          connections: 0, 
          sendStreams: 0, 
          recvStreams: 0 }));});



      it("should log an error message and error object", function () {
        session.connect.callsArgWith(2, { 
          title: "Fake", 
          code: OT.ExceptionCodes.CONNECT_FAILED });


        driver.connectSession(sessionData);

        sinon.assert.calledOnce(console.error);
        sinon.assert.calledWithExactly(console.error, sinon.match.string, { 
          title: "Fake", 
          code: OT.ExceptionCodes.CONNECT_FAILED });});



      it("should dispatch connectionFailure if connecting failed", function () {
        session.connect.callsArgWith(2, new Error("Failure"));

        driver.connectSession(sessionData);

        sinon.assert.calledTwice(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch, 
        sinon.match.hasOwn("name", "connectionFailure"));
        sinon.assert.calledWithMatch(dispatcher.dispatch, 
        sinon.match.hasOwn("reason", FAILURE_DETAILS.COULD_NOT_CONNECT));});});});




  describe("#disconnectSession", function () {
    it("should disconnect the session", function () {
      driver.session = session;

      driver.disconnectSession();

      sinon.assert.calledOnce(session.disconnect);});


    it("should unsubscribe to all the publisher events that were subscribed to in #setupStreamElements", function () {
      var subscribedEvents = [];

      // First find out which events were subscribed to.
      sandbox.stub(publisher, "on", function (eventName) {
        subscribedEvents.push(eventName);});


      driver.setupStreamElements(new sharedActions.SetupStreamElements({ 
        publisherConfig: publisherConfig }));


      // Now disconnect, checking for any unexpected unsubscribes, or any missed
      // unsubscribes.
      sandbox.stub(publisher, "off", function (eventNames) {
        var events = eventNames.split(" ");

        events.forEach(function (eventName) {
          var index = subscribedEvents.indexOf(eventName);

          expect(index).not.eql(-1);

          subscribedEvents.splice(index, 1);});});



      driver.disconnectSession();

      expect(subscribedEvents).eql([]);});


    it("should unsubscribe to all the subscriber events that were subscribed to in #connectSession", function () {
      var subscribedEvents = [];

      // First find out which events were subscribed to.
      sandbox.stub(session, "on", function (eventName) {
        subscribedEvents.push(eventName);});


      driver.connectSession(sessionData);

      // Now disconnect, checking for any unexpected unsubscribes, or any missed
      // unsubscribes.
      sandbox.stub(session, "off", function (eventNames) {
        var events = eventNames.split(" ");

        events.forEach(function (eventName) {
          var index = subscribedEvents.indexOf(eventName);

          expect(index).not.eql(-1);

          subscribedEvents.splice(index, 1);});});



      driver.disconnectSession();

      expect(subscribedEvents).eql([]);});


    it("should reset the metrics to zero", function () {
      driver._metrics = { 
        connections: 1, 
        sendStreams: 2, 
        recvStreams: 3 };


      driver.disconnectSession();

      expect(driver._metrics).eql({ 
        connections: 0, 
        sendStreams: 0, 
        recvStreams: 0 });});



    it("should dispatch a DataChannelsAvailable action with available = false", function () {
      driver.disconnectSession();

      sinon.assert.called(dispatcher.dispatch);
      sinon.assert.calledWithExactly(dispatcher.dispatch, 
      new sharedActions.DataChannelsAvailable({ 
        available: false }));});



    it("should dispatch a MediaStreamDestroyed action with isLocal = false", function () {
      driver.disconnectSession();

      sinon.assert.called(dispatcher.dispatch);
      sinon.assert.calledWithExactly(dispatcher.dispatch, 
      new sharedActions.MediaStreamDestroyed({ 
        isLocal: false }));});



    it("should dispatch a MediaStreamDestroyed action with isLocal = true", function () {
      driver.disconnectSession();

      sinon.assert.called(dispatcher.dispatch);
      sinon.assert.calledWithExactly(dispatcher.dispatch, 
      new sharedActions.MediaStreamDestroyed({ 
        isLocal: true }));});



    it("should destroy the publisher", function () {
      driver.publisher = publisher;

      driver.disconnectSession();

      sinon.assert.calledOnce(publisher.destroy);});


    it("should call _noteConnectionLengthIfNeeded with connection duration", function () {
      driver.session = session;
      var startTime = 1;
      var endTime = 3;
      driver._sendTwoWayMediaTelemetry = true;
      driver._setTwoWayMediaStartTime(startTime);
      sandbox.stub(performance, "now").returns(endTime);
      sandbox.stub(driver, "_noteConnectionLengthIfNeeded");

      driver.disconnectSession();

      sinon.assert.calledWith(driver._noteConnectionLengthIfNeeded, startTime, 
      endTime);});


    it("should reset the two-way media connection start time", function () {
      driver.session = session;
      var startTime = 1;
      driver._sendTwoWayMediaTelemetry = true;
      driver._setTwoWayMediaStartTime(startTime);
      sandbox.stub(performance, "now");
      sandbox.stub(driver, "_noteConnectionLengthIfNeeded");

      driver.disconnectSession();

      expect(driver._getTwoWayMediaStartTime()).to.eql(
      driver.CONNECTION_START_TIME_UNINITIALIZED);});});



  describe("#_noteConnectionLengthIfNeeded", function () {
    var startTimeMS;
    beforeEach(function () {
      startTimeMS = 1;
      driver._sendTwoWayMediaTelemetry = true;
      driver._setTwoWayMediaStartTime(startTimeMS);});


    it("should set two-way media start time to CONNECTION_START_TIME_ALREADY_NOTED", function () {
      var endTimeMS = 3;
      driver._noteConnectionLengthIfNeeded(startTimeMS, endTimeMS);

      expect(driver._getTwoWayMediaStartTime()).to.eql(
      driver.CONNECTION_START_TIME_ALREADY_NOTED);});


    it("should record telemetry with SHORTER_THAN_10S for calls less than 10s", function () {
      var endTimeMS = 9000;

      driver._noteConnectionLengthIfNeeded(startTimeMS, endTimeMS);

      sinon.assert.calledOnce(requestStubs.TelemetryAddValue);
      sinon.assert.calledWith(requestStubs.TelemetryAddValue, 
      "LOOP_TWO_WAY_MEDIA_CONN_LENGTH_1", 
      constants.TWO_WAY_MEDIA_CONN_LENGTH.SHORTER_THAN_10S);});


    it("should call record telemetry with BETWEEN_10S_AND_30S for 15s calls", 
    function () {
      var endTimeMS = 15000;

      driver._noteConnectionLengthIfNeeded(startTimeMS, endTimeMS);

      sinon.assert.calledOnce(requestStubs.TelemetryAddValue);
      sinon.assert.calledWith(requestStubs.TelemetryAddValue, 
      "LOOP_TWO_WAY_MEDIA_CONN_LENGTH_1", 
      constants.TWO_WAY_MEDIA_CONN_LENGTH.BETWEEN_10S_AND_30S);});


    it("should call record telemetry with BETWEEN_30S_AND_5M for 60s calls", 
    function () {
      var endTimeMS = 60 * 1000;

      driver._noteConnectionLengthIfNeeded(startTimeMS, endTimeMS);

      sinon.assert.calledOnce(requestStubs.TelemetryAddValue);
      sinon.assert.calledWith(requestStubs.TelemetryAddValue, 
      "LOOP_TWO_WAY_MEDIA_CONN_LENGTH_1", 
      constants.TWO_WAY_MEDIA_CONN_LENGTH.BETWEEN_30S_AND_5M);});


    it("should call record telemetry with MORE_THAN_5M for 10m calls", function () {
      var endTimeMS = 10 * 60 * 1000;

      driver._noteConnectionLengthIfNeeded(startTimeMS, endTimeMS);

      sinon.assert.calledOnce(requestStubs.TelemetryAddValue);
      sinon.assert.calledWith(requestStubs.TelemetryAddValue, 
      "LOOP_TWO_WAY_MEDIA_CONN_LENGTH_1", 
      constants.TWO_WAY_MEDIA_CONN_LENGTH.MORE_THAN_5M);});


    it("should not call record telemetry if driver._sendTwoWayMediaTelemetry is false", 
    function () {
      var endTimeMS = 10 * 60 * 1000;
      driver._sendTwoWayMediaTelemetry = false;

      driver._noteConnectionLengthIfNeeded(startTimeMS, endTimeMS);

      sinon.assert.notCalled(requestStubs.TelemetryAddValue);});});



  describe("#forceDisconnectAll", function () {
    it("should not disconnect anything when not connected", function () {
      driver.session = session;
      driver.forceDisconnectAll(function () {});

      sinon.assert.notCalled(session.forceDisconnect);});


    it("should disconnect all remote connections when called", function () {
      driver.connectSession(sessionData);
      sinon.assert.calledOnce(session.connect);
      driver._sessionConnected = true;

      // Setup the right state in the driver to make `forceDisconnectAll` do
      // something.
      session.connection = { 
        id: "localUser" };

      session.trigger("connectionCreated", { 
        connection: { id: "remoteUser" } });

      expect(driver.connections).to.include.keys("remoteUser");

      driver.forceDisconnectAll(function () {});
      sinon.assert.calledOnce(session.forceDisconnect);

      // Add another remote connection.
      session.trigger("connectionCreated", { 
        connection: { id: "remoteUser2" } });

      expect(driver.connections).to.include.keys("remoteUser", "remoteUser2");

      driver.forceDisconnectAll(function () {});
      sinon.assert.calledThrice(session.forceDisconnect);});


    describe("#sendTextChatMessage", function () {
      it("should send a message on the publisher data channel", function () {
        driver._publisherChannel = { 
          send: sinon.stub() };


        var message = { 
          contentType: CHAT_CONTENT_TYPES.TEXT, 
          message: "Help!" };


        driver.sendTextChatMessage(message);

        sinon.assert.calledOnce(driver._publisherChannel.send);
        sinon.assert.calledWithExactly(driver._publisherChannel.send, 
        JSON.stringify(message));});});});




  describe("#sendCursorMessage", function () {
    beforeEach(function () {
      driver.session = session;});


    it("should send a message on the publisher cursor data channel", function () {
      driver._publisherCursorChannel = { 
        send: sinon.stub() };


      driver._subscriberCursorChannel = {};

      var message = { 
        contentType: CURSOR_MESSAGE_TYPES.POSITION, 
        top: 10, 
        left: 10, 
        width: 100, 
        height: 100 };


      driver.sendCursorMessage(message);

      sinon.assert.calledOnce(driver._publisherCursorChannel.send);
      sinon.assert.calledWithExactly(driver._publisherCursorChannel.send, 
      JSON.stringify(message));});


    it("should not send a message if no cursor data channel has been set", function () {
      driver._publisherCursorChannel = { 
        send: sinon.stub() };


      driver._subscriberCursorChannel = null;

      var message = { 
        contentType: CURSOR_MESSAGE_TYPES.POSITION, 
        top: 10, 
        left: 10, 
        width: 100, 
        height: 100 };


      driver.sendCursorMessage(message);

      sinon.assert.notCalled(driver._publisherCursorChannel.send);});});



  describe("Events: general media", function () {
    var fakeConnection, fakeStream, fakeSubscriberObject, videoElement;

    beforeEach(function () {
      fakeConnection = "fakeConnection";
      fakeStream = { 
        hasAudio: true, 
        hasVideo: true, 
        videoType: "camera", 
        videoDimensions: { width: 1, height: 2 } };


      fakeSubscriberObject = _.extend({ 
        "_": { 
          getDataChannel: sinon.stub() }, 

        session: { connection: fakeConnection }, 
        stream: fakeStream }, 
      Backbone.Events);

      // use a real video element so that these tests correctly reflect
      // test behavior when run in firefox or chrome
      videoElement = document.createElement("video");

      driver.connectSession(sessionData);

      driver.setupStreamElements(new sharedActions.SetupStreamElements({ 
        publisherConfig: publisherConfig }));});



    describe("connectionDestroyed", function () {
      it("should dispatch a remotePeerDisconnected action if the client" + 
      "disconnected", function () {
        session.trigger("connectionDestroyed", { 
          reason: "clientDisconnected" });


        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch, 
        sinon.match.hasOwn("name", "remotePeerDisconnected"));
        sinon.assert.calledWithMatch(dispatcher.dispatch, 
        sinon.match.hasOwn("peerHungup", true));});


      it("should dispatch a remotePeerDisconnected action if the connection" + 
      "failed", function () {
        session.trigger("connectionDestroyed", { 
          reason: "networkDisconnected" });


        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch, 
        sinon.match.hasOwn("name", "remotePeerDisconnected"));
        sinon.assert.calledWithMatch(dispatcher.dispatch, 
        sinon.match.hasOwn("peerHungup", false));});


      it("should dispatch a ConnectionStatus action", function () {
        driver._metrics.connections = 1;

        session.trigger("connectionDestroyed", { 
          reason: "clientDisconnected" });


        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.ConnectionStatus({ 
          event: "Session.connectionDestroyed", 
          state: "waiting", 
          connections: 0, 
          sendStreams: 0, 
          recvStreams: 0 }));});



      it("should call _noteConnectionLengthIfNeeded with connection duration", function () {
        driver.session = session;
        var startTime = 1;
        var endTime = 3;
        driver._sendTwoWayMediaTelemetry = true;
        driver._setTwoWayMediaStartTime(startTime);
        sandbox.stub(performance, "now").returns(endTime);
        sandbox.stub(driver, "_noteConnectionLengthIfNeeded");

        session.trigger("connectionDestroyed", { 
          reason: "clientDisconnected" });


        sinon.assert.calledWith(driver._noteConnectionLengthIfNeeded, startTime, 
        endTime);});});



    describe("sessionDisconnected", function () {
      it("should notify metrics", function () {
        session.trigger("sessionDisconnected", { 
          reason: "networkDisconnected" });


        sinon.assert.calledTwice(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.ConnectionStatus({ 
          event: "Session.networkDisconnected", 
          state: "starting", 
          connections: 0, 
          sendStreams: 0, 
          recvStreams: 0 }));});



      it("should dispatch a connectionFailure action if the session was disconnected", 
      function () {
        session.trigger("sessionDisconnected", { 
          reason: "networkDisconnected" });


        sinon.assert.calledTwice(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch, 
        sinon.match.hasOwn("name", "connectionFailure"));
        sinon.assert.calledWithMatch(dispatcher.dispatch, 
        sinon.match.hasOwn("reason", FAILURE_DETAILS.NETWORK_DISCONNECTED));});


      it("should dispatch a connectionFailure action if the session was " + 
      "forcibly disconnected", function () {
        session.trigger("sessionDisconnected", { 
          reason: "forceDisconnected" });


        sinon.assert.calledTwice(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch, 
        sinon.match.hasOwn("name", "connectionFailure"));
        sinon.assert.calledWithMatch(dispatcher.dispatch, 
        sinon.match.hasOwn("reason", FAILURE_DETAILS.EXPIRED_OR_INVALID));});


      it("should call _noteConnectionLengthIfNeeded with connection duration", function () {
        driver.session = session;
        var startTime = 1;
        var endTime = 3;
        driver._sendTwoWayMediaTelemetry = true;
        driver._setTwoWayMediaStartTime(startTime);
        sandbox.stub(performance, "now").returns(endTime);
        sandbox.stub(driver, "_noteConnectionLengthIfNeeded");

        session.trigger("sessionDisconnected", { 
          reason: "networkDisconnected" });


        sinon.assert.calledWith(driver._noteConnectionLengthIfNeeded, startTime, 
        endTime);});});




    describe("streamCreated (publisher/local)", function () {
      var stream, fakeMockVideo;

      beforeEach(function () {
        driver._mockPublisherEl = document.createElement("div");
        fakeMockVideo = document.createElement("video");

        driver._mockPublisherEl.appendChild(fakeMockVideo);
        stream = { 
          hasAudio: true, 
          hasVideo: true, 
          videoType: "camera", 
          videoDimensions: { width: 1, height: 2 } };});



      it("should dispatch a VideoDimensionsChanged action", function () {
        publisher.trigger("streamCreated", { stream: stream });

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.VideoDimensionsChanged({ 
          isLocal: true, 
          videoType: "camera", 
          dimensions: { width: 1, height: 2 } }));});



      it("should dispatch a MediaStreamCreated action", function () {
        publisher.trigger("streamCreated", { stream: stream });

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.MediaStreamCreated({ 
          hasAudio: true, 
          hasVideo: true, 
          isLocal: true, 
          srcMediaElement: fakeMockVideo }));});



      it("should dispatch a MediaStreamCreated action with hasVideo false for audio-only streams", function () {
        stream.hasVideo = false;
        publisher.trigger("streamCreated", { stream: stream });

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.MediaStreamCreated({ 
          hasAudio: true, 
          hasVideo: false, 
          isLocal: true, 
          srcMediaElement: fakeMockVideo }));});



      it("should dispatch a ConnectionStatus action", function () {
        driver._metrics.recvStreams = 1;
        driver._metrics.connections = 2;

        publisher.trigger("streamCreated", { stream: stream });

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.ConnectionStatus({ 
          event: "Publisher.streamCreated", 
          state: "sendrecv", 
          connections: 2, 
          recvStreams: 1, 
          sendStreams: 1 }));});});




    describe("streamCreated: session/remote", function () {

      it("should dispatch a VideoDimensionsChanged action", function () {
        session.trigger("streamCreated", { stream: fakeStream });

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.VideoDimensionsChanged({ 
          isLocal: false, 
          videoType: "camera", 
          dimensions: { width: 1, height: 2 } }));});



      it("should dispatch a ConnectionStatus action", function () {
        driver._metrics.connections = 1;

        session.trigger("streamCreated", { stream: fakeStream });

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.ConnectionStatus({ 
          event: "Session.streamCreated", 
          state: "receiving", 
          connections: 1, 
          recvStreams: 1, 
          sendStreams: 0 }));});



      describe("Audio/Video streams", function () {
        beforeEach(function () {
          session.subscribe.yieldsOn(driver, null, fakeSubscriberObject, 
          videoElement).returns(this.fakeSubscriberObject);});


        it("should subscribe to a camera stream", function () {
          session.trigger("streamCreated", { stream: fakeStream });

          sinon.assert.calledOnce(session.subscribe);
          sinon.assert.calledWithExactly(session.subscribe, 
          fakeStream, sinon.match.instanceOf(HTMLDivElement), publisherConfig, 
          sinon.match.func);});


        it("should dispatch MediaStreamCreated after streamCreated is triggered on the session", function () {
          driver.session = session;
          fakeStream.connection = fakeConnection;
          fakeStream.hasVideo = true;
          fakeStream.hasAudio = true;

          session.trigger("streamCreated", { stream: fakeStream });

          sinon.assert.called(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch, 
          new sharedActions.MediaStreamCreated({ 
            hasAudio: true, 
            hasVideo: true, 
            isLocal: false, 
            srcMediaElement: videoElement }));});



        it("should dispatch MediaStreamCreated after streamCreated with audio-only indication if hasVideo=false", function () {
          fakeStream.connection = fakeConnection;
          fakeStream.hasVideo = false;

          session.trigger("streamCreated", { stream: fakeStream });

          sinon.assert.called(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch, 
          new sharedActions.MediaStreamCreated({ 
            hasAudio: true, 
            hasVideo: false, 
            isLocal: false, 
            srcMediaElement: videoElement }));});



        it("should dispatch a mediaConnected action if both streams are up", function () {
          driver._publishedLocalStream = true;

          session.trigger("streamCreated", { stream: fakeStream });

          // Called twice due to the VideoDimensionsChanged above.
          sinon.assert.called(dispatcher.dispatch);
          sinon.assert.calledWithMatch(dispatcher.dispatch, 
          new sharedActions.MediaConnected({}));});


        it("should store the start time when both streams are up and" + 
        " driver._sendTwoWayMediaTelemetry is true", function () {
          driver._sendTwoWayMediaTelemetry = true;
          driver._publishedLocalStream = true;
          var startTime = 1;
          sandbox.stub(performance, "now").returns(startTime);

          session.trigger("streamCreated", { stream: fakeStream });

          expect(driver._getTwoWayMediaStartTime()).to.eql(startTime);});


        it("should not store the start time when both streams are up and" + 
        " driver._isDesktop is false", function () {
          driver._isDesktop = false;
          driver._publishedLocalStream = true;
          var startTime = 73;
          sandbox.stub(performance, "now").returns(startTime);

          session.trigger("streamCreated", { stream: fakeStream });

          expect(driver._getTwoWayMediaStartTime()).to.not.eql(startTime);});


        describe("Data channel setup", function () {
          var fakeChannel;

          beforeEach(function () {
            fakeChannel = _.extend({}, Backbone.Events);
            fakeStream.connection = fakeConnection;
            driver._useDataChannels = true;

            sandbox.stub(console, "error");});


          it("should trigger a readyForDataChannel signal after subscribe is complete", function () {
            session.trigger("streamCreated", { stream: fakeStream });

            sinon.assert.calledOnce(session.signal);
            sinon.assert.calledWith(session.signal, { 
              type: "readyForDataChannel", 
              to: fakeConnection });});



          it("should not trigger readyForDataChannel signal if data channels are not wanted", function () {
            driver._useDataChannels = false;

            session.trigger("streamCreated", { stream: fakeStream });

            sinon.assert.notCalled(session.signal);});


          it("should get the data channel after subscribe is complete", function () {
            session.trigger("streamCreated", { stream: fakeStream });

            sinon.assert.calledTwice(fakeSubscriberObject._.getDataChannel);
            sinon.assert.calledWith(fakeSubscriberObject._.getDataChannel.getCall(0), "text", {});
            sinon.assert.calledWith(fakeSubscriberObject._.getDataChannel.getCall(1), "cursor", {});});


          it("should not get the data channel if data channels are not wanted", function () {
            driver._useDataChannels = false;

            session.trigger("streamCreated", { stream: fakeStream });

            sinon.assert.notCalled(fakeSubscriberObject._.getDataChannel);});


          it("should log an error if the data channel couldn't be obtained", function () {
            var err = new Error("fakeError");

            fakeSubscriberObject._.getDataChannel.callsArgWith(2, err);

            session.trigger("streamCreated", { stream: fakeStream });

            sinon.assert.calledTwice(console.error);
            sinon.assert.calledWithMatch(console.error, err);});


          it("should dispatch ConnectionStatus if the data channel couldn't be obtained", function () {
            fakeSubscriberObject._.getDataChannel.callsArgWith(2, new Error("fakeError"));

            session.trigger("streamCreated", { stream: fakeStream });

            sinon.assert.called(dispatcher.dispatch);
            sinon.assert.calledWithExactly(dispatcher.dispatch, 
            new sharedActions.ConnectionStatus({ 
              connections: 0, 
              event: "sdk.datachannel.sub.text.fakeError", 
              sendStreams: 0, 
              state: "receiving", 
              recvStreams: 1 }));});



          it("should dispatch `DataChannelsAvailable` if the publisher channel is setup", function () {
            // Fake a publisher channel.
            driver._publisherChannel = {};

            fakeSubscriberObject._.getDataChannel.callsArgWith(2, null, fakeChannel);

            session.trigger("streamCreated", { stream: fakeStream });

            sinon.assert.called(dispatcher.dispatch);
            sinon.assert.calledWithExactly(dispatcher.dispatch, 
            new sharedActions.DataChannelsAvailable({ 
              available: true }));});



          it("should not dispatch `DataChannelsAvailable` if the publisher channel isn't setup", function () {
            fakeSubscriberObject._.getDataChannel.callsArgWith(2, null, fakeChannel);

            session.trigger("streamCreated", { stream: fakeStream });

            sinon.assert.neverCalledWith(dispatcher.dispatch, 
            new sharedActions.DataChannelsAvailable({ 
              available: true }));});



          it("should dispatch `ReceivedTextChatMessage` when a text message is received", function () {
            var data = '{"contentType":"' + CHAT_CONTENT_TYPES.TEXT + 
            '","message":"Are you there?","receivedTimestamp": "2015-06-25T00:29:14.197Z"}';
            var clock = sinon.useFakeTimers();

            fakeSubscriberObject._.getDataChannel.callsArgWith(2, null, fakeChannel);

            session.trigger("streamCreated", { stream: fakeStream });

            // Now send the message.
            fakeChannel.trigger("message", { 
              data: data });


            sinon.assert.called(dispatcher.dispatch);
            sinon.assert.calledWithExactly(dispatcher.dispatch, 
            new sharedActions.ReceivedTextChatMessage({ 
              contentType: CHAT_CONTENT_TYPES.TEXT, 
              message: "Are you there?", 
              receivedTimestamp: "1970-01-01T00:00:00.000Z" }));


            /* Restore the time. */
            clock.restore();});});});




      describe("screen sharing streams", function () {
        it("should subscribe to a screen sharing stream", function () {
          fakeStream.videoType = "screen";

          session.trigger("streamCreated", { stream: fakeStream });

          sinon.assert.calledOnce(session.subscribe);
          sinon.assert.calledWithExactly(session.subscribe, 
          fakeStream, sinon.match.instanceOf(HTMLDivElement), publisherConfig, 
          sinon.match.func);});


        it("should not dispatch a mediaConnected action for screen sharing streams", function () {
          driver._publishedLocalStream = true;
          fakeStream.videoType = "screen";

          session.trigger("streamCreated", { stream: fakeStream });

          sinon.assert.neverCalledWithMatch(dispatcher.dispatch, 
          sinon.match.hasOwn("name", "mediaConnected"));});


        it("should not dispatch a ReceivingScreenShare action for camera streams", function () {
          session.trigger("streamCreated", { stream: fakeStream });

          sinon.assert.neverCalledWithMatch(dispatcher.dispatch, 
          new sharedActions.ReceivingScreenShare({ receiving: true }));});


        it("should dispatch a VideoScreenStreamChanged action for paused screen sharing streams", function () {
          fakeStream.videoType = "screen";
          fakeStream.hasVideo = false;

          session.trigger("streamCreated", { stream: fakeStream });

          // Called twice due to the VideoDimensionsChanged above.
          sinon.assert.called(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch, 
          new sharedActions.VideoScreenStreamChanged({ hasVideo: false }));});


        it("should dispatch a ReceivingScreenShare action for screen sharing streams", function () {
          fakeStream.videoType = "screen";

          session.trigger("streamCreated", { stream: fakeStream });

          // Called twice due to the VideoDimensionsChanged above.
          sinon.assert.called(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch, 
          new sharedActions.ReceivingScreenShare({ receiving: true }));});});});




    describe("streamDestroyed: publisher/local", function () {
      it("should dispatch a ConnectionStatus action", function () {
        driver._metrics.sendStreams = 1;
        driver._metrics.recvStreams = 1;
        driver._metrics.connections = 2;

        publisher.trigger("streamDestroyed");

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.ConnectionStatus({ 
          event: "Publisher.streamDestroyed", 
          state: "receiving", 
          connections: 2, 
          recvStreams: 1, 
          sendStreams: 0 }));});



      it("should dispatch a DataChannelsAvailable action", function () {
        publisher.trigger("streamDestroyed");

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.DataChannelsAvailable({ 
          available: false }));});



      it("should dispatch a MediaStreamDestroyed action", function () {
        publisher.trigger("streamDestroyed");

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.MediaStreamDestroyed({ 
          isLocal: true }));});});




    describe("streamDestroyed: session/remote", function () {
      var stream;

      beforeEach(function () {
        stream = { 
          videoType: "screen" };});



      it("should dispatch a ReceivingScreenShare action", function () {
        session.trigger("streamDestroyed", { stream: stream });

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.ReceivingScreenShare({ 
          receiving: false }));});



      it("should dispatch a ConnectionStatus action", function () {
        driver._metrics.connections = 2;
        driver._metrics.sendStreams = 1;
        driver._metrics.recvStreams = 1;

        session.trigger("streamDestroyed", { stream: stream });

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.ConnectionStatus({ 
          event: "Session.streamDestroyed", 
          state: "sending", 
          connections: 2, 
          recvStreams: 0, 
          sendStreams: 1 }));});



      it("should not dispatch a ConnectionStatus action if the videoType is camera", function () {
        stream.videoType = "camera";

        session.trigger("streamDestroyed", { stream: stream });

        sinon.assert.neverCalledWithMatch(dispatcher.dispatch, 
        sinon.match.hasOwn("name", "receivingScreenShare"));});


      it("should dispatch a DataChannelsAvailable action for videoType = camera", function () {
        stream.videoType = "camera";

        session.trigger("streamDestroyed", { stream: stream });

        sinon.assert.calledThrice(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.DataChannelsAvailable({ 
          available: false }));});



      it("should not dispatch a DataChannelsAvailable action for videoType = screen", function () {
        session.trigger("streamDestroyed", { stream: stream });

        sinon.assert.neverCalledWithMatch(dispatcher.dispatch, 
        sinon.match.hasOwn("name", "dataChannelsAvailable"));});


      it("should dispatch a MediaStreamDestroyed action for videoType = camera", function () {
        stream.videoType = "camera";

        session.trigger("streamDestroyed", { stream: stream });

        sinon.assert.calledThrice(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.MediaStreamDestroyed({ 
          isLocal: false }));});



      it("should not dispatch a MediaStreamDestroyed action for videoType = screen", function () {
        session.trigger("streamDestroyed", { stream: stream });

        sinon.assert.neverCalledWithMatch(dispatcher.dispatch, 
        new sharedActions.MediaStreamDestroyed({ 
          isLocal: false }));});});




    describe("streamPropertyChanged", function () {
      var stream = { 
        connection: { id: "fake" }, 
        videoType: "screen", 
        videoDimensions: { 
          width: 320, 
          height: 160 } };



      it("should dispatch a VideoDimensionsChanged action", function () {
        session.connection = { 
          id: "localUser" };

        session.trigger("streamPropertyChanged", { 
          stream: stream, 
          changedProperty: STREAM_PROPERTIES.VIDEO_DIMENSIONS });


        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch, 
        sinon.match.hasOwn("name", "videoDimensionsChanged"));});


      it("should dispatch a VideoScreenShareChanged action", function () {
        session.connection = { 
          id: "localUser" };

        session.trigger("streamPropertyChanged", { 
          stream: stream, 
          oldValue: false, 
          newValue: true, 
          changedProperty: STREAM_PROPERTIES.HAS_VIDEO });


        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch, 
        sinon.match.hasOwn("name", "videoScreenStreamChanged"));});});



    describe("connectionCreated", function () {
      beforeEach(function () {
        session.connection = { 
          id: "localUser" };});



      it("should dispatch a RemotePeerConnected action if this is for a remote user", 
      function () {
        session.trigger("connectionCreated", { 
          connection: { id: "remoteUser" } });


        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.RemotePeerConnected());});


      it("should store the connection details for a remote user", function () {
        session.trigger("connectionCreated", { 
          connection: { id: "remoteUser" } });


        expect(driver.connections).to.include.keys("remoteUser");});


      it("should dispatch a ConnectionStatus action for a remote user", function () {
        driver._metrics.connections = 1;
        driver._metrics.sendStreams = 1;

        session.trigger("connectionCreated", { 
          connection: { id: "remoteUser" } });


        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.ConnectionStatus({ 
          event: "Session.connectionCreated", 
          state: "sending", 
          connections: 2, 
          recvStreams: 0, 
          sendStreams: 1 }));});



      it("should not dispatch an RemotePeerConnected action if this is for a local user", 
      function () {
        session.trigger("connectionCreated", { 
          connection: { id: "localUser" } });


        sinon.assert.neverCalledWithMatch(dispatcher.dispatch, 
        sinon.match.hasOwn("name", "remotePeerConnected"));});


      it("should not store the connection details for a local user", function () {
        session.trigger("connectionCreated", { 
          connection: { id: "localUser" } });


        expect(driver.connections).to.not.include.keys("localUser");});


      it("should dispatch a ConnectionStatus action for a remote user", function () {
        driver._metrics.connections = 0;
        driver._metrics.sendStreams = 0;

        session.trigger("connectionCreated", { 
          connection: { id: "localUser" } });


        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.ConnectionStatus({ 
          event: "Session.connectionCreated", 
          state: "waiting", 
          connections: 1, 
          recvStreams: 0, 
          sendStreams: 0 }));});});





    describe("accessAllowed", function () {
      it("should publish the stream if the connection is ready", function () {
        driver._sessionConnected = true;

        publisher.trigger("accessAllowed", fakeEvent);

        sinon.assert.calledOnce(session.publish);});


      it("should dispatch a GotMediaPermission action", function () {
        publisher.trigger("accessAllowed", fakeEvent);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.GotMediaPermission());});});



    describe("accessDenied", function () {
      it("should prevent the default event behavior", function () {
        publisher.trigger("accessDenied", fakeEvent);

        sinon.assert.calledOnce(fakeEvent.preventDefault);});


      it("should dispatch connectionFailure", function () {
        publisher.trigger("accessDenied", fakeEvent);

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch, 
        sinon.match.hasOwn("name", "connectionFailure"));
        sinon.assert.calledWithMatch(dispatcher.dispatch, 
        sinon.match.hasOwn("reason", FAILURE_DETAILS.MEDIA_DENIED));});});



    describe("accessDialogOpened", function () {
      it("should prevent the default event behavior", function () {
        publisher.trigger("accessDialogOpened", fakeEvent);

        sinon.assert.calledOnce(fakeEvent.preventDefault);});});



    describe("videoEnabled", function () {
      it("should dispatch a RemoteVideoStatus action", function () {
        session.subscribe.yieldsOn(driver, null, fakeSubscriberObject, 
        videoElement).returns(this.fakeSubscriberObject);
        session.trigger("streamCreated", { stream: fakeSubscriberObject.stream });
        driver._mockSubscribeEl.appendChild(videoElement);

        fakeSubscriberObject.trigger("videoEnabled");

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWith(dispatcher.dispatch, 
        new sharedActions.RemoteVideoStatus({ 
          videoEnabled: true }));});});




    describe("videoDisabled", function () {
      it("should dispatch a RemoteVideoStatus action", function () {
        session.subscribe.yieldsOn(driver, null, fakeSubscriberObject, 
        videoElement).returns(this.fakeSubscriberObject);
        session.trigger("streamCreated", { stream: fakeSubscriberObject.stream });


        fakeSubscriberObject.trigger("videoDisabled");

        sinon.assert.called(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.RemoteVideoStatus({ 
          videoEnabled: false }));});});




    describe("signal:readyForDataChannel", function () {
      beforeEach(function () {
        driver.subscriber = subscriber;
        driver._useDataChannels = true;
        sandbox.stub(console, "error");});


      it("should not do anything if data channels are not wanted", function () {
        driver._useDataChannels = false;

        session.trigger("signal:readyForDataChannel");

        sinon.assert.notCalled(publisher._.getDataChannel);
        sinon.assert.notCalled(subscriber._.getDataChannel);});


      it("should get the data channels for the publisher", function () {
        session.trigger("signal:readyForDataChannel");

        sinon.assert.calledTwice(publisher._.getDataChannel);});


      it("should log an error if the data channels couldn't be obtained", function () {
        var err = new Error("fakeError");

        publisher._.getDataChannel.callsArgWith(2, err);

        session.trigger("signal:readyForDataChannel");

        sinon.assert.calledTwice(console.error);
        sinon.assert.calledWithMatch(console.error, err);});


      it("should dispatch ConnectionStatus if the data channel couldn't be obtained", function () {
        publisher._.getDataChannel.callsArgWith(2, new Error("fakeError"));

        session.trigger("signal:readyForDataChannel");

        sinon.assert.calledTwice(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.ConnectionStatus({ 
          connections: 0, 
          event: "sdk.datachannel.pub.text.fakeError", 
          sendStreams: 0, 
          state: "starting", 
          recvStreams: 0 }));});



      it("should dispatch `DataChannelsAvailable` if the subscriber channel is setup", function () {
        var fakeChannel = _.extend({}, Backbone.Events);

        driver._subscriberChannel = fakeChannel;

        publisher._.getDataChannel.callsArgWith(2, null, fakeChannel);

        session.trigger("signal:readyForDataChannel");

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.DataChannelsAvailable({ 
          available: true }));});



      it("should not dispatch `DataChannelsAvailable` if the subscriber channel isn't setup", function () {
        var fakeChannel = _.extend({}, Backbone.Events);

        publisher._.getDataChannel.callsArgWith(2, null, fakeChannel);

        session.trigger("signal:readyForDataChannel");

        sinon.assert.neverCalledWith(dispatcher.dispatch, 
        new sharedActions.DataChannelsAvailable({ 
          available: true }));});});




    describe("exception", function () {
      it("should notify metrics", function () {
        sdk.trigger("exception", { 
          code: OT.ExceptionCodes.CONNECT_FAILED, 
          message: "Fake", 
          title: "Connect Failed" });


        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.ConnectionStatus({ 
          event: "sdk.exception." + OT.ExceptionCodes.CONNECT_FAILED, 
          state: "starting", 
          connections: 0, 
          sendStreams: 0, 
          recvStreams: 0 }));});



      describe("ToS Failure", function () {
        it("should dispatch a ConnectionFailure action", function () {
          sdk.trigger("exception", { 
            code: OT.ExceptionCodes.TERMS_OF_SERVICE_FAILURE, 
            message: "Fake" });


          sinon.assert.calledTwice(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch, 
          new sharedActions.ConnectionFailure({ 
            reason: FAILURE_DETAILS.TOS_FAILURE }));});



        it("should notify metrics", function () {
          sdk.trigger("exception", { 
            code: OT.ExceptionCodes.TERMS_OF_SERVICE_FAILURE, 
            message: "Fake" });


          sinon.assert.calledTwice(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch, 
          new sharedActions.ConnectionStatus({ 
            event: "sdk.exception." + OT.ExceptionCodes.TERMS_OF_SERVICE_FAILURE, 
            state: "starting", 
            connections: 0, 
            sendStreams: 0, 
            recvStreams: 0 }));});});




      describe("ICE failed", function () {
        it("should dispatch a ConnectionFailure action (Publisher)", function () {
          sdk.trigger("exception", { 
            code: OT.ExceptionCodes.PUBLISHER_ICE_WORKFLOW_FAILED, 
            message: "ICE failed" });


          sinon.assert.calledTwice(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch, 
          new sharedActions.ConnectionFailure({ 
            reason: FAILURE_DETAILS.ICE_FAILED }));});



        it("should dispatch a ConnectionFailure action (Subscriber)", function () {
          sdk.trigger("exception", { 
            code: OT.ExceptionCodes.SUBSCRIBER_ICE_WORKFLOW_FAILED, 
            message: "ICE failed" });


          sinon.assert.calledTwice(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch, 
          new sharedActions.ConnectionFailure({ 
            reason: FAILURE_DETAILS.ICE_FAILED }));});



        it("should notify metrics", function () {
          sdk.trigger("exception", { 
            code: OT.ExceptionCodes.PUBLISHER_ICE_WORKFLOW_FAILED, 
            message: "ICE failed" });


          sinon.assert.calledTwice(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch, 
          new sharedActions.ConnectionStatus({ 
            event: "sdk.exception." + OT.ExceptionCodes.PUBLISHER_ICE_WORKFLOW_FAILED, 
            state: "starting", 
            connections: 0, 
            sendStreams: 0, 
            recvStreams: 0 }));});});




      describe("Unable to Publish", function () {
        it("should not do anything if the message is 'GetUserMedia'", function () {
          sdk.trigger("exception", { 
            code: OT.ExceptionCodes.UNABLE_TO_PUBLISH, 
            message: "GetUserMedia" });


          sinon.assert.notCalled(dispatcher.dispatch);});


        it("should notify metrics", function () {
          sdk.trigger("exception", { 
            code: OT.ExceptionCodes.UNABLE_TO_PUBLISH, 
            message: "General Media Fail" });


          sinon.assert.calledOnce(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch, 
          new sharedActions.ConnectionStatus({ 
            event: "sdk.exception." + OT.ExceptionCodes.UNABLE_TO_PUBLISH + 
            ".General Media Fail", 
            state: "starting", 
            connections: 0, 
            sendStreams: 0, 
            recvStreams: 0 }));});



        it("should notify metrics with a special screen indication for screen shares", function () {
          driver.screenshare = { fake: true };

          sdk.trigger("exception", { 
            code: OT.ExceptionCodes.UNABLE_TO_PUBLISH, 
            message: "General Media Fail 2", 
            target: driver.screenshare });


          sinon.assert.calledOnce(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch, 
          new sharedActions.ConnectionStatus({ 
            event: "sdk.exception.screen." + OT.ExceptionCodes.UNABLE_TO_PUBLISH + 
            ".General Media Fail 2", 
            state: "starting", 
            connections: 0, 
            sendStreams: 0, 
            recvStreams: 0 }));});});});});






  describe("Events: screenshare:", function () {
    beforeEach(function () {
      driver.connectSession(sessionData);

      driver.startScreenShare({ 
        videoSource: "window" });});



    describe("accessAllowed", function () {
      it("should publish the stream", function () {
        publisher.trigger("accessAllowed", fakeEvent);

        sinon.assert.calledOnce(session.publish);});


      it("should dispatch a `ScreenSharingState` action", function () {
        publisher.trigger("accessAllowed", fakeEvent);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.ScreenSharingState({ 
          state: SCREEN_SHARE_STATES.ACTIVE }));});});




    describe("accessDenied", function () {
      it("should dispatch a `ScreenShareState` action", function () {
        publisher.trigger("accessDenied", fakeEvent);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.ScreenSharingState({ 
          state: SCREEN_SHARE_STATES.INACTIVE }));});});




    describe("streamCreated", function () {
      it("should dispatch a ConnectionStatus action", function () {
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
          sendStreams: 2 }));});});});});
