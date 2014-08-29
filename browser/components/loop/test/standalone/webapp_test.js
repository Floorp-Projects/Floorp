/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop, sinon */

var expect = chai.expect;
var TestUtils = React.addons.TestUtils;

describe("loop.webapp", function() {
  "use strict";

  var sharedModels = loop.shared.models,
      sharedViews = loop.shared.views,
      sandbox,
      notifier;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    // conversation#outgoing sets timers, so we need to use fake ones
    // to prevent random failures.
    sandbox.useFakeTimers();
    notifier = {
      notify: sandbox.spy(),
      warn: sandbox.spy(),
      warnL10n: sandbox.spy(),
      error: sandbox.spy(),
      errorL10n: sandbox.spy(),
    };
    loop.config.pendingCallTimeout = 1000;
  });

  afterEach(function() {
    sandbox.restore();
    delete loop.config.pendingCallTimeout;
  });

  describe("#init", function() {
    var WebappRouter;

    beforeEach(function() {
      WebappRouter = loop.webapp.WebappRouter;
      sandbox.stub(WebappRouter.prototype, "navigate");
    });

    afterEach(function() {
      Backbone.history.stop();
    });

    it("should navigate to the unsupportedDevice route if the sdk detects " +
       "the device is running iOS", function() {
      sandbox.stub(loop.webapp.WebappHelper.prototype, "isIOS").returns(true);

      loop.webapp.init();

      sinon.assert.calledOnce(WebappRouter.prototype.navigate);
      sinon.assert.calledWithExactly(WebappRouter.prototype.navigate,
                                     "unsupportedDevice", {trigger: true});
    });

    it("should navigate to the unsupportedBrowser route if the sdk detects " +
       "the browser is unsupported", function() {
      sandbox.stub(loop.webapp.WebappHelper.prototype, "isIOS").returns(false);
      sandbox.stub(window.OT, "checkSystemRequirements").returns(false);

      loop.webapp.init();

      sinon.assert.calledOnce(WebappRouter.prototype.navigate);
      sinon.assert.calledWithExactly(WebappRouter.prototype.navigate,
                                     "unsupportedBrowser", {trigger: true});
    });
  });

  describe("WebappRouter", function() {
    var router, conversation, client;

    beforeEach(function() {
      client = new loop.StandaloneClient({
        baseServerUrl: "http://fake.example.com"
      });
      sandbox.stub(client, "requestCallInfo");
      conversation = new sharedModels.ConversationModel({}, {
        sdk: {},
        pendingCallTimeout: 1000
      });
      sandbox.stub(loop.webapp.WebappRouter.prototype, "loadReactComponent");
      router = new loop.webapp.WebappRouter({
        helper: {},
        client: client,
        conversation: conversation,
        notifier: notifier
      });
      sandbox.stub(router, "loadView");
      sandbox.stub(router, "navigate");
    });

    describe("#startCall", function() {
      beforeEach(function() {
        sandbox.stub(router, "_setupWebSocketAndCallView");
      });

      it("should navigate back home if session token is missing", function() {
        router.startCall();

        sinon.assert.calledOnce(router.navigate);
        sinon.assert.calledWithMatch(router.navigate, "home");
      });

      it("should notify the user if session token is missing", function() {
        router.startCall();

        sinon.assert.calledOnce(notifier.errorL10n);
        sinon.assert.calledWithExactly(notifier.errorL10n,
                                       "missing_conversation_info");
      });

      it("should setup the websocket if session token is available", function() {
        conversation.set("loopToken", "fake");

        router.startCall();

        sinon.assert.calledOnce(router._setupWebSocketAndCallView);
        sinon.assert.calledWithExactly(router._setupWebSocketAndCallView, "fake");
      });
    });

    describe("#_setupWebSocketAndCallView", function() {
      beforeEach(function() {
        conversation.setOutgoingSessionData({
          sessionId:      "sessionId",
          sessionToken:   "sessionToken",
          apiKey:         "apiKey",
          callId:         "Hello",
          progressURL:    "http://progress.example.com",
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
          router._setupWebSocketAndCallView("fake");

          promise.then(function () {
            sinon.assert.calledOnce(loop.CallConnectionWebSocket);
            sinon.assert.calledWithExactly(loop.CallConnectionWebSocket, {
              callId: "Hello",
              url: "http://progress.example.com",
              // The websocket token is converted to a hex string.
              websocketToken: "7b"
            });
            done();
          });
        });

        it("should navigate to call/ongoing/:token", function(done) {
          router._setupWebSocketAndCallView("fake");

          promise.then(function () {
            sinon.assert.calledOnce(router.navigate);
            sinon.assert.calledWithMatch(router.navigate, "call/ongoing/fake");
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

        it("should display an error", function() {
          router._setupWebSocketAndCallView();

          promise.then(function() {
          }, function () {
            sinon.assert.calledOnce(router._notifier.errorL10n);
            sinon.assert.calledWithExactly(router._notifier.errorL10n,
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

          router._setupWebSocketAndCallView();
        });

        describe("Progress", function() {
          describe("state: terminate, reason: reject", function() {
            beforeEach(function() {
              sandbox.stub(router, "endCall");
            });

            it("should end the call", function() {
              router._websocket.trigger("progress", {
                state: "terminated",
                reason: "reject"
              });

              sinon.assert.calledOnce(router.endCall);
            });

            it("should display an error message", function() {
              router._websocket.trigger("progress", {
                state: "terminated",
                reason: "reject"
              });

              sinon.assert.calledOnce(router._notifier.errorL10n);
              sinon.assert.calledWithExactly(router._notifier.errorL10n,
                "call_timeout_notification_text");
            });
          });
        });
      });
    });

    describe("#endCall", function() {
      it("should navigate to home if session token is unset", function() {
        router.endCall();

        sinon.assert.calledOnce(router.navigate);
        sinon.assert.calledWithMatch(router.navigate, "home");
      });

      it("should navigate to call/:token if session token is set", function() {
        conversation.set("loopToken", "fake");

        router.endCall();

        sinon.assert.calledOnce(router.navigate);
        sinon.assert.calledWithMatch(router.navigate, "call/fake");
      });
    });

    describe("Routes", function() {
      describe("#home", function() {
        it("should load the HomeView", function() {
          router.home();

          sinon.assert.calledOnce(router.loadView);
          sinon.assert.calledWith(router.loadView,
            sinon.match.instanceOf(loop.webapp.HomeView));
        });
      });

      describe("#expired", function() {
        it("should load the CallUrlExpiredView view", function() {
          router.expired();

          sinon.assert.calledOnce(router.loadReactComponent);
          sinon.assert.calledWith(router.loadReactComponent,
            sinon.match(function(value) {
              return React.addons.TestUtils.isDescriptorOfType(
                value, loop.webapp.CallUrlExpiredView);
            }));
        });
      });

      describe("#initiate", function() {
        it("should set the token on the conversation model", function() {
          router.initiate("fakeToken");

          expect(conversation.get("loopToken")).eql("fakeToken");
        });

        it("should load the StartConversationView", function() {
          router.initiate("fakeToken");

          sinon.assert.calledOnce(router.loadReactComponent);
          sinon.assert.calledWithExactly(router.loadReactComponent,
            sinon.match(function(value) {
              return React.addons.TestUtils.isDescriptorOfType(
                value, loop.webapp.StartConversationView);
            }));
        });

        // https://bugzilla.mozilla.org/show_bug.cgi?id=991118
        it("should terminate any ongoing call session", function() {
          sinon.stub(conversation, "endSession");
          conversation.set("ongoing", true);

          router.initiate("fakeToken");

          sinon.assert.calledOnce(conversation.endSession);
        });
      });

      describe("#loadConversation", function() {
        it("should load the ConversationView if session is set", function() {
          conversation.set("sessionId", "fakeSessionId");

          router.loadConversation();

          sinon.assert.calledOnce(router.loadReactComponent);
          sinon.assert.calledWith(router.loadReactComponent,
            sinon.match(function(value) {
              return React.addons.TestUtils.isDescriptorOfType(
                value, loop.shared.views.ConversationView);
            }));
        });

        it("should navigate to #call/{token} if session isn't ready",
          function() {
            router.loadConversation("fakeToken");

            sinon.assert.calledOnce(router.navigate);
            sinon.assert.calledWithMatch(router.navigate, "call/fakeToken");
          });
      });

      describe("#unsupportedDevice", function() {
        it("should load the UnsupportedDeviceView", function() {
          router.unsupportedDevice();

          sinon.assert.calledOnce(router.loadView);
          sinon.assert.calledWith(router.loadView,
            sinon.match.instanceOf(sharedViews.UnsupportedDeviceView));
        });
      });

      describe("#unsupportedBrowser", function() {
        it("should load the UnsupportedBrowserView", function() {
          router.unsupportedBrowser();

          sinon.assert.calledOnce(router.loadView);
          sinon.assert.calledWith(router.loadView,
            sinon.match.instanceOf(sharedViews.UnsupportedBrowserView));
        });
      });
    });

    describe("Events", function() {
      var fakeSessionData;

      beforeEach(function() {
        fakeSessionData = {
          sessionId:      "sessionId",
          sessionToken:   "sessionToken",
          apiKey:         "apiKey",
          websocketToken: 123
        };
        conversation.set("loopToken", "fakeToken");
        sandbox.stub(router, "startCall");
      });

      it("should attempt to start the call once call session is ready",
        function() {
          router.setupOutgoingCall();
          conversation.outgoing(fakeSessionData);

          sinon.assert.calledOnce(router.startCall);
        });

      it("should navigate to call/{token} when conversation ended", function() {
        conversation.trigger("session:ended");

        sinon.assert.calledOnce(router.navigate);
        sinon.assert.calledWithMatch(router.navigate, "call/fakeToken");
      });

      it("should navigate to call/{token} when peer hangs up", function() {
        conversation.trigger("session:peer-hungup");

        sinon.assert.calledOnce(router.navigate);
        sinon.assert.calledWithMatch(router.navigate, "call/fakeToken");
      });

      it("should navigate to call/{token} when network disconnects",
        function() {
          conversation.trigger("session:network-disconnected");

          sinon.assert.calledOnce(router.navigate);
          sinon.assert.calledWithMatch(router.navigate, "call/fakeToken");
        });

      describe("Published and Subscribed Streams", function() {
        beforeEach(function() {
          router._websocket = {
            mediaUp: sinon.spy()
          };
          router.initiate();
        });

        describe("publishStream", function() {
          it("should not notify the websocket if only one stream is up",
            function() {
              conversation.set("publishedStream", true);

              sinon.assert.notCalled(router._websocket.mediaUp);
            });

          it("should notify the websocket that media is up if both streams" +
             "are connected", function() {
              conversation.set("subscribedStream", true);
              conversation.set("publishedStream", true);

              sinon.assert.calledOnce(router._websocket.mediaUp);
            });
        });

        describe("subscribedStream", function() {
          it("should not notify the websocket if only one stream is up",
            function() {
              conversation.set("subscribedStream", true);

              sinon.assert.notCalled(router._websocket.mediaUp);
            });

          it("should notify the websocket that media is up if both streams" +
             "are connected", function() {
              conversation.set("publishedStream", true);
              conversation.set("subscribedStream", true);

              sinon.assert.calledOnce(router._websocket.mediaUp);
            });
        });
      });

      describe("#setupOutgoingCall", function() {
        beforeEach(function() {
          router.initiate();
        });

        describe("No loop token", function() {
          it("should navigate to home", function() {
            conversation.setupOutgoingCall();

            sinon.assert.calledOnce(router.navigate);
            sinon.assert.calledWithMatch(router.navigate, "home");
          });

          it("should display an error", function() {
            conversation.setupOutgoingCall();

            sinon.assert.calledOnce(notifier.errorL10n);
          });
        });

        describe("Has loop token", function() {
          beforeEach(function() {
            conversation.set("loopToken", "fakeToken");
            conversation.set("selectedCallType", "audio-video");
            sandbox.stub(conversation, "outgoing");
          });

          it("should call requestCallInfo on the client",
            function() {
              conversation.setupOutgoingCall();

              sinon.assert.calledOnce(client.requestCallInfo);
              sinon.assert.calledWith(client.requestCallInfo, "fakeToken",
                                      "audio-video");
            });

          describe("requestCallInfo response handling", function() {
            it("should navigate to call/expired when a session has expired",
               function() {
                client.requestCallInfo.callsArgWith(2, {errno: 105});
                conversation.setupOutgoingCall();

                sinon.assert.calledOnce(router.navigate);
                sinon.assert.calledWith(router.navigate, "/call/expired");
              });

            it("should navigate to home on any other error", function() {
              client.requestCallInfo.callsArgWith(2, {errno: 104});
              conversation.setupOutgoingCall();

              sinon.assert.calledOnce(router.navigate);
              sinon.assert.calledWith(router.navigate, "home");
              });

            it("should notify the user on any other error", function() {
              client.requestCallInfo.callsArgWith(2, {errno: 104});
              conversation.setupOutgoingCall();

              sinon.assert.calledOnce(notifier.errorL10n);
            });

            it("should call outgoing on the conversation model when details " +
               "are successfully received", function() {
                client.requestCallInfo.callsArgWith(2, null, fakeSessionData);
                conversation.setupOutgoingCall();

                sinon.assert.calledOnce(conversation.outgoing);
                sinon.assert.calledWithExactly(conversation.outgoing, fakeSessionData);
              });
          });
        });
      });
    });
  });

  describe("StartConversationView", function() {
    describe("#initialize", function() {
      it("should require a conversation option", function() {
        expect(function() {
          new loop.webapp.WebappRouter();
        }).to.Throw(Error, /missing required conversation/);
      });
    });

    describe("#initiate", function() {
      var conversation, setupOutgoingCall, view, fakeSubmitEvent,
          requestCallUrlInfo;

      beforeEach(function() {
        conversation = new sharedModels.ConversationModel({}, {
          sdk: {},
          pendingCallTimeout: 1000
        });

        fakeSubmitEvent = {preventDefault: sinon.spy()};
        setupOutgoingCall = sinon.stub(conversation, "setupOutgoingCall");

        var standaloneClientStub = {
          requestCallUrlInfo: function(token, cb) {
            cb(null, {urlCreationDate: 0});
          },
          settings: {baseServerUrl: loop.webapp.baseServerUrl}
        };

        view = React.addons.TestUtils.renderIntoDocument(
            loop.webapp.StartConversationView({
              model: conversation,
              notifier: notifier,
              client: standaloneClientStub
            })
        );
      });

      it("should start the audio-video conversation establishment process",
        function() {
          var button = view.getDOMNode().querySelector(".btn-accept");
          React.addons.TestUtils.Simulate.click(button);

          sinon.assert.calledOnce(setupOutgoingCall);
          sinon.assert.calledWithExactly(setupOutgoingCall);
      });

      it("should start the audio-only conversation establishment process",
        function() {
          var button = view.getDOMNode().querySelector(".start-audio-only-call");
          React.addons.TestUtils.Simulate.click(button);

          sinon.assert.calledOnce(setupOutgoingCall);
          sinon.assert.calledWithExactly(setupOutgoingCall);
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

      it("should set state.urlCreationDateString to a locale date string",
         function() {
        // wrap in a jquery object because text is broken up
        // into several span elements
        var date = new Date(0);
        var options = {year: "numeric", month: "long", day: "numeric"};
        var timestamp = date.toLocaleDateString(navigator.language, options);

        expect(view.state.urlCreationDateString).to.eql(timestamp);
      });

    });

    describe("Events", function() {
      var conversation, view, StandaloneClient, requestCallUrlInfo;

      beforeEach(function() {
        conversation = new sharedModels.ConversationModel({
          loopToken: "fake"
        }, {
          sdk: {},
          pendingCallTimeout: 1000
        });

        sandbox.spy(conversation, "listenTo");
        requestCallUrlInfo = sandbox.stub();

        view = React.addons.TestUtils.renderIntoDocument(
            loop.webapp.StartConversationView({
              model: conversation,
              notifier: notifier,
              client: {requestCallUrlInfo: requestCallUrlInfo}
            })
          );
      });

      it("should call requestCallUrlInfo", function() {
        sinon.assert.calledOnce(requestCallUrlInfo);
        sinon.assert.calledWithExactly(requestCallUrlInfo,
                                       sinon.match.string,
                                       sinon.match.func);
      });

      it("should listen for session:error events", function() {
        sinon.assert.calledOnce(conversation.listenTo);
        sinon.assert.calledWithExactly(conversation.listenTo, conversation,
                                       "session:error", sinon.match.func);
      });

      it("should trigger a notication when a session:error model event is " +
         " received", function() {
        conversation.trigger("session:error", "tech error");

        sinon.assert.calledOnce(notifier.errorL10n);
        sinon.assert.calledWithExactly(notifier.errorL10n,
                                       "unable_retrieve_call_info");
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
          sdk: {},
          pendingCallTimeout: 1000
        });

        requestCallUrlInfo = sandbox.stub();
      });

      afterEach(function() {
        if (oldLocalStorageValue !== null)
          localStorage.setItem("has-seen-tos", oldLocalStorageValue);
      });

      it("should show the TOS", function() {
        var tos;

        view = React.addons.TestUtils.renderIntoDocument(
          loop.webapp.StartConversationView({
            model: conversation,
            notifier: notifier,
            client: {requestCallUrlInfo: requestCallUrlInfo}
          })
        );
        tos = view.getDOMNode().querySelector(".terms-service");

        expect(tos.classList.contains("hide")).to.equal(false);
      });

      it("should not show the TOS if it has already been seen", function() {
        var tos;

        localStorage.setItem("has-seen-tos", "true");
        view = React.addons.TestUtils.renderIntoDocument(
          loop.webapp.StartConversationView({
            model: conversation,
            notifier: notifier,
            client: {requestCallUrlInfo: requestCallUrlInfo}
          })
        );
        tos = view.getDOMNode().querySelector(".terms-service");

        expect(tos.classList.contains("hide")).to.equal(true);
      });
    });
  });

  describe("PromoteFirefoxView", function() {
    describe("#render", function() {
      it("should not render when using Firefox", function() {
        var comp = TestUtils.renderIntoDocument(loop.webapp.PromoteFirefoxView({
          helper: {isFirefox: function() { return true; }}
        }));

        expect(comp.getDOMNode().querySelectorAll("h3").length).eql(0);
      });

      it("should render when not using Firefox", function() {
        var comp = TestUtils.renderIntoDocument(loop.webapp.PromoteFirefoxView({
          helper: {isFirefox: function() { return false; }}
        }));

        expect(comp.getDOMNode().querySelectorAll("h3").length).eql(1);
      });
    });
  });

  describe("WebappHelper", function() {
    var helper;

    beforeEach(function() {
      helper = new loop.webapp.WebappHelper();
    });

    describe("#isIOS", function() {
      it("should detect iOS", function() {
        expect(helper.isIOS("iPad")).eql(true);
        expect(helper.isIOS("iPod")).eql(true);
        expect(helper.isIOS("iPhone")).eql(true);
        expect(helper.isIOS("iPhone Simulator")).eql(true);
      });

      it("shouldn't detect iOS with other platforms", function() {
        expect(helper.isIOS("MacIntel")).eql(false);
      });
    });

    describe("#isFirefox", function() {
      it("should detect Firefox", function() {
        expect(helper.isFirefox("Firefox")).eql(true);
        expect(helper.isFirefox("Gecko/Firefox")).eql(true);
        expect(helper.isFirefox("Firefox/Gecko")).eql(true);
        expect(helper.isFirefox("Gecko/Firefox/Chuck Norris")).eql(true);
      });

      it("shouldn't detect Firefox with other platforms", function() {
        expect(helper.isFirefox("Opera")).eql(false);
      });
    });
  });

});
