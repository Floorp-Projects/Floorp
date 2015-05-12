/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop, sinon */

var expect = chai.expect;
var TestUtils = React.addons.TestUtils;

describe("loop.webapp", function() {
  "use strict";

  var sharedActions = loop.shared.actions;
  var sharedModels = loop.shared.models,
      sharedViews = loop.shared.views,
      sharedUtils = loop.shared.utils,
      standaloneMedia = loop.standaloneMedia,
      sandbox,
      notifications,
      stubGetPermsAndCacheMedia,
      fakeAudioXHR,
      dispatcher,
      WEBSOCKET_REASONS = loop.shared.utils.WEBSOCKET_REASONS;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    dispatcher = new loop.Dispatcher();
    notifications = new sharedModels.NotificationCollection();
    loop.store.StoreMixin.register({
      feedbackStore: new loop.store.FeedbackStore(dispatcher, {
        feedbackClient: {}
      })
    });

    stubGetPermsAndCacheMedia = sandbox.stub(
      loop.standaloneMedia._MultiplexGum.prototype, "getPermsAndCacheMedia");

    fakeAudioXHR = {
      open: sinon.spy(),
      send: function() {},
      abort: function() {},
      getResponseHeader: function(header) {
        if (header === "Content-Type") {
          return "audio/ogg";
        }
      },
      responseType: null,
      response: new ArrayBuffer(10),
      onload: null
    };
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("#init", function() {
    beforeEach(function() {
      sandbox.stub(React, "render");
      loop.config.feedbackApiUrl = "http://fake.invalid";
      sandbox.stub(loop.Dispatcher.prototype, "dispatch");
    });

    it("should create the WebappRootView", function() {
      loop.webapp.init();

      sinon.assert.calledOnce(React.render);
      sinon.assert.calledWith(React.render,
        sinon.match(function(value) {
          return TestUtils.isCompositeComponentElement(value,
            loop.webapp.WebappRootView);
      }));
    });

    it("should dispatch a ExtractTokenInfo action with the path and hash",
      function() {
        sandbox.stub(loop.shared.utils, "locationData").returns({
          hash: "#fakeKey",
          pathname: "/c/faketoken"
        });

      loop.webapp.init();

      sinon.assert.calledOnce(loop.Dispatcher.prototype.dispatch);
      sinon.assert.calledWithExactly(loop.Dispatcher.prototype.dispatch,
        new sharedActions.ExtractTokenInfo({
          windowPath: "/c/faketoken",
          windowHash: "#fakeKey"
        }));
    });
  });

  describe("OutgoingConversationView", function() {
    var ocView, conversation, client;

    function mountTestComponent(props) {
      return TestUtils.renderIntoDocument(
        React.createElement(
          loop.webapp.OutgoingConversationView, props));
    }

    beforeEach(function() {
      client = new loop.StandaloneClient({
        baseServerUrl: "http://fake.example.com"
      });
      sandbox.stub(client, "requestCallInfo");
      sandbox.stub(client, "requestCallUrlInfo");
      conversation = new sharedModels.ConversationModel({}, {
        sdk: {}
      });
      conversation.set("loopToken", "fakeToken");
      ocView = mountTestComponent({
        client: client,
        conversation: conversation,
        notifications: notifications,
        sdk: {
          on: sandbox.stub()
        },
        dispatcher: dispatcher
      });
    });

    describe("start", function() {
      it("should display the StartConversationView", function() {
        TestUtils.findRenderedComponentWithType(ocView,
          loop.webapp.StartConversationView);
      });
    });

    // This is tested separately to ease testing, although it isn't really a
    // public API. This will probably be refactored soon anyway.
    describe("#_setupWebSocket", function() {
      beforeEach(function() {
        conversation.setOutgoingSessionData({
          sessionId:      "sessionId",
          sessionToken:   "sessionToken",
          apiKey:         "apiKey",
          callId:         "Hello",
          progressURL:    "http://invalid/url",
          websocketToken: 123
        });
      });

      describe("Websocket connection successful", function() {
        var promise;

        beforeEach(function() {
          sandbox.stub(loop, "CallConnectionWebSocket").returns({
            promiseConnect: function() {
              promise = new Promise(function(resolve, reject) {
                resolve();
              });
              return promise;
            },

            on: sandbox.spy()
          });
        });

        it("should create a CallConnectionWebSocket", function(done) {
          ocView._setupWebSocket();

          promise.then(function () {
            sinon.assert.calledOnce(loop.CallConnectionWebSocket);
            sinon.assert.calledWithExactly(loop.CallConnectionWebSocket, {
              callId: "Hello",
              url: "http://invalid/url",
              // The websocket token is converted to a hex string.
              websocketToken: "7b"
            });
            done();
          });
        });
      });

      describe("Websocket connection failed", function() {
        var promise;

        beforeEach(function() {
          sandbox.stub(loop, "CallConnectionWebSocket").returns({
            promiseConnect: function() {
              promise = new Promise(function(resolve, reject) {
                reject();
              });
              return promise;
            },

            on: sandbox.spy()
          });
        });

        it("should display an error", function(done) {
          sandbox.stub(notifications, "errorL10n");
          ocView._setupWebSocket();

          promise.then(function() {
          }, function () {
            sinon.assert.calledOnce(notifications.errorL10n);
            sinon.assert.calledWithExactly(notifications.errorL10n,
              "cannot_start_call_session_not_ready");
            done();
          });
        });
      });

      describe("Websocket Events", function() {
        beforeEach(function() {
          conversation.setOutgoingSessionData({
            sessionId:      "sessionId",
            sessionToken:   "sessionToken",
            apiKey:         "apiKey",
            callId:         "Hello",
            progressURL:    "http://progress.example.com",
            websocketToken: 123
          });

          sandbox.stub(loop.CallConnectionWebSocket.prototype,
                       "promiseConnect").returns({
            then: sandbox.spy()
          });

          ocView._setupWebSocket();
        });

        describe("Progress", function() {
          describe("state: terminate, reason: reject", function() {
            beforeEach(function() {
              sandbox.stub(notifications, "errorL10n");
              sandbox.stub(window, "XMLHttpRequest").returns(fakeAudioXHR);
            });

            it("should display the FailedConversationView", function() {
              ocView._websocket.trigger("progress", {
                state: "terminated",
                reason: WEBSOCKET_REASONS.REJECT
              });

              TestUtils.findRenderedComponentWithType(ocView,
                loop.webapp.FailedConversationView);
            });

            it("should reset multiplexGum when a call is rejected",
              function() {
                var multiplexGum = new standaloneMedia._MultiplexGum();
                standaloneMedia.setSingleton(multiplexGum);
                sandbox.stub(standaloneMedia._MultiplexGum.prototype, "reset");

                ocView._websocket.trigger("progress", {
                  state: "terminated",
                  reason: WEBSOCKET_REASONS.REJECT
                });

                sinon.assert.calledOnce(multiplexGum.reset);
              });

            it("should display an error message if the reason is not WEBSOCKET_REASONS.CANCEL",
              function() {
                ocView._websocket.trigger("progress", {
                  state: "terminated",
                  reason: WEBSOCKET_REASONS.REJECT
                });

                sinon.assert.calledOnce(notifications.errorL10n);
                sinon.assert.calledWithExactly(notifications.errorL10n,
                  "call_timeout_notification_text");
              });

            it("should not display an error message if the reason is WEBSOCKET_REASONS.CANCEL",
              function() {
                ocView._websocket.trigger("progress", {
                  state: "terminated",
                  reason: WEBSOCKET_REASONS.CANCEL
                });

                sinon.assert.notCalled(notifications.errorL10n);
              });
          });

          describe("state: connecting", function() {
            it("should set display the ConversationView", function() {
              // Prevent the conversation trying to start the session for
              // this test.
              sandbox.stub(conversation, "startSession");

              conversation.set({"loopToken": "fakeToken"});

              ocView._websocket.trigger("progress", {
                state: "connecting"
              });

              TestUtils.findRenderedComponentWithType(ocView,
                sharedViews.ConversationView);
            });
          });
        });
      });
    });

    describe("Events", function() {
      var fakeSessionData, promiseConnectStub;

      beforeEach(function() {
        fakeSessionData = {
          sessionId:      "sessionId",
          sessionToken:   "sessionToken",
          apiKey:         "apiKey",
          websocketToken: 123,
          progressURL:    "fakeUrl",
          callId:         "fakeCallId"
        };
        conversation.set(fakeSessionData);
        conversation.set("loopToken", "fakeToken");
        sandbox.stub(notifications, "errorL10n");
        sandbox.stub(notifications, "warnL10n");
        promiseConnectStub =
          sandbox.stub(loop.CallConnectionWebSocket.prototype, "promiseConnect");
        promiseConnectStub.returns(new Promise(function(resolve, reject) {}));
        sandbox.stub(window, "XMLHttpRequest").returns(fakeAudioXHR);
      });

      describe("call:outgoing", function() {
        it("should display FailedConversationView if session token is missing",
          function() {
            conversation.set("loopToken", "");

            ocView.startCall();

            TestUtils.findRenderedComponentWithType(ocView,
              loop.webapp.FailedConversationView);
          });

        it("should notify the user if session token is missing", function() {
          conversation.set("loopToken", "");

          ocView.startCall();

          sinon.assert.calledOnce(notifications.errorL10n);
          sinon.assert.calledWithExactly(notifications.errorL10n,
                                         "missing_conversation_info");
        });

        it("should setup the websocket if session token is available",
          function() {
            ocView.startCall();

            sinon.assert.calledOnce(promiseConnectStub);
          });

        it("should show the PendingConversationView if session token is available",
          function() {
            ocView.startCall();

            TestUtils.findRenderedComponentWithType(ocView,
              loop.webapp.PendingConversationView);
          });
      });

      describe("session:ended", function() {
        it("should call multiplexGum.reset", function() {
          var multiplexGum = new standaloneMedia._MultiplexGum();
          standaloneMedia.setSingleton(multiplexGum);
          sandbox.stub(standaloneMedia._MultiplexGum.prototype, "reset");

          conversation.trigger("session:ended");

          sinon.assert.calledOnce(multiplexGum.reset);
        });

        it("should display the StartConversationView", function() {
          conversation.trigger("session:ended");

          TestUtils.findRenderedComponentWithType(ocView,
            loop.webapp.EndedConversationView);
        });

        it("should display the FailedConversationView if callStatus is failure",
          function() {
            ocView.setState({
              callStatus: "failure"
            });
            conversation.trigger("session:ended");

            var failedView = TestUtils.findRenderedComponentWithType(ocView,
                loop.webapp.FailedConversationView);
            expect(failedView).to.not.equal(null);
          });
      });

      describe("session:peer-hungup", function() {
        it("should set display the StartConversationView", function() {
          conversation.trigger("session:peer-hungup");

          TestUtils.findRenderedComponentWithType(ocView,
            loop.webapp.EndedConversationView);
        });

        it("should notify the user", function() {
          conversation.trigger("session:peer-hungup");

          sinon.assert.calledOnce(notifications.warnL10n);
          sinon.assert.calledWithExactly(notifications.warnL10n,
                                         "peer_ended_conversation2");
        });

      });

      describe("session:network-disconnected", function() {
        it("should display the StartConversationView",
          function() {
            conversation.trigger("session:network-disconnected");

            TestUtils.findRenderedComponentWithType(ocView,
              loop.webapp.EndedConversationView);
          });

        it("should notify the user", function() {
          conversation.trigger("session:network-disconnected");

          sinon.assert.calledOnce(notifications.warnL10n);
          sinon.assert.calledWithExactly(notifications.warnL10n,
                                         "network_disconnected");
        });
      });

      describe("Published and Subscribed Streams", function() {
        beforeEach(function() {
          ocView._websocket = {
            mediaUp: sinon.spy()
          };
        });

        describe("publishStream", function() {
          it("should not notify the websocket if only one stream is up",
            function() {
              conversation.set("publishedStream", true);

              sinon.assert.notCalled(ocView._websocket.mediaUp);
            });

          it("should notify the websocket that media is up if both streams" +
             "are connected", function() {
              conversation.set("subscribedStream", true);
              conversation.set("publishedStream", true);

              sinon.assert.calledOnce(ocView._websocket.mediaUp);
            });
        });

        describe("subscribedStream", function() {
          it("should not notify the websocket if only one stream is up",
            function() {
              conversation.set("subscribedStream", true);

              sinon.assert.notCalled(ocView._websocket.mediaUp);
            });

          it("should notify tloadhe websocket that media is up if both streams" +
             "are connected", function() {
              conversation.set("publishedStream", true);
              conversation.set("subscribedStream", true);

              sinon.assert.calledOnce(ocView._websocket.mediaUp);
            });
        });
      });

      describe("#setupOutgoingCall", function() {
        describe("No loop token", function() {
          beforeEach(function() {
            conversation.set("loopToken", "");
          });

          it("should display the FailedConversationView", function() {
            ocView.setupOutgoingCall();

            TestUtils.findRenderedComponentWithType(ocView,
              loop.webapp.FailedConversationView);
          });

          it("should display an error", function() {
            ocView.setupOutgoingCall();

            sinon.assert.calledOnce(notifications.errorL10n);
          });
        });

        describe("Has loop token", function() {
          beforeEach(function() {
            sandbox.stub(conversation, "outgoing");
          });

          it("should call requestCallInfo on the client",
            function() {
              conversation.set("selectedCallType", "audio-video");
              ocView.setupOutgoingCall();

              sinon.assert.calledOnce(client.requestCallInfo);
              sinon.assert.calledWith(client.requestCallInfo, "fakeToken",
                                      "audio-video");
            });

          describe("requestCallInfo response handling", function() {
            it("should set display the CallUrlExpiredView if the call has expired",
               function() {
                client.requestCallInfo.callsArgWith(2, {errno: 105});

                ocView.setupOutgoingCall();

                TestUtils.findRenderedComponentWithType(ocView,
                  loop.webapp.CallUrlExpiredView);
              });

            it("should set display the FailedConversationView on any other error",
               function() {
                client.requestCallInfo.callsArgWith(2, {errno: 104});

                ocView.setupOutgoingCall();

                TestUtils.findRenderedComponentWithType(ocView,
                  loop.webapp.FailedConversationView);
              });

            it("should notify the user on any other error", function() {
              client.requestCallInfo.callsArgWith(2, {errno: 104});

              ocView.setupOutgoingCall();

              sinon.assert.calledOnce(notifications.errorL10n);
            });

            it("should call outgoing on the conversation model when details " +
               "are successfully received", function() {
                client.requestCallInfo.callsArgWith(2, null, fakeSessionData);

                ocView.setupOutgoingCall();

                sinon.assert.calledOnce(conversation.outgoing);
                sinon.assert.calledWithExactly(conversation.outgoing, fakeSessionData);
              });
          });
        });
      });

      describe("getMediaPrivs", function() {
        var multiplexGum;

        beforeEach(function() {
          multiplexGum = new standaloneMedia._MultiplexGum();
          standaloneMedia.setSingleton(multiplexGum);
          sandbox.stub(standaloneMedia._MultiplexGum.prototype, "reset");

          sandbox.stub(conversation, "gotMediaPrivs");
        });

        it("should call getPermsAndCacheMedia", function() {
          conversation.trigger("call:outgoing:get-media-privs");

          sinon.assert.calledOnce(stubGetPermsAndCacheMedia);
        });

        it("should call gotMediaPrevs on the model when successful", function() {
          stubGetPermsAndCacheMedia.callsArgWith(1, {});

          conversation.trigger("call:outgoing:get-media-privs");

          sinon.assert.calledOnce(conversation.gotMediaPrivs);
        });

        it("should call multiplexGum.reset when getPermsAndCacheMedia fails",
          function() {
            stubGetPermsAndCacheMedia.callsArgWith(2, "FAKE_ERROR");

            conversation.trigger("call:outgoing:get-media-privs");

            sinon.assert.calledOnce(multiplexGum.reset);
          });

        it("should set state to `failure` when getPermsAndCacheMedia fails",
          function() {
            stubGetPermsAndCacheMedia.callsArgWith(2, "FAKE_ERROR");

            conversation.trigger("call:outgoing:get-media-privs");

            expect(ocView.state.callStatus).eql("failure");
          });
      });


    });

    describe("FailedConversationView", function() {
      var view, conversation, client, fakeAudio;

      beforeEach(function() {
        sandbox.stub(window, "XMLHttpRequest").returns(fakeAudioXHR);

        fakeAudio = {
          play: sinon.spy(),
          pause: sinon.spy(),
          removeAttribute: sinon.spy()
        };
        sandbox.stub(window, "Audio").returns(fakeAudio);

        client = new loop.StandaloneClient({
          baseServerUrl: "http://fake.example.com"
        });
        conversation = new sharedModels.ConversationModel({}, {
          sdk: {}
        });
        conversation.set("loopToken", "fakeToken");

        sandbox.stub(client, "requestCallUrlInfo");
        view = React.addons.TestUtils.renderIntoDocument(
          React.createElement(
            loop.webapp.FailedConversationView, {
              conversation: conversation,
              client: client,
              notifications: notifications
            }));
      });

      it("should play a failure sound, once", function() {
        fakeAudioXHR.onload();

        sinon.assert.called(fakeAudioXHR.open);
        sinon.assert.calledWithExactly(
          fakeAudioXHR.open, "GET", "shared/sounds/failure.ogg", true);
        sinon.assert.calledOnce(fakeAudio.play);
        expect(fakeAudio.loop).to.equal(false);
      });
    });
  });

  describe("WebappRootView", function() {
    var sdk, conversationModel, client, props, standaloneAppStore;
    var activeRoomStore;

    function mountTestComponent() {
      return TestUtils.renderIntoDocument(
        React.createElement(
          loop.webapp.WebappRootView, {
            client: client,
            dispatcher: dispatcher,
            notifications: notifications,
            sdk: sdk,
            conversation: conversationModel,
            standaloneAppStore: standaloneAppStore,
            activeRoomStore: activeRoomStore
          }));
    }

    beforeEach(function() {
      sdk = {
        checkSystemRequirements: function() { return true; }
      };
      conversationModel = new sharedModels.ConversationModel({}, {
        sdk: sdk
      });
      client = new loop.StandaloneClient({
        baseServerUrl: "fakeUrl"
      });
      activeRoomStore = new loop.store.ActiveRoomStore(dispatcher, {
        mozLoop: {},
        sdkDriver: {}
      });
      standaloneAppStore = new loop.store.StandaloneAppStore({
        dispatcher: dispatcher,
        sdk: sdk,
        conversation: conversationModel
      });
      // Stub this to stop the StartConversationView kicking in the request and
      // follow-ups.
      sandbox.stub(client, "requestCallUrlInfo");
    });

    it("should display the UnsupportedDeviceView for `unsupportedDevice` window type",
      function() {
        standaloneAppStore.setStoreState({windowType: "unsupportedDevice"});

        var webappRootView = mountTestComponent();

        TestUtils.findRenderedComponentWithType(webappRootView,
          loop.webapp.UnsupportedDeviceView);
      });

    it("should display the UnsupportedBrowserView for `unsupportedBrowser` window type",
      function() {
        standaloneAppStore.setStoreState({windowType: "unsupportedBrowser"});

        var webappRootView = mountTestComponent();

        TestUtils.findRenderedComponentWithType(webappRootView,
          loop.webapp.UnsupportedBrowserView);
      });

    it("should display the OutgoingConversationView for `outgoing` window type",
      function() {
        standaloneAppStore.setStoreState({windowType: "outgoing"});

        var webappRootView = mountTestComponent();

        TestUtils.findRenderedComponentWithType(webappRootView,
          loop.webapp.OutgoingConversationView);
      });

    it("should display the StandaloneRoomView for `room` window type",
      function() {
        standaloneAppStore.setStoreState({windowType: "room"});

        var webappRootView = mountTestComponent();

        TestUtils.findRenderedComponentWithType(webappRootView,
          loop.standaloneRoomViews.StandaloneRoomView);
      });

    it("should display the HomeView for `home` window type", function() {
        standaloneAppStore.setStoreState({windowType: "home"});

        var webappRootView = mountTestComponent();

        TestUtils.findRenderedComponentWithType(webappRootView,
          loop.webapp.HomeView);
    });
  });

  describe("HomeView", function() {
    it("should call loop.standaloneMedia.reset", function() {
      var multiplexGum = new standaloneMedia._MultiplexGum();
      standaloneMedia.setSingleton(multiplexGum);
      sandbox.stub(standaloneMedia._MultiplexGum.prototype, "reset");

      TestUtils.renderIntoDocument(
        React.createElement(loop.webapp.HomeView));

      sinon.assert.calledOnce(multiplexGum.reset);
      sinon.assert.calledWithExactly(multiplexGum.reset);
    });
  });

  describe("WaitingConversationView", function() {
    var view, websocket, fakeAudio;

    beforeEach(function() {
      websocket = new loop.CallConnectionWebSocket({
        url: "wss://fake/",
        callId: "callId",
        websocketToken: "7b"
      });

      sinon.stub(websocket, "cancel");
      fakeAudio = {
        play: sinon.spy(),
        pause: sinon.spy(),
        removeAttribute: sinon.spy()
      };
      sandbox.stub(window, "Audio").returns(fakeAudio);
      sandbox.stub(window, "XMLHttpRequest").returns(fakeAudioXHR);

      view = React.addons.TestUtils.renderIntoDocument(
        React.createElement(
          loop.webapp.WaitingConversationView, {
            websocket: websocket
          })
      );
    });

    describe("#componentDidMount", function() {

      it("should play a looped connecting sound", function() {
        fakeAudioXHR.onload();

        sinon.assert.called(fakeAudioXHR.open);
        sinon.assert.calledWithExactly(
          fakeAudioXHR.open, "GET", "shared/sounds/connecting.ogg", true);
        sinon.assert.calledOnce(fakeAudio.play);
        expect(fakeAudio.loop).to.equal(true);
      });

    });

    describe("#_cancelOutgoingCall", function() {
      it("should inform the websocket to cancel the setup", function() {
        var button = view.getDOMNode().querySelector(".btn-cancel");
        React.addons.TestUtils.Simulate.click(button);

        sinon.assert.calledOnce(websocket.cancel);
      });

      it("should call multiplexGum.reset to release the camera", function() {
        var multiplexGum = new standaloneMedia._MultiplexGum();
        standaloneMedia.setSingleton(multiplexGum);
        sandbox.stub(standaloneMedia._MultiplexGum.prototype, "reset");

        var button = view.getDOMNode().querySelector(".btn-cancel");
        React.addons.TestUtils.Simulate.click(button);

        sinon.assert.calledOnce(multiplexGum.reset);
        sinon.assert.calledWithExactly(multiplexGum.reset);
      });
    });

    describe("Events", function() {
      describe("progress:alerting", function() {
        it("should update the callstate to ringing", function () {
          websocket.trigger("progress:alerting");

          expect(view.state.callState).to.be.equal("ringing");
        });

        it("should play a looped ringing sound", function() {
          websocket.trigger("progress:alerting");
          fakeAudioXHR.onload();

          sinon.assert.called(fakeAudioXHR.open);
          sinon.assert.calledWithExactly(
            fakeAudioXHR.open, "GET", "shared/sounds/ringtone.ogg", true);

          sinon.assert.called(fakeAudio.play);
          expect(fakeAudio.loop).to.equal(true);
        });
      });
    });
  });

  describe("StartConversationView", function() {
    describe("#initiate", function() {
      var conversation, view, fakeSubmitEvent, requestCallUrlInfo;

      beforeEach(function() {
        conversation = new sharedModels.ConversationModel({}, {
          sdk: {}
        });

        fakeSubmitEvent = {preventDefault: sinon.spy()};

        var standaloneClientStub = {
          requestCallUrlInfo: function(token, cb) {
            cb(null, {urlCreationDate: 0});
          },
          settings: {baseServerUrl: loop.webapp.baseServerUrl}
        };

        view = React.addons.TestUtils.renderIntoDocument(
          React.createElement(
            loop.webapp.StartConversationView, {
              conversation: conversation,
              notifications: notifications,
              client: standaloneClientStub
            }));
      });

      it("should start the audio-video conversation establishment process",
        function() {
          var setupOutgoingCall = sinon.stub(conversation, "setupOutgoingCall");

          var button = view.getDOMNode().querySelector(".btn-accept");
          React.addons.TestUtils.Simulate.click(button);

          sinon.assert.calledOnce(setupOutgoingCall);
          sinon.assert.calledWithExactly(setupOutgoingCall, "audio-video");
      });

      it("should start the audio-only conversation establishment process",
        function() {
          var setupOutgoingCall = sinon.stub(conversation, "setupOutgoingCall");

          var button = view.getDOMNode().querySelector(".start-audio-only-call");
          React.addons.TestUtils.Simulate.click(button);

          sinon.assert.calledOnce(setupOutgoingCall);
          sinon.assert.calledWithExactly(setupOutgoingCall, "audio");
        });

      it("should disable audio-video button once session is initiated",
         function() {
           conversation.set("loopToken", "fake");

           var button = view.getDOMNode().querySelector(".btn-accept");
           React.addons.TestUtils.Simulate.click(button);

           expect(button.disabled).to.eql(true);
         });

      it("should disable audio-only button once session is initiated",
         function() {
           conversation.set("loopToken", "fake");

           var button = view.getDOMNode().querySelector(".start-audio-only-call");
           React.addons.TestUtils.Simulate.click(button);

           expect(button.disabled).to.eql(true);
         });

      it("should set selectedCallType to audio", function() {
        conversation.set("loopToken", "fake");

         var button = view.getDOMNode().querySelector(".start-audio-only-call");
         React.addons.TestUtils.Simulate.click(button);

         expect(conversation.get("selectedCallType")).to.eql("audio");
       });

       it("should set selectedCallType to audio-video", function() {
         conversation.set("loopToken", "fake");

         var button = view.getDOMNode().querySelector(".standalone-call-btn-video-icon");
         React.addons.TestUtils.Simulate.click(button);

         expect(conversation.get("selectedCallType")).to.eql("audio-video");
      });

      // XXX this test breaks while the feature actually works; find a way to
      // test this properly.
      it.skip("should set state.urlCreationDateString to a locale date string",
        function() {
          var date = new Date();
          var options = {year: "numeric", month: "long", day: "numeric"};
          var timestamp = date.toLocaleDateString(navigator.language, options);
          var dateElem = view.getDOMNode().querySelector(".call-url-date");

          expect(dateElem.textContent).to.eql(timestamp);
        });
    });

    describe("Events", function() {
      var conversation, view, StandaloneClient, requestCallUrlInfo;

      beforeEach(function() {
        conversation = new sharedModels.ConversationModel({
          loopToken: "fake"
        }, {
          sdk: {}
        });

        conversation.onMarketplaceMessage = function() {};
        sandbox.stub(notifications, "errorL10n");
        requestCallUrlInfo = sandbox.stub();

        view = React.addons.TestUtils.renderIntoDocument(
            React.createElement(
              loop.webapp.StartConversationView, {
                conversation: conversation,
                notifications: notifications,
                client: {requestCallUrlInfo: requestCallUrlInfo}
              }));

        loop.config.marketplaceUrl = "http://market/";
      });

      it("should call requestCallUrlInfo", function() {
        sinon.assert.calledOnce(requestCallUrlInfo);
        sinon.assert.calledWithExactly(requestCallUrlInfo,
                                       sinon.match.string,
                                       sinon.match.func);
      });

      it("should add a notification when a session:error model event is " +
         " received without an argument", function() {
        conversation.trigger("session:error");

        sinon.assert.calledOnce(notifications.errorL10n);
        sinon.assert.calledWithExactly(notifications.errorL10n,
          sinon.match.string, undefined);
      });

      it("should add a notification with the custom message id when a " +
         "session:error event is fired with an argument", function() {
        conversation.trigger("session:error", "tech_error");

        sinon.assert.calledOnce(notifications.errorL10n);
        sinon.assert.calledWithExactly(notifications.errorL10n,
                                       "tech_error", undefined);
      });

      it("should add a notification with the custom message id when a " +
         "session:error event is fired with an argument and parameters",
         function() {
          conversation.trigger("session:error", "tech_error", {param: "value"});

          sinon.assert.calledOnce(notifications.errorL10n);
          sinon.assert.calledWithExactly(notifications.errorL10n,
                                         "tech_error", { param: "value" });
      });

      it("should set marketplace hidden iframe src when fxos:app-needed is " +
         "triggered", function(done) {
        var marketplace = view.getDOMNode().querySelector("#marketplace");
        expect(marketplace.src).to.be.equal("");

        conversation.trigger("fxos:app-needed");

        view.forceUpdate(function() {
          expect(marketplace.src).to.be.equal(loop.config.marketplaceUrl);
          done();
        });
      });

    });

    describe("#render", function() {
      var conversation, view, requestCallUrlInfo, oldLocalStorageValue;

      beforeEach(function() {
        oldLocalStorageValue = localStorage.getItem("has-seen-tos");
        localStorage.removeItem("has-seen-tos");

        conversation = new sharedModels.ConversationModel({
          loopToken: "fake"
        }, {
          sdk: {}
        });

        requestCallUrlInfo = sandbox.stub();
      });

      afterEach(function() {
        if (oldLocalStorageValue !== null) {
          localStorage.setItem("has-seen-tos", oldLocalStorageValue);
        }
      });

      it("should show the TOS", function() {
        var tos;

        view = React.addons.TestUtils.renderIntoDocument(
          React.createElement(
            loop.webapp.StartConversationView, {
              conversation: conversation,
              notifications: notifications,
              client: {requestCallUrlInfo: requestCallUrlInfo}
            }));
        tos = view.getDOMNode().querySelector(".terms-service");

        expect(tos.classList.contains("hide")).to.equal(false);
      });

      it("should not show the TOS if it has already been seen", function() {
        var tos;

        localStorage.setItem("has-seen-tos", "true");
        view = React.addons.TestUtils.renderIntoDocument(
          React.createElement(
            loop.webapp.StartConversationView, {
              conversation: conversation,
              notifications: notifications,
              client: {requestCallUrlInfo: requestCallUrlInfo}
            }));
        tos = view.getDOMNode().querySelector(".terms-service");

        expect(tos.classList.contains("hide")).to.equal(true);
      });
    });
  });

  describe("EndedConversationView", function() {
    var view, conversation, fakeAudio;

    beforeEach(function() {
      fakeAudio = {
        play: sinon.spy(),
        pause: sinon.spy(),
        removeAttribute: sinon.spy()
      };
      sandbox.stub(window, "Audio").returns(fakeAudio);

      conversation = new sharedModels.ConversationModel({}, {
        sdk: {}
      });
      sandbox.stub(window, "XMLHttpRequest").returns(fakeAudioXHR);
      view = React.addons.TestUtils.renderIntoDocument(
        React.createElement(
          loop.webapp.EndedConversationView, {
            conversation: conversation,
            sdk: {},
            onAfterFeedbackReceived: function(){}
          }));
    });

    it("should render a ConversationView", function() {
      TestUtils.findRenderedComponentWithType(view, sharedViews.ConversationView);
    });

    it("should render a FeedbackView", function() {
      TestUtils.findRenderedComponentWithType(view, sharedViews.FeedbackView);
    });
  });

  describe("PromoteFirefoxView", function() {
    describe("#render", function() {
      it("should not render when using Firefox", function() {
        var comp = TestUtils.renderIntoDocument(
          React.createElement(loop.webapp.PromoteFirefoxView, {
            isFirefox: true
          }));

        expect(comp.getDOMNode().querySelectorAll("h3").length).eql(0);
      });

      it("should render when not using Firefox", function() {
        var comp = TestUtils.renderIntoDocument(
          React.createElement(
            loop.webapp.PromoteFirefoxView, {
              isFirefox: false
            }));

        expect(comp.getDOMNode().querySelectorAll("h3").length).eql(1);
      });
    });
  });

  describe("Firefox OS", function() {
    var conversation, client;

    before(function() {
      client = new loop.StandaloneClient({
        baseServerUrl: "http://fake.example.com"
      });
      sandbox.stub(client, "requestCallInfo");
      conversation = new sharedModels.ConversationModel({}, {
        sdk: {},
        pendingCallTimeout: 1000
      });
    });

    describe("Setup call", function() {
      var conversation, setupOutgoingCall, view, requestCallUrlInfo;

      beforeEach(function() {
        conversation = new loop.webapp.FxOSConversationModel({
          loopToken: "fakeToken"
        });
        setupOutgoingCall = sandbox.stub(conversation, "setupOutgoingCall");

        var standaloneClientStub = {
          requestCallUrlInfo: function(token, cb) {
            cb(null, {urlCreationDate: 0});
          },
          settings: {baseServerUrl: loop.webapp.baseServerUrl}
        };

        view = React.addons.TestUtils.renderIntoDocument(
          React.createElement(
            loop.webapp.StartConversationView, {
              conversation: conversation,
              notifications: notifications,
              client: standaloneClientStub
            }));

        // default to succeeding with a null local media object
        stubGetPermsAndCacheMedia.callsArgWith(1, {});
      });

      it("should start the conversation establishment process", function() {
        var button = view.getDOMNode().querySelector("button");
        React.addons.TestUtils.Simulate.click(button);

        sinon.assert.calledOnce(setupOutgoingCall);
      });
    });

    describe("FxOSConversationModel", function() {
      var model, realMozActivity;

      before(function() {
        model = new loop.webapp.FxOSConversationModel({
          loopToken: "fakeToken",
          callerId: "callerId"
        });

        realMozActivity = window.MozActivity;

        loop.config.fxosApp = {
          name: "Firefox Hello"
        };
      });

      after(function() {
        window.MozActivity = realMozActivity;
      });

      describe("setupOutgoingCall", function() {
        var _activityProps, _onerror, trigger;

        function fireError(errorName) {
          _onerror({
            target: {
              error: {
                name: errorName
              }
            }
          });
        }

        before(function() {
          window.MozActivity = function(activityProps) {
            _activityProps = activityProps;
            return {
              set onerror(callback) {
                _onerror = callback;
              }
            };
          };
        });

        after(function() {
          window.MozActivity = realMozActivity;
        });

        beforeEach(function() {
          trigger = sandbox.stub(model, "trigger");
          _activityProps = undefined;
        });

        afterEach(function() {
          trigger.restore();
        });

        it("Activity properties with video call", function() {
          expect(_activityProps).to.not.exist;
          model.setupOutgoingCall("audio-video");
          expect(_activityProps).to.exist;
          expect(_activityProps).eql({
            name: "loop-call",
            data: {
              type: "loop/token",
              token: "fakeToken",
              callerId: "callerId",
              video: true
            }
          });
        });

        it("Activity properties with audio call", function() {
          expect(_activityProps).to.not.exist;
          model.setupOutgoingCall("audio");
          expect(_activityProps).to.exist;
          expect(_activityProps).eql({
            name: "loop-call",
            data: {
              type: "loop/token",
              token: "fakeToken",
              callerId: "callerId",
              video: false
            }
          });
        });

        it("Activity properties by default", function() {
          expect(_activityProps).to.not.exist;
          model.setupOutgoingCall();
          expect(_activityProps).to.exist;
          expect(_activityProps).eql({
            name: "loop-call",
            data: {
              type: "loop/token",
              token: "fakeToken",
              callerId: "callerId",
              video: false
            }
          });
        });

        it("NO_PROVIDER activity error should trigger fxos:app-needed",
          function() {
            sinon.assert.notCalled(trigger);
            model.setupOutgoingCall();
            fireError("NO_PROVIDER");
            sinon.assert.calledOnce(trigger);
            sinon.assert.calledWithExactly(trigger, "fxos:app-needed");
          }
        );

        it("Other activity error should trigger session:error",
          function() {
            sinon.assert.notCalled(trigger);
            model.setupOutgoingCall();
            fireError("whatever");
            sinon.assert.calledOnce(trigger);
            sinon.assert.calledWithExactly(trigger, "session:error",
              "fxos_app_needed", { fxosAppName: loop.config.fxosApp.name });
          }
        );
      });

      describe("onMarketplaceMessage", function() {
        var view, setupOutgoingCall, trigger;

        before(function() {
          view = React.addons.TestUtils.renderIntoDocument(
            React.createElement(
              loop.webapp.StartConversationView, {
                conversation: model,
                notifications: notifications,
                client: {requestCallUrlInfo: sandbox.stub()}
              }));
        });

        beforeEach(function() {
          setupOutgoingCall = sandbox.stub(model, "setupOutgoingCall");
          trigger = sandbox.stub(model, "trigger");
        });

        afterEach(function() {
          setupOutgoingCall.restore();
          trigger.restore();
        });

        it("We should call trigger a FxOS outgoing call if we get " +
           "install-package message without error", function() {
          sinon.assert.notCalled(setupOutgoingCall);
          model.onMarketplaceMessage({
            data: {
              name: "install-package"
            }
          });
          sinon.assert.calledOnce(setupOutgoingCall);
        });

        it("We should trigger a session:error event if we get " +
           "install-package message with an error", function() {
          sinon.assert.notCalled(trigger);
          sinon.assert.notCalled(setupOutgoingCall);
          model.onMarketplaceMessage({
            data: {
              name: "install-package",
              error: "error"
            }
          });
          sinon.assert.notCalled(setupOutgoingCall);
          sinon.assert.calledOnce(trigger);
          sinon.assert.calledWithExactly(trigger, "session:error",
            "fxos_app_needed", { fxosAppName: loop.config.fxosApp.name });
        });
      });
    });
  });
});
