/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var expect = chai.expect;

describe("loop.conversationViews", function () {
  "use strict";

  var sharedUtils = loop.shared.utils;
  var sharedView = loop.shared.views;
  var sandbox, oldTitle, view, dispatcher, contact, fakeAudioXHR;
  var fakeMozLoop, fakeWindow;

  var CALL_STATES = loop.store.CALL_STATES;
  var REST_ERRNOS = loop.shared.utils.REST_ERRNOS;
  var WEBSOCKET_REASONS = loop.shared.utils.WEBSOCKET_REASONS;

  // XXX refactor to Just Work with "sandbox.stubComponent" or else
  // just pass in the sandbox and put somewhere generally usable

  function stubComponent(obj, component, mockTagName){
    var reactClass = React.createClass({
      render: function() {
        var mockTagName = mockTagName || "div";
        return React.DOM[mockTagName](null, this.props.children);
      }
    });
    return sandbox.stub(obj, component, reactClass);
  }

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    sandbox.useFakeTimers();

    oldTitle = document.title;
    sandbox.stub(document.mozL10n, "get", function(x) {
      return x;
    });

    dispatcher = new loop.Dispatcher();
    sandbox.stub(dispatcher, "dispatch");

    contact = {
      name: [ "mrsmith" ],
      email: [{
        type: "home",
        value: "fakeEmail",
        pref: true
      }]
    };
    fakeAudioXHR = {
      open: sinon.spy(),
      send: function() {},
      abort: function() {},
      getResponseHeader: function(header) {
        if (header === "Content-Type")
          return "audio/ogg";
      },
      responseType: null,
      response: new ArrayBuffer(10),
      onload: null
    };

    fakeMozLoop = navigator.mozLoop = {
      // Dummy function, stubbed below.
      getLoopPref: function() {},
      calls: {
        clearCallInProgress: sinon.stub()
      },
      composeEmail: sinon.spy(),
      get appVersionInfo() {
        return {
          version: "42",
          channel: "test",
          platform: "test"
        };
      },
      getAudioBlob: sinon.spy(function(name, callback) {
        callback(null, new Blob([new ArrayBuffer(10)], {type: "audio/ogg"}));
      }),
      startAlerting: sinon.stub(),
      stopAlerting: sinon.stub(),
      userProfile: {
        email: "bob@invalid.tld"
      }
    };
    sinon.stub(fakeMozLoop, "getLoopPref", function(pref) {
        if (pref === "fake") {
          return"http://fakeurl";
        }

        return false;
    });

    fakeWindow = {
      navigator: { mozLoop: fakeMozLoop },
      close: sinon.stub(),
      addEventListener: function() {},
      removeEventListener: function() {}
    };
    loop.shared.mixins.setRootObject(fakeWindow);

  });

  afterEach(function() {
    loop.shared.mixins.setRootObject(window);
    document.title = oldTitle;
    view = undefined;
    delete navigator.mozLoop;
    sandbox.restore();
  });

  describe("CallIdentifierView", function() {
    function mountTestComponent(props) {
      return TestUtils.renderIntoDocument(
        React.createElement(loop.conversationViews.CallIdentifierView, props));
    }

    it("should set display the peer identifer", function() {
      view = mountTestComponent({
        showIcons: false,
        peerIdentifier: "mrssmith"
      });

      expect(TestUtils.findRenderedDOMComponentWithClass(
        view, "fx-embedded-call-identifier-text").props.children).eql("mrssmith");
    });

    it("should not display the icons if showIcons is false", function() {
      view = mountTestComponent({
        showIcons: false,
        peerIdentifier: "mrssmith"
      });

      expect(TestUtils.findRenderedDOMComponentWithClass(
        view, "fx-embedded-call-detail").props.className).to.contain("hide");
    });

    it("should display the icons if showIcons is true", function() {
      view = mountTestComponent({
        showIcons: true,
        peerIdentifier: "mrssmith"
      });

      expect(TestUtils.findRenderedDOMComponentWithClass(
        view, "fx-embedded-call-detail").props.className).to.not.contain("hide");
    });

    it("should display the url timestamp", function() {
      sandbox.stub(loop.shared.utils, "formatDate").returns(("October 9, 2014"));

      view = mountTestComponent({
        showIcons: true,
        peerIdentifier: "mrssmith",
        urlCreationDate: (new Date() / 1000).toString()
      });

      expect(TestUtils.findRenderedDOMComponentWithClass(
        view, "fx-embedded-conversation-timestamp").props.children).eql("(October 9, 2014)");
    });

    it("should show video as muted if video is false", function() {
      view = mountTestComponent({
        showIcons: true,
        peerIdentifier: "mrssmith",
        video: false
      });

      expect(TestUtils.findRenderedDOMComponentWithClass(
        view, "fx-embedded-tiny-video-icon").props.className).to.contain("muted");
    });
  });

  describe("ConversationDetailView", function() {
    function mountTestComponent(props) {
      return TestUtils.renderIntoDocument(
        React.createElement(loop.conversationViews.ConversationDetailView, props));
    }

    it("should set the document title to the calledId", function() {
      mountTestComponent({contact: contact});

      expect(document.title).eql("mrsmith");
    });

    it("should fallback to the email if the contact name is not defined",
      function() {
        delete contact.name;

        mountTestComponent({contact: contact});

        expect(document.title).eql("fakeEmail");
      });
  });

  describe("PendingConversationView", function() {
    function mountTestComponent(props) {
      return TestUtils.renderIntoDocument(
        React.createElement(loop.conversationViews.PendingConversationView, props));
    }

    it("should set display connecting string when the state is not alerting",
      function() {
        view = mountTestComponent({
          callState: CALL_STATES.CONNECTING,
          contact: contact,
          dispatcher: dispatcher
        });

        var label = TestUtils.findRenderedDOMComponentWithClass(
          view, "btn-label").props.children;

        expect(label).to.have.string("connecting");
    });

    it("should set display ringing string when the state is alerting",
      function() {
        view = mountTestComponent({
          callState: CALL_STATES.ALERTING,
          contact: contact,
          dispatcher: dispatcher
        });

        var label = TestUtils.findRenderedDOMComponentWithClass(
          view, "btn-label").props.children;

        expect(label).to.have.string("ringing");
    });

    it("should disable the cancel button if enableCancelButton is false",
      function() {
        view = mountTestComponent({
          callState: CALL_STATES.CONNECTING,
          contact: contact,
          dispatcher: dispatcher,
          enableCancelButton: false
        });

        var cancelBtn = view.getDOMNode().querySelector('.btn-cancel');

        expect(cancelBtn.classList.contains("disabled")).eql(true);
      });

    it("should enable the cancel button if enableCancelButton is false",
      function() {
        view = mountTestComponent({
          callState: CALL_STATES.CONNECTING,
          contact: contact,
          dispatcher: dispatcher,
          enableCancelButton: true
        });

        var cancelBtn = view.getDOMNode().querySelector('.btn-cancel');

        expect(cancelBtn.classList.contains("disabled")).eql(false);
      });

    it("should dispatch a cancelCall action when the cancel button is pressed",
      function() {
        view = mountTestComponent({
          callState: CALL_STATES.CONNECTING,
          contact: contact,
          dispatcher: dispatcher
        });

        var cancelBtn = view.getDOMNode().querySelector('.btn-cancel');

        React.addons.TestUtils.Simulate.click(cancelBtn);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "cancelCall"));
      });
  });

  describe("CallFailedView", function() {
    var store, fakeAudio;

    var contact = {email: [{value: "test@test.tld"}]};

    function mountTestComponent(options) {
      options = options || {};
      return TestUtils.renderIntoDocument(
        React.createElement(loop.conversationViews.CallFailedView, {
          dispatcher: dispatcher,
          store: store,
          contact: options.contact
        }));
    }

    beforeEach(function() {
      store = new loop.store.ConversationStore(dispatcher, {
        client: {},
        mozLoop: navigator.mozLoop,
        sdkDriver: {}
      });
      fakeAudio = {
        play: sinon.spy(),
        pause: sinon.spy(),
        removeAttribute: sinon.spy()
      };
      sandbox.stub(window, "Audio").returns(fakeAudio);
    });

    it("should dispatch a retryCall action when the retry button is pressed",
      function() {
        view = mountTestComponent({contact: contact});

        var retryBtn = view.getDOMNode().querySelector('.btn-retry');

        React.addons.TestUtils.Simulate.click(retryBtn);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "retryCall"));
      });

    it("should dispatch a cancelCall action when the cancel button is pressed",
      function() {
        view = mountTestComponent({contact: contact});

        var cancelBtn = view.getDOMNode().querySelector('.btn-cancel');

        React.addons.TestUtils.Simulate.click(cancelBtn);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "cancelCall"));
      });

    it("should dispatch a fetchRoomEmailLink action when the email button is pressed",
      function() {
        view = mountTestComponent({contact: contact});

        var emailLinkBtn = view.getDOMNode().querySelector('.btn-email');

        React.addons.TestUtils.Simulate.click(emailLinkBtn);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "fetchRoomEmailLink"));
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("roomOwner", fakeMozLoop.userProfile.email));
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("roomName", "test@test.tld"));
      });

    it("should name the created room using the contact name when available",
      function() {
        view = mountTestComponent({contact: {
          email: [{value: "test@test.tld"}],
          name: ["Mr Fake ContactName"]
        }});

        var emailLinkBtn = view.getDOMNode().querySelector('.btn-email');

        React.addons.TestUtils.Simulate.click(emailLinkBtn);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("roomName", "Mr Fake ContactName"));
      });

    it("should disable the email link button once the action is dispatched",
      function() {
        view = mountTestComponent({contact: contact});
        var emailLinkBtn = view.getDOMNode().querySelector('.btn-email');
        React.addons.TestUtils.Simulate.click(emailLinkBtn);

        expect(view.getDOMNode().querySelector(".btn-email").disabled).eql(true);
      });

    it("should compose an email once the email link is received", function() {
      var composeCallUrlEmail = sandbox.stub(sharedUtils, "composeCallUrlEmail");
      view = mountTestComponent({contact: contact});
      store.setStoreState({emailLink: "http://fake.invalid/"});

      sinon.assert.calledOnce(composeCallUrlEmail);
      sinon.assert.calledWithExactly(composeCallUrlEmail,
        "http://fake.invalid/", "test@test.tld");
    });

    it("should close the conversation window once the email link is received",
      function() {
        view = mountTestComponent({contact: contact});

        store.setStoreState({emailLink: "http://fake.invalid/"});

        sinon.assert.calledOnce(fakeWindow.close);
      });

    it("should display an error message in case email link retrieval failed",
      function() {
        view = mountTestComponent({contact: contact});

        store.trigger("error:emailLink");

        expect(view.getDOMNode().querySelector(".error")).not.eql(null);
      });

    it("should allow retrying to get a call url if it failed previously",
      function() {
        view = mountTestComponent({contact: contact});

        store.trigger("error:emailLink");

        expect(view.getDOMNode().querySelector(".btn-email").disabled).eql(false);
      });

    it("should play a failure sound, once", function() {
      view = mountTestComponent({contact: contact});

      sinon.assert.calledOnce(navigator.mozLoop.getAudioBlob);
      sinon.assert.calledWithExactly(navigator.mozLoop.getAudioBlob,
                                     "failure", sinon.match.func);
      sinon.assert.calledOnce(fakeAudio.play);
      expect(fakeAudio.loop).to.equal(false);
    });

    it("should show 'something went wrong' when the reason is WEBSOCKET_REASONS.MEDIA_FAIL",
      function () {
        store.setStoreState({callStateReason: WEBSOCKET_REASONS.MEDIA_FAIL});

        view = mountTestComponent({contact: contact});

        sinon.assert.calledWith(document.mozL10n.get, "generic_failure_title");
      });

    it("should show 'contact unavailable' when the reason is WEBSOCKET_REASONS.REJECT",
      function () {
        store.setStoreState({callStateReason: WEBSOCKET_REASONS.REJECT});

        view = mountTestComponent({contact: contact});

        sinon.assert.calledWithExactly(document.mozL10n.get,
          "contact_unavailable_title",
          {contactName: loop.conversationViews._getContactDisplayName(contact)});
      });

    it("should show 'contact unavailable' when the reason is WEBSOCKET_REASONS.BUSY",
      function () {
        store.setStoreState({callStateReason: WEBSOCKET_REASONS.BUSY});

        view = mountTestComponent({contact: contact});

        sinon.assert.calledWithExactly(document.mozL10n.get,
          "contact_unavailable_title",
          {contactName: loop.conversationViews._getContactDisplayName(contact)});
      });

    it("should show 'something went wrong' when the reason is 'setup'",
      function () {
        store.setStoreState({callStateReason: "setup"});

        view = mountTestComponent({contact: contact});

        sinon.assert.calledWithExactly(document.mozL10n.get,
          "generic_failure_title");
      });

    it("should show 'contact unavailable' when the reason is REST_ERRNOS.USER_UNAVAILABLE",
      function () {
        store.setStoreState({callStateReason: REST_ERRNOS.USER_UNAVAILABLE});

        view = mountTestComponent({contact: contact});

        sinon.assert.calledWithExactly(document.mozL10n.get,
          "contact_unavailable_title",
          {contactName: loop.conversationViews._getContactDisplayName(contact)});
      });

    it("should display a generic contact unavailable msg when the reason is" +
       " WEBSOCKET_REASONS.BUSY and no display name is available", function() {
        store.setStoreState({callStateReason: WEBSOCKET_REASONS.BUSY});
        var phoneOnlyContact = {
          tel: [{"pref": true, type: "work", value: ""}]
        };

        view = mountTestComponent({contact: phoneOnlyContact});

        sinon.assert.calledWith(document.mozL10n.get,
          "generic_contact_unavailable_title");
    });
  });

  describe("OngoingConversationView", function() {
    function mountTestComponent(props) {
      return TestUtils.renderIntoDocument(
        React.createElement(loop.conversationViews.OngoingConversationView, props));
    }

    it("should dispatch a setupStreamElements action when the view is created",
      function() {
        view = mountTestComponent({
          dispatcher: dispatcher
        });

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "setupStreamElements"));
      });

    it("should dispatch a hangupCall action when the hangup button is pressed",
      function() {
        view = mountTestComponent({
          dispatcher: dispatcher
        });

        var hangupBtn = view.getDOMNode().querySelector('.btn-hangup');

        React.addons.TestUtils.Simulate.click(hangupBtn);

        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "hangupCall"));
      });

    it("should dispatch a setMute action when the audio mute button is pressed",
      function() {
        view = mountTestComponent({
          dispatcher: dispatcher,
          audio: {enabled: false}
        });

        var muteBtn = view.getDOMNode().querySelector('.btn-mute-audio');

        React.addons.TestUtils.Simulate.click(muteBtn);

        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "setMute"));
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("enabled", true));
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("type", "audio"));
      });

    it("should dispatch a setMute action when the video mute button is pressed",
      function() {
        view = mountTestComponent({
          dispatcher: dispatcher,
          video: {enabled: true}
        });

        var muteBtn = view.getDOMNode().querySelector('.btn-mute-video');

        React.addons.TestUtils.Simulate.click(muteBtn);

        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "setMute"));
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("enabled", false));
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("type", "video"));
      });

    it("should set the mute button as mute off", function() {
      view = mountTestComponent({
        dispatcher: dispatcher,
        video: {enabled: true}
      });

      var muteBtn = view.getDOMNode().querySelector('.btn-mute-video');

      expect(muteBtn.classList.contains("muted")).eql(false);
    });

    it("should set the mute button as mute on", function() {
      view = mountTestComponent({
        dispatcher: dispatcher,
        audio: {enabled: false}
      });

      var muteBtn = view.getDOMNode().querySelector('.btn-mute-audio');

      expect(muteBtn.classList.contains("muted")).eql(true);
    });
  });

  describe("OutgoingConversationView", function() {
    var store, feedbackStore;

    function mountTestComponent() {
      return TestUtils.renderIntoDocument(
        React.createElement(loop.conversationViews.OutgoingConversationView, {
          dispatcher: dispatcher,
          store: store
        }));
    }

    beforeEach(function() {
      store = new loop.store.ConversationStore(dispatcher, {
        client: {},
        mozLoop: fakeMozLoop,
        sdkDriver: {}
      });
      feedbackStore = new loop.store.FeedbackStore(dispatcher, {
        feedbackClient: {}
      });
    });

    it("should render the CallFailedView when the call state is 'terminated'",
      function() {
        store.setStoreState({
          callState: CALL_STATES.TERMINATED,
          contact: contact
        });

        view = mountTestComponent();

        TestUtils.findRenderedComponentWithType(view,
          loop.conversationViews.CallFailedView);
    });

    it("should render the PendingConversationView when the call state is 'gather'",
      function() {
        store.setStoreState({
          callState: CALL_STATES.GATHER,
          contact: contact
        });

        view = mountTestComponent();

        TestUtils.findRenderedComponentWithType(view,
          loop.conversationViews.PendingConversationView);
    });

    it("should render the OngoingConversationView when the call state is 'ongoing'",
      function() {
        store.setStoreState({callState: CALL_STATES.ONGOING});

        view = mountTestComponent();

        TestUtils.findRenderedComponentWithType(view,
          loop.conversationViews.OngoingConversationView);
    });

    it("should render the FeedbackView when the call state is 'finished'",
      function() {
        store.setStoreState({callState: CALL_STATES.FINISHED});

        view = mountTestComponent();

        TestUtils.findRenderedComponentWithType(view,
          loop.shared.views.FeedbackView);
    });

    it("should play the terminated sound when the call state is 'finished'",
      function() {
        var fakeAudio = {
          play: sinon.spy(),
          pause: sinon.spy(),
          removeAttribute: sinon.spy()
        };
        sandbox.stub(window, "Audio").returns(fakeAudio);

        store.setStoreState({callState: CALL_STATES.FINISHED});

        view = mountTestComponent();

        sinon.assert.calledOnce(fakeAudio.play);
    });

    it("should update the rendered views when the state is changed.",
      function() {
        store.setStoreState({
          callState: CALL_STATES.GATHER,
          contact: contact
        });

        view = mountTestComponent();

        TestUtils.findRenderedComponentWithType(view,
          loop.conversationViews.PendingConversationView);

        store.setStoreState({callState: CALL_STATES.TERMINATED});

        TestUtils.findRenderedComponentWithType(view,
          loop.conversationViews.CallFailedView);
    });
  });

  describe("IncomingConversationView", function() {
    var conversationAppStore, conversation, client, icView, oldTitle,
        feedbackStore;

    function mountTestComponent() {
      return TestUtils.renderIntoDocument(
        React.createElement(loop.conversationViews.IncomingConversationView, {
          client: client,
          conversation: conversation,
          sdk: {},
          conversationAppStore: conversationAppStore
        }));
    }

    beforeEach(function() {
      oldTitle = document.title;
      client = new loop.Client();
      conversation = new loop.shared.models.ConversationModel({}, {
        sdk: {}
      });
      conversation.set({windowId: 42});
      var dispatcher = new loop.Dispatcher();
      conversationAppStore = new loop.store.ConversationAppStore({
        dispatcher: dispatcher,
        mozLoop: navigator.mozLoop
      });
      feedbackStore = new loop.store.FeedbackStore(dispatcher, {
        feedbackClient: {}
      });
      sandbox.stub(conversation, "setOutgoingSessionData");
    });

    afterEach(function() {
      icView = undefined;
      document.title = oldTitle;
    });

    describe("start", function() {
      it("should set the title to incoming_call_title2", function() {
        conversationAppStore.setStoreState({
          windowData: {
            progressURL:    "fake",
            websocketToken: "fake",
            callId: 42
          }
        });

        icView = mountTestComponent();

        expect(document.title).eql("incoming_call_title2");
      });
    });

    describe("componentDidMount", function() {
      var fakeSessionData, promise, resolveWebSocketConnect;
      var rejectWebSocketConnect;

      beforeEach(function() {
        fakeSessionData  = {
          sessionId:      "sessionId",
          sessionToken:   "sessionToken",
          apiKey:         "apiKey",
          callType:       "callType",
          callId:         "Hello",
          progressURL:    "http://progress.example.com",
          websocketToken: "7b"
        };

        conversationAppStore.setStoreState({
          windowData: fakeSessionData
        });

        stubComponent(loop.conversationViews, "IncomingCallView");
        stubComponent(sharedView, "ConversationView");
      });

      it("should start alerting", function() {
        icView = mountTestComponent();

        sinon.assert.calledOnce(navigator.mozLoop.startAlerting);
      });

      describe("Session Data setup", function() {
        beforeEach(function() {
          sandbox.stub(loop, "CallConnectionWebSocket").returns({
            promiseConnect: function () {
              promise = new Promise(function(resolve, reject) {
                resolveWebSocketConnect = resolve;
                rejectWebSocketConnect = reject;
              });
              return promise;
            },
            on: sinon.stub()
          });
        });

        it("should store the session data", function() {
          sandbox.stub(conversation, "setIncomingSessionData");

          icView = mountTestComponent();

          sinon.assert.calledOnce(conversation.setIncomingSessionData);
          sinon.assert.calledWithExactly(conversation.setIncomingSessionData,
                                         fakeSessionData);
        });

        it("should setup the websocket connection", function() {
          icView = mountTestComponent();

          sinon.assert.calledOnce(loop.CallConnectionWebSocket);
          sinon.assert.calledWithExactly(loop.CallConnectionWebSocket, {
            callId: "Hello",
            url: "http://progress.example.com",
            websocketToken: "7b"
          });
        });
      });

      describe("WebSocket Handling", function() {
        beforeEach(function() {
          promise = new Promise(function(resolve, reject) {
            resolveWebSocketConnect = resolve;
            rejectWebSocketConnect = reject;
          });

          sandbox.stub(loop.CallConnectionWebSocket.prototype, "promiseConnect").returns(promise);
        });

        it("should set the state to incoming on success", function(done) {
          icView = mountTestComponent();
          resolveWebSocketConnect("incoming");

          promise.then(function () {
            expect(icView.state.callStatus).eql("incoming");
            done();
          });
        });

        it("should set the state to close on success if the progress " +
          "state is terminated", function(done) {
            icView = mountTestComponent();
            resolveWebSocketConnect("terminated");

            promise.then(function () {
              expect(icView.state.callStatus).eql("close");
              done();
            });
          });

        // XXX implement me as part of bug 1047410
        // see https://hg.mozilla.org/integration/fx-team/rev/5d2c69ebb321#l18.259
        it.skip("should should switch view state to failed", function(done) {
          icView = mountTestComponent();
          rejectWebSocketConnect();

          promise.then(function() {}, function() {
            done();
          });
        });
      });

      describe("WebSocket Events", function() {
        describe("Call cancelled or timed out before acceptance", function() {
          beforeEach(function() {
            // Mounting the test component automatically calls the required
            // setup functions
            icView = mountTestComponent();
            promise = new Promise(function(resolve, reject) {
              resolve();
            });

            sandbox.stub(loop.CallConnectionWebSocket.prototype, "promiseConnect").returns(promise);
            sandbox.stub(loop.CallConnectionWebSocket.prototype, "close");
          });

          describe("progress - terminated (previousState = alerting)", function() {
            it("should stop alerting", function(done) {
              promise.then(function() {
                icView._websocket.trigger("progress", {
                  state: "terminated",
                  reason: WEBSOCKET_REASONS.TIMEOUT
                }, "alerting");

                sinon.assert.calledOnce(navigator.mozLoop.stopAlerting);
                done();
              });
            });

            it("should close the websocket", function(done) {
              promise.then(function() {
                icView._websocket.trigger("progress", {
                  state: "terminated",
                  reason: WEBSOCKET_REASONS.CLOSED
                }, "alerting");

                sinon.assert.calledOnce(icView._websocket.close);
                done();
              });
            });

            it("should close the window", function(done) {
              promise.then(function() {
                icView._websocket.trigger("progress", {
                  state: "terminated",
                  reason: WEBSOCKET_REASONS.ANSWERED_ELSEWHERE
                }, "alerting");

                sandbox.clock.tick(1);

                sinon.assert.calledOnce(fakeWindow.close);
                done();
              });
            });
          });


          describe("progress - terminated (previousState not init" +
                   " nor alerting)",
            function() {
              it("should set the state to end", function(done) {
                promise.then(function() {
                  icView._websocket.trigger("progress", {
                    state: "terminated",
                    reason: WEBSOCKET_REASONS.MEDIA_FAIL
                  }, "connecting");

                  expect(icView.state.callStatus).eql("end");
                  done();
                });
              });

              it("should stop alerting", function(done) {
                promise.then(function() {
                  icView._websocket.trigger("progress", {
                    state: "terminated",
                    reason: WEBSOCKET_REASONS.MEDIA_FAIL
                  }, "connecting");

                  sinon.assert.calledOnce(navigator.mozLoop.stopAlerting);
                  done();
                });
              });
            });
        });
      });

      describe("#accept", function() {
        beforeEach(function() {
          icView = mountTestComponent();
          conversation.setIncomingSessionData({
            sessionId:      "sessionId",
            sessionToken:   "sessionToken",
            apiKey:         "apiKey",
            callType:       "callType",
            callId:         "Hello",
            progressURL:    "http://progress.example.com",
            websocketToken: 123
          });

          sandbox.stub(icView._websocket, "accept");
          sandbox.stub(icView.props.conversation, "accepted");
        });

        it("should initiate the conversation", function() {
          icView.accept();

          sinon.assert.calledOnce(icView.props.conversation.accepted);
        });

        it("should notify the websocket of the user acceptance", function() {
          icView.accept();

          sinon.assert.calledOnce(icView._websocket.accept);
        });

        it("should stop alerting", function() {
          icView.accept();

          sinon.assert.calledOnce(navigator.mozLoop.stopAlerting);
        });
      });

      describe("#decline", function() {
        beforeEach(function() {
          icView = mountTestComponent();

          icView._websocket = {
            decline: sinon.stub(),
            close: sinon.stub()
          };
          conversation.set({
            windowId: "8699"
          });
          conversation.setIncomingSessionData({
            websocketToken: 123
          });
        });

        it("should close the window", function() {
          icView.decline();

          sandbox.clock.tick(1);

          sinon.assert.calledOnce(fakeWindow.close);
        });

        it("should stop alerting", function() {
          icView.decline();

          sinon.assert.calledOnce(navigator.mozLoop.stopAlerting);
        });

        it("should release callData", function() {
          icView.decline();

          sinon.assert.calledOnce(navigator.mozLoop.calls.clearCallInProgress);
          sinon.assert.calledWithExactly(
            navigator.mozLoop.calls.clearCallInProgress, "8699");
        });
      });

      describe("#blocked", function() {
        var mozLoop, deleteCallUrlStub;

        beforeEach(function() {
          icView = mountTestComponent();

          icView._websocket = {
            decline: sinon.spy(),
            close: sinon.stub()
          };

          mozLoop = {
            LOOP_SESSION_TYPE: {
              GUEST: 1,
              FXA: 2
            }
          };

          deleteCallUrlStub = sandbox.stub(loop.Client.prototype,
                                           "deleteCallUrl");
        });

        it("should call mozLoop.stopAlerting", function() {
          icView.declineAndBlock();

          sinon.assert.calledOnce(navigator.mozLoop.stopAlerting);
        });

        it("should call delete call", function() {
          sandbox.stub(conversation, "get").withArgs("callToken")
                                           .returns("fakeToken")
                                           .withArgs("sessionType")
                                           .returns(mozLoop.LOOP_SESSION_TYPE.FXA);

          icView.declineAndBlock();

          sinon.assert.calledOnce(deleteCallUrlStub);
          sinon.assert.calledWithExactly(deleteCallUrlStub,
            "fakeToken", mozLoop.LOOP_SESSION_TYPE.FXA, sinon.match.func);
        });

        it("should get callToken from conversation model", function() {
          sandbox.stub(conversation, "get");
          icView.declineAndBlock();

          sinon.assert.called(conversation.get);
          sinon.assert.calledWithExactly(conversation.get, "callToken");
          sinon.assert.calledWithExactly(conversation.get, "windowId");
        });

        it("should trigger error handling in case of error", function() {
          // XXX just logging to console for now
          var log = sandbox.stub(console, "log");
          var fakeError = {
            error: true
          };
          deleteCallUrlStub.callsArgWith(2, fakeError);
          icView.declineAndBlock();

          sinon.assert.calledOnce(log);
          sinon.assert.calledWithExactly(log, fakeError);
        });

        it("should close the window", function() {
          icView.declineAndBlock();

          sandbox.clock.tick(1);

          sinon.assert.calledOnce(fakeWindow.close);
        });
      });
    });

    describe("Events", function() {
      var fakeSessionData;

      beforeEach(function() {

        fakeSessionData = {
          sessionId:    "sessionId",
          sessionToken: "sessionToken",
          apiKey:       "apiKey"
        };

        conversationAppStore.setStoreState({
          windowData: fakeSessionData
        });

        sandbox.stub(conversation, "setIncomingSessionData");
        sandbox.stub(loop, "CallConnectionWebSocket").returns({
          promiseConnect: function() {
            return new Promise(function() {});
          },
          on: sandbox.spy()
        });

        icView = mountTestComponent();

        conversation.set("loopToken", "fakeToken");
        stubComponent(sharedView, "ConversationView");
      });

      describe("call:accepted", function() {
        it("should display the ConversationView",
          function() {
            conversation.accepted();

            TestUtils.findRenderedComponentWithType(icView,
              sharedView.ConversationView);
          });

        it("should set the title to the call identifier", function() {
          sandbox.stub(conversation, "getCallIdentifier").returns("fakeId");

          conversation.accepted();

          expect(document.title).eql("fakeId");
        });
      });

      describe("session:ended", function() {
        it("should display the feedback view when the call session ends",
          function() {
            conversation.trigger("session:ended");

            TestUtils.findRenderedComponentWithType(icView,
              sharedView.FeedbackView);
          });
      });

      describe("session:peer-hungup", function() {
        it("should display the feedback view when the peer hangs up",
          function() {
            conversation.trigger("session:peer-hungup");

              TestUtils.findRenderedComponentWithType(icView,
                sharedView.FeedbackView);
          });
      });

      describe("session:network-disconnected", function() {
        it("should navigate to call failed when network disconnects",
          function() {
            conversation.trigger("session:network-disconnected");

            TestUtils.findRenderedComponentWithType(icView,
              loop.conversationViews.GenericFailureView);
          });

        it("should update the conversation window toolbar title",
          function() {
            conversation.trigger("session:network-disconnected");

            expect(document.title).eql("generic_failure_title");
          });
      });

      describe("Published and Subscribed Streams", function() {
        beforeEach(function() {
          icView._websocket = {
            mediaUp: sinon.spy()
          };
        });

        describe("publishStream", function() {
          it("should not notify the websocket if only one stream is up",
            function() {
              conversation.set("publishedStream", true);

              sinon.assert.notCalled(icView._websocket.mediaUp);
            });

          it("should notify the websocket that media is up if both streams" +
             "are connected", function() {
              conversation.set("subscribedStream", true);
              conversation.set("publishedStream", true);

              sinon.assert.calledOnce(icView._websocket.mediaUp);
            });
        });

        describe("subscribedStream", function() {
          it("should not notify the websocket if only one stream is up",
            function() {
              conversation.set("subscribedStream", true);

              sinon.assert.notCalled(icView._websocket.mediaUp);
            });

          it("should notify the websocket that media is up if both streams" +
             "are connected", function() {
              conversation.set("publishedStream", true);
              conversation.set("subscribedStream", true);

              sinon.assert.calledOnce(icView._websocket.mediaUp);
            });
        });
      });
    });
  });

  describe("IncomingCallView", function() {
    var view, model, fakeAudio;

    beforeEach(function() {
      var Model = Backbone.Model.extend({
        getCallIdentifier: function() {return "fakeId";}
      });
      model = new Model();
      sandbox.spy(model, "trigger");
      sandbox.stub(model, "set");

      fakeAudio = {
        play: sinon.spy(),
        pause: sinon.spy(),
        removeAttribute: sinon.spy()
      };
      sandbox.stub(window, "Audio").returns(fakeAudio);

      view = TestUtils.renderIntoDocument(
        React.createElement(loop.conversationViews.IncomingCallView, {
          model: model,
          video: true
        }));
    });

    describe("default answer mode", function() {
      it("should display video as primary answer mode", function() {
        view = TestUtils.renderIntoDocument(
          React.createElement(loop.conversationViews.IncomingCallView, {
            model: model,
            video: true
          }));
        var primaryBtn = view.getDOMNode()
                                  .querySelector('.fx-embedded-btn-icon-video');

        expect(primaryBtn).not.to.eql(null);
      });

      it("should display audio as primary answer mode", function() {
        view = TestUtils.renderIntoDocument(
          React.createElement(loop.conversationViews.IncomingCallView, {
            model: model,
            video: false
          }));
        var primaryBtn = view.getDOMNode()
                                  .querySelector('.fx-embedded-btn-icon-audio');

        expect(primaryBtn).not.to.eql(null);
      });

      it("should accept call with video", function() {
        view = TestUtils.renderIntoDocument(
          React.createElement(loop.conversationViews.IncomingCallView, {
            model: model,
            video: true
          }));
        var primaryBtn = view.getDOMNode()
                                  .querySelector('.fx-embedded-btn-icon-video');

        React.addons.TestUtils.Simulate.click(primaryBtn);

        sinon.assert.calledOnce(model.set);
        sinon.assert.calledWithExactly(model.set, "selectedCallType", "audio-video");
        sinon.assert.calledOnce(model.trigger);
        sinon.assert.calledWithExactly(model.trigger, "accept");
      });

      it("should accept call with audio", function() {
        view = TestUtils.renderIntoDocument(
          React.createElement(loop.conversationViews.IncomingCallView, {
            model: model,
            video: false
          }));
        var primaryBtn = view.getDOMNode()
                                  .querySelector('.fx-embedded-btn-icon-audio');

        React.addons.TestUtils.Simulate.click(primaryBtn);

        sinon.assert.calledOnce(model.set);
        sinon.assert.calledWithExactly(model.set, "selectedCallType", "audio");
        sinon.assert.calledOnce(model.trigger);
        sinon.assert.calledWithExactly(model.trigger, "accept");
      });

      it("should accept call with video when clicking on secondary btn",
         function() {
          view = TestUtils.renderIntoDocument(
            React.createElement(loop.conversationViews.IncomingCallView, {
              model: model,
              video: false
            }));
          var secondaryBtn = view.getDOMNode()
          .querySelector('.fx-embedded-btn-video-small');

          React.addons.TestUtils.Simulate.click(secondaryBtn);

          sinon.assert.calledOnce(model.set);
          sinon.assert.calledWithExactly(model.set, "selectedCallType", "audio-video");
          sinon.assert.calledOnce(model.trigger);
          sinon.assert.calledWithExactly(model.trigger, "accept");
         });

      it("should accept call with audio when clicking on secondary btn",
         function() {
          view = TestUtils.renderIntoDocument(
            React.createElement(loop.conversationViews.IncomingCallView, {
              model: model,
              video: true
            }));
          var secondaryBtn = view.getDOMNode()
          .querySelector('.fx-embedded-btn-audio-small');

          React.addons.TestUtils.Simulate.click(secondaryBtn);

          sinon.assert.calledOnce(model.set);
          sinon.assert.calledWithExactly(model.set, "selectedCallType", "audio");
          sinon.assert.calledOnce(model.trigger);
          sinon.assert.calledWithExactly(model.trigger, "accept");
         });
    });

    describe("click event on .btn-accept", function() {
      it("should trigger an 'accept' conversation model event", function () {
        var buttonAccept = view.getDOMNode().querySelector(".btn-accept");
        model.trigger.withArgs("accept");
        TestUtils.Simulate.click(buttonAccept);

        /* Setting a model property triggers 2 events */
        sinon.assert.calledOnce(model.trigger.withArgs("accept"));
      });

      it("should set selectedCallType to audio-video", function () {
        var buttonAccept = view.getDOMNode().querySelector(".btn-accept");

        TestUtils.Simulate.click(buttonAccept);

        sinon.assert.calledOnce(model.set);
        sinon.assert.calledWithExactly(model.set, "selectedCallType",
          "audio-video");
      });
    });

    describe("click event on .btn-decline", function() {
      it("should trigger an 'decline' conversation model event", function() {
        var buttonDecline = view.getDOMNode().querySelector(".btn-decline");

        TestUtils.Simulate.click(buttonDecline);

        sinon.assert.calledOnce(model.trigger);
        sinon.assert.calledWith(model.trigger, "decline");
        });
    });

    describe("click event on .btn-block", function() {
      it("should trigger a 'block' conversation model event", function() {
        var buttonBlock = view.getDOMNode().querySelector(".btn-block");

        TestUtils.Simulate.click(buttonBlock);

        sinon.assert.calledOnce(model.trigger);
        sinon.assert.calledWith(model.trigger, "declineAndBlock");
      });
    });
  });

  describe("GenericFailureView", function() {
    var view, fakeAudio;

    beforeEach(function() {
      fakeAudio = {
        play: sinon.spy(),
        pause: sinon.spy(),
        removeAttribute: sinon.spy()
      };
      navigator.mozLoop.doNotDisturb = false;
      sandbox.stub(window, "Audio").returns(fakeAudio);

      view = TestUtils.renderIntoDocument(
        React.createElement(loop.conversationViews.GenericFailureView, {
          cancelCall: function() {}
        }));
    });

    it("should play a failure sound, once", function() {
      sinon.assert.calledOnce(navigator.mozLoop.getAudioBlob);
      sinon.assert.calledWithExactly(navigator.mozLoop.getAudioBlob,
                                     "failure", sinon.match.func);
      sinon.assert.calledOnce(fakeAudio.play);
      expect(fakeAudio.loop).to.equal(false);
    });

  });
});
