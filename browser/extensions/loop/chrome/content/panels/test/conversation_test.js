/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

describe("loop.conversation", function() {
  "use strict";

  var FeedbackView = loop.feedbackViews.FeedbackView;
  var expect = chai.expect;
  var TestUtils = React.addons.TestUtils;
  var sharedActions = loop.shared.actions;
  var fakeWindow, sandbox, setLoopPrefStub, mozL10nGet,
    remoteCursorStore, dispatcher, requestStubs;

  beforeEach(function() {
    sandbox = LoopMochaUtils.createSandbox();
    setLoopPrefStub = sandbox.stub();

    LoopMochaUtils.stubLoopRequest(requestStubs = {
      GetDoNotDisturb: function() { return true; },
      GetAllStrings: function() {
        return JSON.stringify({ textContent: "fakeText" });
      },
      GetLocale: function() {
        return "en-US";
      },
      SetLoopPref: setLoopPrefStub,
      GetLoopPref: function(prefName) {
        switch (prefName) {
          case "debug.sdk":
          case "debug.dispatcher":
            return false;
          default:
            return "http://fake";
        }
      },
      GetAllConstants: function() {
        return {
          LOOP_SESSION_TYPE: {
            GUEST: 1,
            FXA: 2
          },
          LOOP_MAU_TYPE: {
            OPEN_PANEL: 0,
            OPEN_CONVERSATION: 1,
            ROOM_OPEN: 2,
            ROOM_SHARE: 3,
            ROOM_DELETE: 4
          }
        };
      },
      EnsureRegistered: sinon.stub(),
      GetAppVersionInfo: function() {
        return {
          version: "42",
          channel: "test",
          platform: "test"
        };
      },
      GetAudioBlob: sinon.spy(function() {
        return new Blob([new ArrayBuffer(10)], { type: "audio/ogg" });
      }),
      GetSelectedTabMetadata: function() {
        return {};
      },
      GetConversationWindowData: function() {
        return {};
      },
      TelemetryAddValue: sinon.stub()
    });

    fakeWindow = {
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
    document.mozL10n.initialize({
      getStrings: function() { return JSON.stringify({ textContent: "fakeText" }); },
      locale: "en_US"
    });

    dispatcher = new loop.Dispatcher();

    remoteCursorStore = new loop.store.RemoteCursorStore(dispatcher, {
      sdkDriver: {}
    });

    loop.store.StoreMixin.register({ remoteCursorStore: remoteCursorStore });
  });

  afterEach(function() {
    loop.shared.mixins.setRootObject(window);
    sandbox.restore();
    LoopMochaUtils.restore();
  });

  describe("#init", function() {
    var OTRestore;
    beforeEach(function() {
      sandbox.stub(React, "render");
      sandbox.stub(document.mozL10n, "initialize");

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
      sinon.assert.calledWith(document.mozL10n.initialize, sinon.match({ locale: "en-US" }));
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

    it("should log a telemetry event when opening the conversation window", function() {
      var constants = requestStubs.GetAllConstants();
      loop.conversation.init();

      sinon.assert.calledOnce(requestStubs["TelemetryAddValue"]);
      sinon.assert.calledWithExactly(requestStubs["TelemetryAddValue"],
        "LOOP_ACTIVITY_COUNTER", constants.LOOP_MAU_TYPE.OPEN_CONVERSATION);
    });
  });

  describe("AppControllerView", function() {
    var activeRoomStore,
        ccView,
        addRemoteCursorStub,
        clickRemoteCursorStub;
    var conversationAppStore,
        roomStore;
    var ROOM_STATES = loop.store.ROOM_STATES;

    function mountTestComponent() {
      return TestUtils.renderIntoDocument(
        React.createElement(loop.conversation.AppControllerView, {
          cursorStore: remoteCursorStore,
          dispatcher: dispatcher,
          roomStore: roomStore
        }));
    }

    beforeEach(function() {
      activeRoomStore = new loop.store.ActiveRoomStore(dispatcher, {
        mozLoop: {},
        sdkDriver: {}
      });
      roomStore = new loop.store.RoomStore(dispatcher, {
        activeRoomStore: activeRoomStore,
        constants: {}
      });
      remoteCursorStore = new loop.store.RemoteCursorStore(dispatcher, {
        sdkDriver: {}
      });
      conversationAppStore = new loop.store.ConversationAppStore({
        activeRoomStore: activeRoomStore,
        dispatcher: dispatcher,
        feedbackPeriod: 42,
        feedbackTimestamp: 42,
        facebookEnabled: false
      });

      loop.store.StoreMixin.register({
        conversationAppStore: conversationAppStore
      });

      addRemoteCursorStub = sandbox.stub();
      clickRemoteCursorStub = sandbox.stub();
      LoopMochaUtils.stubLoopRequest({
        AddRemoteCursorOverlay: addRemoteCursorStub,
        ClickRemoteCursor: clickRemoteCursorStub
      });

      loop.config = {
        tilesIframeUrl: null,
        tilesSupportUrl: null
      };

      sinon.stub(dispatcher, "dispatch");
    });

    afterEach(function() {
      ccView = undefined;
    });

    it("should request AddRemoteCursorOverlay when cursor position changes", function() {

      mountTestComponent();
      remoteCursorStore.setStoreState({
        "remoteCursorPosition": {
          "ratioX": 10,
          "ratioY": 10
        }
      });

      sinon.assert.calledOnce(addRemoteCursorStub);
    });

    it("should NOT request AddRemoteCursorOverlay when cursor position DOES NOT changes", function() {

      mountTestComponent();
      remoteCursorStore.setStoreState({
        "realVideoSize": {
          "height": 400,
          "width": 600
        }
      });

      sinon.assert.notCalled(addRemoteCursorStub);
    });

    it("should request ClickRemoteCursor when click event detected", function() {

      mountTestComponent();
      remoteCursorStore.setStoreState({
        "remoteCursorClick": true
      });

      sinon.assert.calledOnce(clickRemoteCursorStub);
    });

    it("should NOT request ClickRemoteCursor when reset click on store", function() {

      mountTestComponent();
      remoteCursorStore.setStoreState({
        "remoteCursorClick": false
      });

      sinon.assert.notCalled(clickRemoteCursorStub);
    });

    it("should display the RoomView for rooms", function() {
      conversationAppStore.setStoreState({ windowType: "room" });
      activeRoomStore.setStoreState({ roomState: ROOM_STATES.READY });

      ccView = mountTestComponent();

      var desktopRoom = TestUtils.findRenderedComponentWithType(ccView,
        loop.roomViews.DesktopRoomConversationView);

      expect(desktopRoom.props.facebookEnabled).to.eql(false);
    });

    it("should pass the correct value of facebookEnabled to DesktopRoomConversationView",
      function() {
        conversationAppStore.setStoreState({
          windowType: "room",
          facebookEnabled: true
        });
        activeRoomStore.setStoreState({ roomState: ROOM_STATES.READY });

        ccView = mountTestComponent();

        var desktopRoom = TestUtils.findRenderedComponentWithType(ccView,
            loop.roomViews.DesktopRoomConversationView);

        expect(desktopRoom.props.facebookEnabled).to.eql(true);
      });

    it("should display the RoomFailureView for failures", function() {
      conversationAppStore.setStoreState({
        outgoing: false,
        windowType: "failed"
      });

      ccView = mountTestComponent();

      TestUtils.findRenderedComponentWithType(ccView,
        loop.roomViews.RoomFailureView);
    });

    it("should set the correct title when rendering feedback view", function() {
      conversationAppStore.setStoreState({ showFeedbackForm: true });

      ccView = mountTestComponent();

      sinon.assert.calledWithExactly(mozL10nGet, "conversation_has_ended");
    });

    it("should render FeedbackView if showFeedbackForm state is true",
       function() {
         conversationAppStore.setStoreState({ showFeedbackForm: true });

         ccView = mountTestComponent();

         TestUtils.findRenderedComponentWithType(ccView, FeedbackView);
       });

    it("should dispatch LeaveConversation when handleCallTerminated is called", function() {
      ccView = mountTestComponent();

      ccView.handleCallTerminated();

      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWithExactly(dispatcher.dispatch,
        new sharedActions.LeaveConversation());
    });
  });
});
