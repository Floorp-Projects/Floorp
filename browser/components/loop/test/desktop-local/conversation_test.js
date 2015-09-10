/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

describe("loop.conversation", function() {
  "use strict";

  var expect = chai.expect;
  var FeedbackView = loop.feedbackViews.FeedbackView;
  var TestUtils = React.addons.TestUtils;
  var sharedActions = loop.shared.actions;
  var sharedModels = loop.shared.models;
  var FAILURE_DETAILS = loop.shared.utils.FAILURE_DETAILS;
  var fakeWindow, sandbox, getLoopPrefStub, setLoopPrefStub, mozL10nGet;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    setLoopPrefStub = sandbox.stub();

    navigator.mozLoop = {
      doNotDisturb: true,
      getStrings: function() {
        return JSON.stringify({textContent: "fakeText"});
      },
      get locale() {
        return "en-US";
      },
      setLoopPref: setLoopPrefStub,
      getLoopPref: function(prefName) {
        switch (prefName) {
          case "contextInConversations.enabled":
            return false;
          case "debug.sdk":
            return false;
          default:
            return "http://fake";
        }
      },
      LOOP_SESSION_TYPE: {
        GUEST: 1,
        FXA: 2
      },
      startAlerting: sinon.stub(),
      stopAlerting: sinon.stub(),
      ensureRegistered: sinon.stub(),
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
      getSelectedTabMetadata: function(callback) {
        callback({});
      }
    };

    fakeWindow = {
      navigator: { mozLoop: navigator.mozLoop },
      close: sinon.stub(),
      document: {},
      addEventListener: function() {},
      removeEventListener: function() {}
    };
    loop.shared.mixins.setRootObject(fakeWindow);

    // XXX These stubs should be hoisted in a common file
    // Bug 1040968
    mozL10nGet = sandbox.stub(document.mozL10n, "get", function(x) {
      return x;
    });
    document.mozL10n.initialize(navigator.mozLoop);
  });

  afterEach(function() {
    loop.shared.mixins.setRootObject(window);
    delete navigator.mozLoop;
    sandbox.restore();
  });

  describe("#init", function() {
    var OTRestore;
    beforeEach(function() {
      sandbox.stub(React, "render");
      sandbox.stub(document.mozL10n, "initialize");

      sandbox.stub(loop.shared.models.ConversationModel.prototype,
        "initialize");

      sandbox.stub(loop.Dispatcher.prototype, "dispatch");

      sandbox.stub(loop.shared.utils,
        "locationData").returns({
          hash: "#42",
          pathname: "/"
        });

      OTRestore = window.OT;
      window.OT = {
        overrideGuidStorage: sinon.stub()
      };
    });

    afterEach(function() {
      window.OT = OTRestore;
    });

    it("should initialize L10n", function() {
      loop.conversation.init();

      sinon.assert.calledOnce(document.mozL10n.initialize);
      sinon.assert.calledWithExactly(document.mozL10n.initialize,
        navigator.mozLoop);
    });

    it("should create the AppControllerView", function() {
      loop.conversation.init();

      sinon.assert.calledOnce(React.render);
      sinon.assert.calledWith(React.render,
        sinon.match(function(value) {
          return TestUtils.isCompositeComponentElement(value,
            loop.conversation.AppControllerView);
      }));
    });

    it("should trigger a getWindowData action", function() {
      loop.conversation.init();

      sinon.assert.calledOnce(loop.Dispatcher.prototype.dispatch);
      sinon.assert.calledWithExactly(loop.Dispatcher.prototype.dispatch,
        new loop.shared.actions.GetWindowData({
          windowId: "42"
        }));
    });
  });

  describe("AppControllerView", function() {
    var conversationStore, activeRoomStore, client, ccView, dispatcher;
    var conversationAppStore, roomStore, feedbackPeriodMs = 15770000000;
    var ROOM_STATES = loop.store.ROOM_STATES;

    function mountTestComponent() {
      return TestUtils.renderIntoDocument(
        React.createElement(loop.conversation.AppControllerView, {
          roomStore: roomStore,
          dispatcher: dispatcher,
          mozLoop: navigator.mozLoop
        }));
    }

    beforeEach(function() {
      client = new loop.Client();
      dispatcher = new loop.Dispatcher();
      conversationStore = new loop.store.ConversationStore(
        dispatcher, {
          client: client,
          mozLoop: navigator.mozLoop,
          sdkDriver: {}
        });

      conversationStore.setStoreState({contact: {
        name: [ "Mr Smith" ],
        email: [{
          type: "home",
          value: "fakeEmail",
          pref: true
        }]
      }});

      activeRoomStore = new loop.store.ActiveRoomStore(dispatcher, {
        mozLoop: {},
        sdkDriver: {}
      });
      roomStore = new loop.store.RoomStore(dispatcher, {
        mozLoop: navigator.mozLoop,
        activeRoomStore: activeRoomStore
      });
      conversationAppStore = new loop.store.ConversationAppStore({
        dispatcher: dispatcher,
        mozLoop: navigator.mozLoop
      });

      loop.store.StoreMixin.register({
        conversationAppStore: conversationAppStore,
        conversationStore: conversationStore
      });
    });

    afterEach(function() {
      ccView = undefined;
    });

    it("should display the CallControllerView for outgoing calls", function() {
      conversationAppStore.setStoreState({windowType: "outgoing"});

      ccView = mountTestComponent();

      TestUtils.findRenderedComponentWithType(ccView,
        loop.conversationViews.CallControllerView);
    });

    it("should display the CallControllerView for incoming calls", function() {
      conversationAppStore.setStoreState({windowType: "incoming"});

      ccView = mountTestComponent();

      TestUtils.findRenderedComponentWithType(ccView,
        loop.conversationViews.CallControllerView);
    });

    it("should display the RoomView for rooms", function() {
      conversationAppStore.setStoreState({windowType: "room"});
      activeRoomStore.setStoreState({roomState: ROOM_STATES.READY});

      ccView = mountTestComponent();

      TestUtils.findRenderedComponentWithType(ccView,
        loop.roomViews.DesktopRoomConversationView);
    });

    it("should display the DirectCallFailureView for failures", function() {
      conversationAppStore.setStoreState({
        contact: {},
        outgoing: false,
        windowType: "failed"
      });
      conversationStore.setStoreState({
        callStateReason: FAILURE_DETAILS.UNKNOWN
      });

      ccView = mountTestComponent();

      TestUtils.findRenderedComponentWithType(ccView,
        loop.conversationViews.DirectCallFailureView);
    });

    it("should set the correct title when rendering feedback view", function() {
      conversationAppStore.setStoreState({showFeedbackForm: true});

      ccView = mountTestComponent();

      sinon.assert.calledWithExactly(mozL10nGet, "conversation_has_ended");
    });

    it("should render FeedbackView if showFeedbackForm state is true",
       function() {
         conversationAppStore.setStoreState({showFeedbackForm: true});

         ccView = mountTestComponent();

         TestUtils.findRenderedComponentWithType(ccView, FeedbackView);
       });

    it("should dispatch a ShowFeedbackForm action if timestamp is 0",
       function() {
         conversationAppStore.setStoreState({feedbackTimestamp: 0});
         sandbox.stub(dispatcher, "dispatch");

         ccView = mountTestComponent();

         ccView.handleCallTerminated();

         sinon.assert.calledOnce(dispatcher.dispatch);
         sinon.assert.calledWithExactly(dispatcher.dispatch,
                                        new sharedActions.ShowFeedbackForm());
       });

    it("should set feedback timestamp if delta is > feedback period",
       function() {
         var feedbackTimestamp = new Date() - feedbackPeriodMs;
         conversationAppStore.setStoreState({
           feedbackTimestamp: feedbackTimestamp,
           feedbackPeriod: feedbackPeriodMs
         });

         ccView = mountTestComponent();

         ccView.handleCallTerminated();

         sinon.assert.calledOnce(setLoopPrefStub);
       });

    it("should dispatch a ShowFeedbackForm action if delta > feedback period",
       function() {
         var feedbackTimestamp = new Date() - feedbackPeriodMs;
         conversationAppStore.setStoreState({
           feedbackTimestamp: feedbackTimestamp,
           feedbackPeriod: feedbackPeriodMs
         });
         sandbox.stub(dispatcher, "dispatch");

         ccView = mountTestComponent();

         ccView.handleCallTerminated();

         sinon.assert.calledOnce(dispatcher.dispatch);
         sinon.assert.calledWithExactly(dispatcher.dispatch,
                                        new sharedActions.ShowFeedbackForm());
       });

    it("should close the window if delta < feedback period", function() {
      var feedbackTimestamp = new Date().getTime();
      conversationAppStore.setStoreState({
        feedbackTimestamp: feedbackTimestamp,
        feedbackPeriod: feedbackPeriodMs
      });

      ccView = mountTestComponent();
      var closeWindowStub = sandbox.stub(ccView, "closeWindow");
      ccView.handleCallTerminated();

      sinon.assert.calledOnce(closeWindowStub);
    });

    it("should set the correct timestamp for dateLastSeenSec", function() {
      var feedbackTimestamp = new Date().getTime();
      conversationAppStore.setStoreState({
        feedbackTimestamp: feedbackTimestamp,
        feedbackPeriod: feedbackPeriodMs
      });

      ccView = mountTestComponent();
      var closeWindowStub = sandbox.stub(ccView, "closeWindow");
      ccView.handleCallTerminated();

      sinon.assert.calledOnce(closeWindowStub);
    });
  });
});
