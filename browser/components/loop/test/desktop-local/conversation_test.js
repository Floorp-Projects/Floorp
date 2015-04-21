/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop, sinon, React, TestUtils */

var expect = chai.expect;

describe("loop.conversation", function() {
  "use strict";

  var sharedModels = loop.shared.models,
      fakeWindow,
      sandbox;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();

    navigator.mozLoop = {
      doNotDisturb: true,
      getStrings: function() {
        return JSON.stringify({textContent: "fakeText"});
      },
      get locale() {
        return "en-US";
      },
      setLoopPref: sinon.stub(),
      getLoopPref: function(prefName) {
        if (prefName == "debug.sdk") {
          return false;
        }

        return "http://fake";
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
        callback(null, new Blob([new ArrayBuffer(10)], {type: 'audio/ogg'}));
      })
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
    sandbox.stub(document.mozL10n, "get", function(x) {
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

      window.OT = {
        overrideGuidStorage: sinon.stub()
      };
    });

    afterEach(function() {
      delete window.OT;
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
    var conversationStore, client, ccView, dispatcher;
    var conversationAppStore, roomStore;

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

      roomStore = new loop.store.RoomStore(dispatcher, {
        mozLoop: navigator.mozLoop,
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

      ccView = mountTestComponent();

      TestUtils.findRenderedComponentWithType(ccView,
        loop.roomViews.DesktopRoomConversationView);
    });

    it("should display the GenericFailureView for failures", function() {
      conversationAppStore.setStoreState({windowType: "failed"});

      ccView = mountTestComponent();

      TestUtils.findRenderedComponentWithType(ccView,
        loop.conversationViews.GenericFailureView);
    });
  });
});
