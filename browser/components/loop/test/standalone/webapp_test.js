/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

describe("loop.webapp", function() {
  "use strict";

  var expect = chai.expect;
  var TestUtils = React.addons.TestUtils;
  var sharedActions = loop.shared.actions;
  var sharedUtils = loop.shared.utils,
      sandbox,
      dispatcher,
      mozL10nGet;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    dispatcher = new loop.Dispatcher();

    mozL10nGet = sandbox.stub(navigator.mozL10n, "get", function(x) {
      return "translated:" + x;
    });
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("#init", function() {
    var loopConfigRestore;

    beforeEach(function() {
      sandbox.stub(React, "render");
      loopConfigRestore = loop.config;
      loop.config = {
        feedbackApiUrl: "http://fake.invalid",
        serverUrl: "http://fake.invalid"
      };
      sandbox.stub(loop.Dispatcher.prototype, "dispatch");
    });

    afterEach(function() {
      loop.config = loopConfigRestore;
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
        sandbox.stub(sharedUtils, "locationData").returns({
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

  describe("WebappRootView", function() {
    var sdk, conversationModel, client, props, standaloneAppStore;
    var activeRoomStore;

    function mountTestComponent() {
      return TestUtils.renderIntoDocument(
        React.createElement(
          loop.webapp.WebappRootView, {
            activeRoomStore: activeRoomStore,
            dispatcher: dispatcher,
            standaloneAppStore: standaloneAppStore
          }));
    }

    beforeEach(function() {
      sdk = {
        checkSystemRequirements: function() { return true; }
      };
      activeRoomStore = new loop.store.ActiveRoomStore(dispatcher, {
        mozLoop: {},
        sdkDriver: {}
      });
      standaloneAppStore = new loop.store.StandaloneAppStore({
        dispatcher: dispatcher,
        sdk: sdk
      });
    });

    it("should display the UnsupportedDeviceView for `unsupportedDevice` window type",
      function() {
        standaloneAppStore.setStoreState({windowType: "unsupportedDevice", unsupportedPlatform: "ios"});
        var webappRootView = mountTestComponent();

        TestUtils.findRenderedComponentWithType(webappRootView,
          loop.webapp.UnsupportedDeviceView);
      });

    it("should display the UnsupportedBrowserView for `unsupportedBrowser` window type",
      function() {
        standaloneAppStore.setStoreState({windowType: "unsupportedBrowser", isFirefox: false});

        var webappRootView = mountTestComponent();

        TestUtils.findRenderedComponentWithType(webappRootView,
          loop.webapp.UnsupportedBrowserView);
      });

    it("should display the StandaloneRoomControllerView for `room` window type",
      function() {
        standaloneAppStore.setStoreState({windowType: "room", isFirefox: true});

        var webappRootView = mountTestComponent();

        TestUtils.findRenderedComponentWithType(webappRootView,
          loop.standaloneRoomViews.StandaloneRoomControllerView);
      });

    it("should display the HomeView for `home` window type", function() {
        standaloneAppStore.setStoreState({windowType: "home", isFirefox: true});

        var webappRootView = mountTestComponent();

        TestUtils.findRenderedComponentWithType(webappRootView,
          loop.webapp.HomeView);
    });
  });

  describe("HomeView", function() {
    it("should display a welcome", function() {
      var homeView = TestUtils.renderIntoDocument(
        React.createElement(loop.webapp.HomeView));

      expect(homeView.getDOMNode().textContent.includes("welcome")).eql(true);
    });
  });

  describe("PromoteFirefoxView", function() {
    describe("#render", function() {
      it("should not render when using Firefox", function() {
        var comp = TestUtils.renderIntoDocument(
          React.createElement(loop.webapp.PromoteFirefoxView, {
            isFirefox: true
        }));

        expect(comp.getDOMNode()).eql(null);
      });

      it("should render when not using Firefox", function() {
        var comp = TestUtils.renderIntoDocument(
          React.createElement(loop.webapp.PromoteFirefoxView, {
              isFirefox: false
        }));

        sinon.assert.calledWith(mozL10nGet, "promote_firefox_hello_heading");
        sinon.assert.calledWith(mozL10nGet, "get_firefox_button");
      });
    });
  });
});
