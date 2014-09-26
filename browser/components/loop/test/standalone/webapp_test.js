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
      sharedUtils = loop.shared.utils,
      sandbox,
      notifications,
      feedbackApiClient;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    notifications = new sharedModels.NotificationCollection();
    feedbackApiClient = new loop.FeedbackAPIClient("http://invalid", {
      product: "Loop"
    });
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("#init", function() {
    var conversationSetStub;

    beforeEach(function() {
      sandbox.stub(React, "renderComponent");
      sandbox.stub(sharedUtils.Helper.prototype,
                   "locationHash").returns("#call/fake-Token");
      loop.config.feedbackApiUrl = "http://fake.invalid";
      conversationSetStub =
        sandbox.stub(sharedModels.ConversationModel.prototype, "set");
    });

    it("should create the WebappRootView", function() {
      loop.webapp.init();

      sinon.assert.calledOnce(React.renderComponent);
      sinon.assert.calledWith(React.renderComponent,
        sinon.match(function(value) {
          return TestUtils.isDescriptorOfType(value,
            loop.webapp.WebappRootView);
      }));
    });

    it("should set the loopToken on the conversation", function() {
      loop.webapp.init();

       sinon.assert.called(conversationSetStub);
       sinon.assert.calledWithExactly(conversationSetStub, "loopToken", "fake-Token");
    });
  });

  describe("OutgoingConversationView", function() {
    var ocView, conversation, client;

    function mountTestComponent(props) {
      return TestUtils.renderIntoDocument(
        loop.webapp.OutgoingConversationView(props));
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
        helper: new sharedUtils.Helper(),
        client: client,
        conversation: conversation,
        notifications: notifications,
        sdk: {},
        feedbackApiClient: feedbackApiClient
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
            });

            it("should display the FailedConversationView", function() {
              ocView._websocket.trigger("progress", {
                state: "terminated",
                reason: "reject"
              });

              TestUtils.findRenderedComponentWithType(ocView,
                loop.webapp.FailedConversationView);
            });

            it("should display an error message if the reason is not 'cancel'",
              function() {
                ocView._websocket.trigger("progress", {
                  state: "terminated",
                  reason: "reject"
                });

                sinon.assert.calledOnce(notifications.errorL10n);
                sinon.assert.calledWithExactly(notifications.errorL10n,
                  "call_timeout_notification_text");
              });

            it("should not display an error message if the reason is 'cancel'",
              function() {
                ocView._websocket.trigger("progress", {
                  state: "terminated",
                  reason: "cancel"
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
        it("should set display the StartConversationView", function() {
          conversation.trigger("session:ended");

          TestUtils.findRenderedComponentWithType(ocView,
            loop.webapp.EndedConversationView);
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
            conversation.setupOutgoingCall();

            TestUtils.findRenderedComponentWithType(ocView,
              loop.webapp.FailedConversationView);
          });

          it("should display an error", function() {
            conversation.setupOutgoingCall();

            sinon.assert.calledOnce(notifications.errorL10n);
          });
        });

        describe("Has loop token", function() {
          beforeEach(function() {
            sandbox.stub(conversation, "outgoing");
          });

          it("should call requestCallInfo on the client",
            function() {
              conversation.setupOutgoingCall("audio-video");

              sinon.assert.calledOnce(client.requestCallInfo);
              sinon.assert.calledWith(client.requestCallInfo, "fakeToken",
                                      "audio-video");
            });

          describe("requestCallInfo response handling", function() {
            it("should set display the CallUrlExpiredView if the call has expired",
               function() {
                client.requestCallInfo.callsArgWith(2, {errno: 105});

                conversation.setupOutgoingCall();

                TestUtils.findRenderedComponentWithType(ocView,
                  loop.webapp.CallUrlExpiredView);
              });

            it("should set display the FailedConversationView on any other error",
               function() {
                client.requestCallInfo.callsArgWith(2, {errno: 104});

                conversation.setupOutgoingCall();

                TestUtils.findRenderedComponentWithType(ocView,
                  loop.webapp.FailedConversationView);
              });

            it("should notify the user on any other error", function() {
              client.requestCallInfo.callsArgWith(2, {errno: 104});

              conversation.setupOutgoingCall();

              sinon.assert.calledOnce(notifications.errorL10n);
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

  describe("WebappRootView", function() {
    var helper, sdk, conversationModel, client, props;

    function mountTestComponent() {
      return TestUtils.renderIntoDocument(
        loop.webapp.WebappRootView({
        client: client,
        helper: helper,
        notifications: notifications,
        sdk: sdk,
        conversation: conversationModel,
        feedbackApiClient: feedbackApiClient
      }));
    }

    beforeEach(function() {
      helper = new sharedUtils.Helper();
      sdk = {
        checkSystemRequirements: function() { return true; }
      };
      conversationModel = new sharedModels.ConversationModel({}, {
        sdk: sdk
      });
      client = new loop.StandaloneClient({
        baseServerUrl: "fakeUrl"
      });
      // Stub this to stop the StartConversationView kicking in the request and
      // follow-ups.
      sandbox.stub(client, "requestCallUrlInfo");
    });

    it("should mount the unsupportedDevice view if the device is running iOS",
      function() {
        sandbox.stub(helper, "isIOS").returns(true);

        var webappRootView = mountTestComponent();

        TestUtils.findRenderedComponentWithType(webappRootView,
          loop.webapp.UnsupportedDeviceView);
      });

    it("should mount the unsupportedBrowser view if the sdk detects " +
      "the browser is unsupported", function() {
        sdk.checkSystemRequirements = function() {
          return false;
        };

        var webappRootView = mountTestComponent();

        TestUtils.findRenderedComponentWithType(webappRootView,
          loop.webapp.UnsupportedBrowserView);
      });

    it("should mount the OutgoingConversationView view if there is a loopToken",
      function() {
        conversationModel.set("loopToken", "fakeToken");

        var webappRootView = mountTestComponent();

        TestUtils.findRenderedComponentWithType(webappRootView,
          loop.webapp.OutgoingConversationView);
      });

    it("should mount the Home view there is no loopToken", function() {
        var webappRootView = mountTestComponent();

        TestUtils.findRenderedComponentWithType(webappRootView,
          loop.webapp.HomeView);
    });
  });

  describe("PendingConversationView", function() {
    var view, websocket;

    beforeEach(function() {
      websocket = new loop.CallConnectionWebSocket({
        url: "wss://fake/",
        callId: "callId",
        websocketToken: "7b"
      });

      sinon.stub(websocket, "cancel");

      view = React.addons.TestUtils.renderIntoDocument(
        loop.webapp.PendingConversationView({
          websocket: websocket
        })
      );
    });

    describe("#_cancelOutgoingCall", function() {
      it("should inform the websocket to cancel the setup", function() {
        var button = view.getDOMNode().querySelector(".btn-cancel");
        React.addons.TestUtils.Simulate.click(button);

        sinon.assert.calledOnce(websocket.cancel);
      });
    });

    describe("Events", function() {
      describe("progress:alerting", function() {
        it("should update the callstate to ringing", function () {
          websocket.trigger("progress:alerting");

          expect(view.state.callState).to.be.equal("ringing");
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
            loop.webapp.StartConversationView({
              conversation: conversation,
              notifications: notifications,
              client: standaloneClientStub
            })
        );
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
            loop.webapp.StartConversationView({
              conversation: conversation,
              notifications: notifications,
              client: {requestCallUrlInfo: requestCallUrlInfo}
            })
          );

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
        if (oldLocalStorageValue !== null)
          localStorage.setItem("has-seen-tos", oldLocalStorageValue);
      });

      it("should show the TOS", function() {
        var tos;

        view = React.addons.TestUtils.renderIntoDocument(
          loop.webapp.StartConversationView({
            conversation: conversation,
            notifications: notifications,
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
            conversation: conversation,
            notifications: notifications,
            client: {requestCallUrlInfo: requestCallUrlInfo}
          })
        );
        tos = view.getDOMNode().querySelector(".terms-service");

        expect(tos.classList.contains("hide")).to.equal(true);
      });
    });
  });

  describe("EndedConversationView", function() {
    var view, conversation;

    beforeEach(function() {
      conversation = new sharedModels.ConversationModel({}, {
        sdk: {}
      });
      view = React.addons.TestUtils.renderIntoDocument(
        loop.webapp.EndedConversationView({
          conversation: conversation,
          sdk: {},
          feedbackApiClient: feedbackApiClient,
          onAfterFeedbackReceived: function(){}
        })
      );
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
            loop.webapp.StartConversationView({
              conversation: conversation,
              notifications: notifications,
              client: standaloneClientStub
            })
        );
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
          callerId: "callerId",
          callType: "callType"
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
        });

        afterEach(function() {
          trigger.restore();
        });

        it("Activity properties", function() {
          expect(_activityProps).to.not.exist;
          model.setupOutgoingCall();
          expect(_activityProps).to.exist;
          expect(_activityProps).eql({
            name: "loop-call",
            data: {
              type: "loop/token",
              token: "fakeToken",
              callerId: "callerId",
              callType: "callType"
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
            loop.webapp.StartConversationView({
              conversation: model,
              notifications: notifications,
              client: {requestCallUrlInfo: sandbox.stub()}
            })
          );
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
