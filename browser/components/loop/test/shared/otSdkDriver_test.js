/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var expect = chai.expect;

describe("loop.OTSdkDriver", function () {
  "use strict";

  var sharedActions = loop.shared.actions;
  var FAILURE_REASONS = loop.shared.utils.FAILURE_REASONS;
  var sandbox;
  var dispatcher, driver, publisher, sdk, session, sessionData;
  var fakeLocalElement, fakeRemoteElement, publisherConfig, fakeEvent;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();

    fakeLocalElement = {fake: 1};
    fakeRemoteElement = {fake: 2};
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
      subscribe: sinon.stub()
    }, Backbone.Events);

    publisher = _.extend({
      destroy: sinon.stub(),
      publishAudio: sinon.stub(),
      publishVideo: sinon.stub()
    }, Backbone.Events);

    sdk = {
      initPublisher: sinon.stub().returns(publisher),
      initSession: sinon.stub().returns(session)
    };

    driver = new loop.OTSdkDriver({
      dispatcher: dispatcher,
      sdk: sdk
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
          sinon.match.hasOwn("reason", FAILURE_REASONS.COULD_NOT_CONNECT));
      });
    });
  });

  describe("#disconnectionSession", function() {
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
  });

  describe("Events", function() {
    beforeEach(function() {
      driver.connectSession(sessionData);

      dispatcher.dispatch(new sharedActions.SetupStreamElements({
        getLocalElementFunc: function() {return fakeLocalElement;},
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
            sinon.match.hasOwn("reason", FAILURE_REASONS.NETWORK_DISCONNECTED));
        });
    });

    describe("streamCreated", function() {
      var fakeStream;

      beforeEach(function() {
        fakeStream = {
          fakeStream: 3
        };
      });

      it("should subscribe to the stream", function() {
        session.trigger("streamCreated", {stream: fakeStream});

        sinon.assert.calledOnce(session.subscribe);
        sinon.assert.calledWithExactly(session.subscribe,
          fakeStream, fakeRemoteElement, publisherConfig);
      });

      it("should dispach a mediaConnected action if both streams are up", function() {
        driver._publishedLocalStream = true;

        session.trigger("streamCreated", {stream: fakeStream});

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "mediaConnected"));
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
        });

      it("should not dispatch an action if this is for a local user",
        function() {
          session.trigger("connectionCreated", {
            connection: {id: "localUser"}
          });

          sinon.assert.notCalled(dispatcher.dispatch);
        });
    });

    describe("accessAllowed", function() {
      it("should publish the stream if the connection is ready", function() {
        driver._sessionConnected = true;

        publisher.trigger("accessAllowed", fakeEvent);

        sinon.assert.calledOnce(session.publish);
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
          sinon.match.hasOwn("reason", FAILURE_REASONS.MEDIA_DENIED));
      });
    });

    describe("accessDialogOpened", function() {
      it("should prevent the default event behavior", function() {
        publisher.trigger("accessDialogOpened", fakeEvent);

        sinon.assert.calledOnce(fakeEvent.preventDefault);
      });
    });
  });
});
