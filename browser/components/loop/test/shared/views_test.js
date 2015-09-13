/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

describe("loop.shared.views", function() {
  "use strict";

  var expect = chai.expect;
  var l10n = navigator.mozL10n || document.mozL10n;
  var TestUtils = React.addons.TestUtils;
  var sharedActions = loop.shared.actions;
  var sharedModels = loop.shared.models;
  var sharedViews = loop.shared.views;
  var SCREEN_SHARE_STATES = loop.shared.utils.SCREEN_SHARE_STATES;
  var getReactElementByClass = TestUtils.findRenderedDOMComponentWithClass;
  var sandbox, fakeAudioXHR, dispatcher, OS, OSVersion;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    sandbox.useFakeTimers(); // exposes sandbox.clock as a fake timer
    sandbox.stub(l10n, "get", function(x) {
      return "translated:" + x;
    });

    dispatcher = new loop.Dispatcher();
    sandbox.stub(dispatcher, "dispatch");

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

    OS = "mac";
    OSVersion = { major: 10, minor: 10 };
    sandbox.stub(loop.shared.utils, "getOS", function() {
      return OS;
    });
    sandbox.stub(loop.shared.utils, "getOSVersion", function() {
      return OSVersion;
    });
  });

  afterEach(function() {
    loop.store.StoreMixin.clearRegisteredStores();
    sandbox.restore();
  });

  describe("MediaControlButton", function() {
    it("should render an enabled local audio button", function() {
      var comp = TestUtils.renderIntoDocument(
        React.createElement(sharedViews.MediaControlButton, {
          scope: "local",
          type: "audio",
          action: function(){},
          enabled: true
        }));

      expect(comp.getDOMNode().classList.contains("muted")).eql(false);
    });

    it("should render a muted local audio button", function() {
      var comp = TestUtils.renderIntoDocument(
          React.createElement(sharedViews.MediaControlButton, {
          scope: "local",
          type: "audio",
          action: function(){},
          enabled: false
        }));

      expect(comp.getDOMNode().classList.contains("muted")).eql(true);
    });

    it("should render an enabled local video button", function() {
      var comp = TestUtils.renderIntoDocument(
          React.createElement(sharedViews.MediaControlButton, {
          scope: "local",
          type: "video",
          action: function(){},
          enabled: true
        }));

      expect(comp.getDOMNode().classList.contains("muted")).eql(false);
    });

    it("should render a muted local video button", function() {
      var comp = TestUtils.renderIntoDocument(
        React.createElement(sharedViews.MediaControlButton, {
          scope: "local",
          type: "video",
          action: function(){},
          enabled: false
        }));

      expect(comp.getDOMNode().classList.contains("muted")).eql(true);
    });
  });

  describe("ScreenShareControlButton", function() {
    it("should render a visible share button", function() {
      var comp = TestUtils.renderIntoDocument(
        React.createElement(sharedViews.ScreenShareControlButton, {
          dispatcher: dispatcher,
          visible: true,
          state: SCREEN_SHARE_STATES.INACTIVE
        }));

      expect(comp.getDOMNode().classList.contains("active")).eql(false);
      expect(comp.getDOMNode().classList.contains("disabled")).eql(false);
    });

    it("should render a disabled share button when share is pending", function() {
      var comp = TestUtils.renderIntoDocument(
        React.createElement(sharedViews.ScreenShareControlButton, {
          dispatcher: dispatcher,
          visible: true,
          state: SCREEN_SHARE_STATES.PENDING
        }));

      var node = comp.getDOMNode().querySelector(".btn-screen-share");
      expect(node.classList.contains("active")).eql(false);
      expect(node.classList.contains("disabled")).eql(true);
    });

    it("should render an active share button", function() {
      var comp = TestUtils.renderIntoDocument(
        React.createElement(sharedViews.ScreenShareControlButton, {
          dispatcher: dispatcher,
          visible: true,
          state: SCREEN_SHARE_STATES.ACTIVE
        }));

      var node = comp.getDOMNode().querySelector(".btn-screen-share");
      expect(node.classList.contains("active")).eql(true);
      expect(node.classList.contains("disabled")).eql(false);
    });

    it("should show the screenshare dropdown on click when the state is not active",
       function() {
        var comp = TestUtils.renderIntoDocument(
          React.createElement(sharedViews.ScreenShareControlButton, {
            dispatcher: dispatcher,
            visible: true,
            state: SCREEN_SHARE_STATES.INACTIVE
          }));

        expect(comp.state.showMenu).eql(false);

        TestUtils.Simulate.click(comp.getDOMNode().querySelector(".btn-screen-share"));

        expect(comp.state.showMenu).eql(true);
      });

    it("should dispatch a 'browser' StartScreenShare action on option click",
      function() {
        var comp = TestUtils.renderIntoDocument(
          React.createElement(sharedViews.ScreenShareControlButton, {
            dispatcher: dispatcher,
            visible: true,
            state: SCREEN_SHARE_STATES.INACTIVE
          }));

        TestUtils.Simulate.click(comp.getDOMNode().querySelector(
          ".conversation-window-dropdown > li"));

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.StartScreenShare({ type: "browser" }));
      });

    it("should dispatch a 'window' StartScreenShare action on option click",
      function() {
        var comp = TestUtils.renderIntoDocument(
          React.createElement(sharedViews.ScreenShareControlButton, {
            dispatcher: dispatcher,
            visible: true,
            state: SCREEN_SHARE_STATES.INACTIVE
          }));

        TestUtils.Simulate.click(comp.getDOMNode().querySelector(
          ".conversation-window-dropdown > li:last-child"));

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.StartScreenShare({ type: "window" }));
      });

    it("should have the 'window' option enabled", function() {
      var comp = TestUtils.renderIntoDocument(
        React.createElement(sharedViews.ScreenShareControlButton, {
          dispatcher: dispatcher,
          visible: true,
          state: SCREEN_SHARE_STATES.INACTIVE
        }));

      var node = comp.getDOMNode().querySelector(".conversation-window-dropdown > li:last-child");
      expect(node.classList.contains("disabled")).eql(false);
    });

    it("should disable the 'window' option on Windows XP", function() {
      OS = "win";
      OSVersion = { major: 5, minor: 1 };

      var comp = TestUtils.renderIntoDocument(
        React.createElement(sharedViews.ScreenShareControlButton, {
          dispatcher: dispatcher,
          visible: true,
          state: SCREEN_SHARE_STATES.INACTIVE
        }));

      var node = comp.getDOMNode().querySelector(".conversation-window-dropdown > li:last-child");
      expect(node.classList.contains("disabled")).eql(true);
    });

    it("should disable the 'window' option on OSX 10.6", function() {
      OS = "mac";
      OSVersion = { major: 10, minor: 6 };

      var comp = TestUtils.renderIntoDocument(
        React.createElement(sharedViews.ScreenShareControlButton, {
          dispatcher: dispatcher,
          visible: true,
          state: SCREEN_SHARE_STATES.INACTIVE
        }));

      var node = comp.getDOMNode().querySelector(".conversation-window-dropdown > li:last-child");
      expect(node.classList.contains("disabled")).eql(true);
    });

    it("should dispatch a EndScreenShare action on click when the state is active",
      function() {
        var comp = TestUtils.renderIntoDocument(
          React.createElement(sharedViews.ScreenShareControlButton, {
            dispatcher: dispatcher,
            visible: true,
            state: SCREEN_SHARE_STATES.ACTIVE
          }));

        TestUtils.Simulate.click(comp.getDOMNode().querySelector(".btn-screen-share"));

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.EndScreenShare({}));
      });
  });

  describe("SettingsControlButton", function() {
    var fakeMozLoop;
    var support_url = "https://support.com";
    var feedback_url = "https://feedback.com";

    beforeEach(function() {
      fakeMozLoop = {
        openURL: sandbox.stub(),
        setLoopPref: sandbox.stub(),
        getLoopPref: function (prefName) {
          switch (prefName) {
            case "support_url":
              return support_url;
            case "feedback.formURL":
              return feedback_url;
            default:
              return prefName;
          }
        }
      };
    });

    function mountTestComponent(props) {
      props = _.extend({
        mozLoop: fakeMozLoop
      }, props);

      return TestUtils.renderIntoDocument(
        React.createElement(sharedViews.SettingsControlButton, props));
    }

    it("should render a visible button", function() {
      var settingsMenuItems = [{ id: "feedback" }];
      var comp = mountTestComponent({ menuItems: settingsMenuItems} );

      var node = comp.getDOMNode().querySelector(".btn-settings");
      expect(node.classList.contains("hide")).eql(false);
    });

    it("should not render anything", function() {
      var comp = mountTestComponent();
      expect(comp.getDOMNode()).to.eql(null);
    });

    it("should not show an indefined menu option", function() {
      var settingsMenuItems = [
        { id: "not Defined" },
        { id: "help" }
      ];
      var comp = mountTestComponent({ menuItems: settingsMenuItems} );
      var menuItems = comp.getDOMNode().querySelectorAll(".settings-menu > li");
      expect(menuItems).to.have.length.of(1);
    });

    it("should not render anythin if not exists any valid item to show", function() {
      var settingsMenuItems = [
        { id: "not Defined" },
        { id: "another wrong menu item" }
      ];
      var comp = mountTestComponent({ menuItems: settingsMenuItems} );
      expect(comp.getDOMNode()).to.eql(null);
    });

    it("should show the settings dropdown on click", function() {
      var settingsMenuItems = [{ id: "feedback" }];
      var comp = mountTestComponent({ menuItems: settingsMenuItems} );

      expect(comp.state.showMenu).eql(false);
      TestUtils.Simulate.click(comp.getDOMNode().querySelector(".btn-settings"));

      expect(comp.state.showMenu).eql(true);
    });

    it("should have a `menu-below` class on the dropdown when the prop is set.", function() {
      var settingsMenuItems = [
        { id: "help" }
      ];
      var comp = mountTestComponent({
        menuBelow: true,
        menuItems: settingsMenuItems
      });
      var menuItems = comp.getDOMNode().querySelector(".settings-menu");

      expect(menuItems.classList.contains("menu-below")).eql(true);
    });

    it("should not have a `menu-below` class on the dropdown when the prop is not set.", function() {
      var settingsMenuItems = [
        { id: "help" }
      ];
      var comp = mountTestComponent({
        menuItems: settingsMenuItems
      });
      var menuItems = comp.getDOMNode().querySelector(".settings-menu");

      expect(menuItems.classList.contains("menu-below")).eql(false);
    });

    it("should show edit Context on menu when the option is enabled", function() {
      var settingsMenuItems = [
        {
          id: "edit",
          enabled: true,
          visible: true,
          onClick: function() {}
        }
      ];
      var comp = mountTestComponent({ menuItems: settingsMenuItems} );

      var node = comp.getDOMNode().querySelector(".settings-menu > li.entry-settings-edit");
      expect(node.classList.contains("hide")).eql(false);
    });

    it("should hide edit Context on menu when the option is not visible", function() {
      var settingsMenuItems = [
        {
          id: "edit",
          enabled: false,
          visible: false,
          onClick: function() {}
        }
      ];
      var comp = mountTestComponent({ menuItems: settingsMenuItems} );

      var node = comp.getDOMNode().querySelector(".settings-menu > li.entry-settings-edit");
      expect(node.classList.contains("hide")).eql(true);
    });

    it("should call onClick method when the edit context menu item is clicked", function() {
      var onClickCalled = false;
      var settingsMenuItems = [
        {
          id: "edit",
          enabled: true,
          visible: true,
          onClick: sandbox.stub()
        }
      ];
      var comp = mountTestComponent({ menuItems: settingsMenuItems} );

      TestUtils.Simulate.click(comp.getDOMNode().querySelector(".settings-menu > li.entry-settings-edit"));
      sinon.assert.calledOnce(settingsMenuItems[0].onClick);
    });

    it("should open a tab to the feedback url when the feedback menu item is clicked", function() {
      var settingsMenuItems = [
        { id: "feedback" },
        { id: "help" }
      ];
      var comp = mountTestComponent({ menuItems: settingsMenuItems} );

      TestUtils.Simulate.click(comp.getDOMNode().querySelector(".settings-menu > li:first-child"));

      sinon.assert.calledOnce(fakeMozLoop.openURL);
      sinon.assert.calledWithExactly(fakeMozLoop.openURL, feedback_url);
    });

    it("should open a tab to the support url when the support menu item is clicked", function() {
      var settingsMenuItems = [
        { id: "feedback" },
        { id: "help" }
      ];
      var comp = mountTestComponent({ menuItems: settingsMenuItems} );

      TestUtils.Simulate.click(comp.getDOMNode().querySelector(".settings-menu > li:last-child"));

      sinon.assert.calledOnce(fakeMozLoop.openURL);
      sinon.assert.calledWithExactly(fakeMozLoop.openURL, support_url);
    });
  });

  describe("ConversationToolbar", function() {
    var clock, hangup, publishStream;

    function mountTestComponent(props) {
      props = _.extend({
        dispatcher: dispatcher,
        mozLoop: {}
      }, props || {});
      return TestUtils.renderIntoDocument(
        React.createElement(sharedViews.ConversationToolbar, props));
    }

    beforeEach(function() {
      hangup = sandbox.stub();
      publishStream = sandbox.stub();
      clock = sinon.useFakeTimers();
    });

    afterEach(function() {
      clock.restore();
    });

    it("should start no idle", function() {
      var comp = mountTestComponent({
        hangupButtonLabel: "foo",
        hangup: hangup,
        publishStream: publishStream
      });
      expect(comp.getDOMNode().classList.contains("idle")).eql(false);
    });

    it("should be on idle state after 6 seconds", function() {
      var comp = mountTestComponent({
        hangupButtonLabel: "foo",
        hangup: hangup,
        publishStream: publishStream
      });
      expect(comp.getDOMNode().classList.contains("idle")).eql(false);

      clock.tick(6001);
      expect(comp.getDOMNode().classList.contains("idle")).eql(true);
    });

    it("should remove idle state when the user moves the mouse", function() {
      var comp = mountTestComponent({
        hangupButtonLabel: "foo",
        hangup: hangup,
        publishStream: publishStream
      });

      clock.tick(6001);
      expect(comp.getDOMNode().classList.contains("idle")).eql(true);

      document.body.dispatchEvent(new CustomEvent("mousemove"));

      expect(comp.getDOMNode().classList.contains("idle")).eql(false);
    });

    it("should accept a hangupButtonLabel optional prop", function() {
      var comp = mountTestComponent({
        hangupButtonLabel: "foo",
        hangup: hangup,
        publishStream: publishStream
      });

      expect(comp.getDOMNode().querySelector("button.btn-hangup").textContent)
            .eql("foo");
    });

    it("should accept a enableHangup optional prop", function() {
      var comp = mountTestComponent({
        enableHangup: false,
        hangup: hangup,
        publishStream: publishStream
      });

      expect(comp.getDOMNode().querySelector("button.btn-hangup").disabled)
            .eql(true);
    });

    it("should hangup when hangup button is clicked", function() {
      var comp = mountTestComponent({
        hangup: hangup,
        publishStream: publishStream,
        audio: {enabled: true}
      });

      TestUtils.Simulate.click(
        comp.getDOMNode().querySelector(".btn-hangup"));

      sinon.assert.calledOnce(hangup);
      sinon.assert.calledWithExactly(hangup);
    });

    it("should unpublish audio when audio mute btn is clicked", function() {
      var comp = mountTestComponent({
        hangup: hangup,
        publishStream: publishStream,
        audio: {enabled: true}
      });

      TestUtils.Simulate.click(
        comp.getDOMNode().querySelector(".btn-mute-audio"));

      sinon.assert.calledOnce(publishStream);
      sinon.assert.calledWithExactly(publishStream, "audio", false);
    });

    it("should publish audio when audio mute btn is clicked", function() {
      var comp = mountTestComponent({
        hangup: hangup,
        publishStream: publishStream,
        audio: {enabled: false}
      });

      TestUtils.Simulate.click(
        comp.getDOMNode().querySelector(".btn-mute-audio"));

      sinon.assert.calledOnce(publishStream);
      sinon.assert.calledWithExactly(publishStream, "audio", true);
    });

    it("should unpublish video when video mute btn is clicked", function() {
      var comp = mountTestComponent({
        hangup: hangup,
        publishStream: publishStream,
        video: {enabled: true}
      });

      TestUtils.Simulate.click(
        comp.getDOMNode().querySelector(".btn-mute-video"));

      sinon.assert.calledOnce(publishStream);
      sinon.assert.calledWithExactly(publishStream, "video", false);
    });

    it("should publish video when video mute btn is clicked", function() {
      var comp = mountTestComponent({
        hangup: hangup,
        publishStream: publishStream,
        video: {enabled: false}
      });

      TestUtils.Simulate.click(
        comp.getDOMNode().querySelector(".btn-mute-video"));

      sinon.assert.calledOnce(publishStream);
      sinon.assert.calledWithExactly(publishStream, "video", true);
    });
  });

  describe("ConversationView", function() {
    var fakeSDK, fakeSessionData, fakeSession, fakePublisher, model, fakeAudio;

    function mountTestComponent(props) {
      props = _.extend({
        dispatcher: dispatcher,
        mozLoop: {}
      }, props || {});
      return TestUtils.renderIntoDocument(
        React.createElement(sharedViews.ConversationView, props));
    }

    beforeEach(function() {
      fakeAudio = {
        play: sinon.spy(),
        pause: sinon.spy(),
        removeAttribute: sinon.spy()
      };
      sandbox.stub(window, "Audio").returns(fakeAudio);

      fakeSessionData = {
        sessionId: "sessionId",
        sessionToken: "sessionToken",
        apiKey: "apiKey"
      };
      fakeSession = _.extend({
        connection: {connectionId: 42},
        connect: sandbox.spy(),
        disconnect: sandbox.spy(),
        publish: sandbox.spy(),
        unpublish: sandbox.spy(),
        subscribe: sandbox.spy()
      }, Backbone.Events);
      fakePublisher = _.extend({
        publishAudio: sandbox.spy(),
        publishVideo: sandbox.spy()
      }, Backbone.Events);
      fakeSDK = {
        initPublisher: sandbox.stub().returns(fakePublisher),
        initSession: sandbox.stub().returns(fakeSession),
        on: sandbox.stub()
      };
      model = new sharedModels.ConversationModel(fakeSessionData, {
        sdk: fakeSDK
      });
    });

    describe("#componentDidMount", function() {
      it("should start a session by default", function() {
        sandbox.stub(model, "startSession");

        mountTestComponent({
          sdk: fakeSDK,
          model: model,
          video: {enabled: true}
        });

        sinon.assert.calledOnce(model.startSession);
      });

      // Test loop.shared.utils.findParentNode.
      // Added here to take advantage of having markup.
      it("should find '.video-layout-wrapper'", function() {
        var view = mountTestComponent({
          initiate: false,
          sdk: fakeSDK,
          model: model,
          video: {enabled: true}
        });
        var menu = view.getDOMNode().querySelector(".btn-hangup-entry");

        var result = loop.shared.utils.findParentNode(menu,
                                                      "video-layout-wrapper");

        expect(result.classList.contains("video-layout-wrapper")).to.eql(true);
      });

      it("shouldn't start a session if initiate is false", function() {
        sandbox.stub(model, "startSession");

        mountTestComponent({
          initiate: false,
          sdk: fakeSDK,
          model: model,
          video: {enabled: true}
        });

        sinon.assert.notCalled(model.startSession);
      });
    });

    describe("constructed", function() {
      var comp;

      beforeEach(function() {
        comp = mountTestComponent({
          sdk: fakeSDK,
          model: model,
          video: {enabled: false}
        });
      });

      describe("#hangup", function() {
        beforeEach(function() {
          comp.startPublishing();
        });

        it("should disconnect the session", function() {
          sandbox.stub(model, "endSession");

          comp.hangup();

          sinon.assert.calledOnce(model.endSession);
        });

        it("should stop publishing local streams", function() {
          comp.hangup();

          sinon.assert.calledOnce(fakeSession.unpublish);
        });
      });

      describe("#startPublishing", function() {
        it("should publish local stream", function() {
          comp.startPublishing();

          sinon.assert.calledOnce(fakeSDK.initPublisher);
          sinon.assert.calledOnce(fakeSession.publish);
        });

        // XXX This test would need reworking, but the code should be going
        // away after the obsolences of call urls (currently bug 1170150).
        it("should start listening to OT publisher accessDialogOpened and " +
          " accessDenied events");
      });

      describe("#stopPublishing", function() {
        beforeEach(function() {
          sandbox.stub(fakePublisher, "off");
          comp.startPublishing();
        });

        it("should stop publish local stream", function() {
          comp.stopPublishing();

          sinon.assert.calledOnce(fakeSession.unpublish);
        });

        it("should unsubscribe from publisher events",
          function() {
            comp.stopPublishing();

            // Note: Backbone.Events#stopListening calls off() on passed object.
            sinon.assert.calledOnce(fakePublisher.off);
          });
      });

      describe("#publishStream", function() {
        var component;

        beforeEach(function() {
          component = mountTestComponent({
            sdk: fakeSDK,
            model: model,
            video: {enabled: false}
          });
          component.startPublishing();
        });

        it("should start streaming local audio", function() {
          component.publishStream("audio", true);

          sinon.assert.calledOnce(fakePublisher.publishAudio);
          sinon.assert.calledWithExactly(fakePublisher.publishAudio, true);
        });

        it("should stop streaming local audio", function() {
          component.publishStream("audio", false);

          sinon.assert.calledOnce(fakePublisher.publishAudio);
          sinon.assert.calledWithExactly(fakePublisher.publishAudio, false);
        });

        it("should start streaming local video", function() {
          component.publishStream("video", true);

          sinon.assert.calledOnce(fakePublisher.publishVideo);
          sinon.assert.calledWithExactly(fakePublisher.publishVideo, true);
        });

        it("should stop streaming local video", function() {
          component.publishStream("video", false);

          sinon.assert.calledOnce(fakePublisher.publishVideo);
          sinon.assert.calledWithExactly(fakePublisher.publishVideo, false);
        });
      });

      describe("Model events", function() {

        describe("for standalone", function() {

          beforeEach(function() {
            // In standalone, navigator.mozLoop does not exists
            if (navigator.hasOwnProperty("mozLoop")) {
              sandbox.stub(navigator, "mozLoop", undefined);
            }
          });

          it("should play a connected sound, once, on session:connected",
             function() {
               var url = "shared/sounds/connected.ogg";
               sandbox.stub(window, "XMLHttpRequest").returns(fakeAudioXHR);
               model.trigger("session:connected");

               fakeAudioXHR.onload();

               sinon.assert.called(fakeAudioXHR.open);
               sinon.assert.calledWithExactly(fakeAudioXHR.open, "GET", url, true);

               sinon.assert.calledOnce(fakeAudio.play);
               expect(fakeAudio.loop).to.not.equal(true);
             });
        });

        describe("for desktop", function() {
          var origMozLoop;

          beforeEach(function() {
            origMozLoop = navigator.mozLoop;
            navigator.mozLoop = {
              getAudioBlob: sinon.spy(function(name, callback) {
                var data = new ArrayBuffer(10);
                callback(null, new Blob([data], {type: "audio/ogg"}));
              })
            };
          });

          afterEach(function() {
            navigator.mozLoop = origMozLoop;
          });

          it("should play a connected sound, once, on session:connected",
             function() {
               var url = "chrome://browser/content/loop/shared/sounds/connected.ogg";
               model.trigger("session:connected");

               sinon.assert.calledOnce(navigator.mozLoop.getAudioBlob);
               sinon.assert.calledWithExactly(navigator.mozLoop.getAudioBlob,
                                              "connected", sinon.match.func);
               sinon.assert.calledOnce(fakeAudio.play);
               expect(fakeAudio.loop).to.not.equal(true);
             });
        });

        describe("for both (standalone and desktop)", function() {
          beforeEach(function() {
            sandbox.stub(window, "XMLHttpRequest").returns(fakeAudioXHR);
          });

          it("should start streaming on session:connected", function() {
            model.trigger("session:connected");

            sinon.assert.calledOnce(fakeSDK.initPublisher);
          });

          it("should publish remote stream on session:stream-created",
             function() {
               var s1 = {connection: {connectionId: 42}};

               model.trigger("session:stream-created", {stream: s1});

               sinon.assert.calledOnce(fakeSession.subscribe);
               sinon.assert.calledWith(fakeSession.subscribe, s1);
             });

          it("should unpublish local stream on session:ended", function() {
            comp.startPublishing();

            model.trigger("session:ended");

            sinon.assert.calledOnce(fakeSession.unpublish);
          });

          it("should unpublish local stream on session:peer-hungup", function() {
            comp.startPublishing();

            model.trigger("session:peer-hungup");

            sinon.assert.calledOnce(fakeSession.unpublish);
          });

          it("should unpublish local stream on session:network-disconnected",
             function() {
               comp.startPublishing();

               model.trigger("session:network-disconnected");

               sinon.assert.calledOnce(fakeSession.unpublish);
             });
        });

      });

      describe("Publisher events", function() {
        beforeEach(function() {
          comp.startPublishing();
        });

        it("should set audio state on streamCreated", function() {
          fakePublisher.trigger("streamCreated", {stream: {hasAudio: true}});
          expect(comp.state.audio.enabled).eql(true);

          fakePublisher.trigger("streamCreated", {stream: {hasAudio: false}});
          expect(comp.state.audio.enabled).eql(false);
        });

        it("should set video state on streamCreated", function() {
          fakePublisher.trigger("streamCreated", {stream: {hasVideo: true}});
          expect(comp.state.video.enabled).eql(true);

          fakePublisher.trigger("streamCreated", {stream: {hasVideo: false}});
          expect(comp.state.video.enabled).eql(false);
        });

        it("should set media state on streamDestroyed", function() {
          fakePublisher.trigger("streamDestroyed");

          expect(comp.state.audio.enabled).eql(false);
          expect(comp.state.video.enabled).eql(false);
        });
      });
    });
  });

  describe("NotificationListView", function() {
    var coll, view, testNotif;

    function mountTestComponent(props) {
      props = _.extend({
        key: 0
      }, props || {});
      return TestUtils.renderIntoDocument(
        React.createElement(sharedViews.NotificationListView, props));
    }

    beforeEach(function() {
      coll = new sharedModels.NotificationCollection();
      view = mountTestComponent({notifications: coll});
      testNotif = {level: "warning", message: "foo"};
      sinon.spy(view, "render");
    });

    afterEach(function() {
      view.render.restore();
    });

    describe("Collection events", function() {
      it("should render when a notification is added to the collection",
        function() {
          coll.add(testNotif);

          sinon.assert.calledOnce(view.render);
        });

      it("should render when a notification is removed from the collection",
        function() {
          coll.add(testNotif);
          coll.remove(testNotif);

          sinon.assert.calledOnce(view.render);
        });

      it("should render when the collection is reset", function() {
        coll.reset();

        sinon.assert.calledOnce(view.render);
      });
    });
  });

  describe("Checkbox", function() {
    var view;

    afterEach(function() {
      view = null;
    });

    function mountTestComponent(props) {
      props = _.extend({ onChange: function() {} }, props);
      return TestUtils.renderIntoDocument(
        React.createElement(sharedViews.Checkbox, props));
    }

    describe("#render", function() {
      it("should render a checkbox with only required props supplied", function() {
        view = mountTestComponent();

        var node = view.getDOMNode();
        expect(node).to.not.eql(null);
        expect(node.classList.contains("checkbox-wrapper")).to.eql(true);
        expect(node.hasAttribute("disabled")).to.eql(false);
        expect(node.childNodes.length).to.eql(1);
      });

      it("should render a label when it's supplied", function() {
        view = mountTestComponent({ label: "Some label" });

        var node = view.getDOMNode();
        expect(node.lastChild.localName).to.eql("div");
        expect(node.lastChild.textContent).to.eql("Some label");
      });

      it("should render the checkbox as disabled when told to", function() {
        view = mountTestComponent({
          disabled: true
        });

        var node = view.getDOMNode();
        expect(node.classList.contains("disabled")).to.eql(true);
        expect(node.hasAttribute("disabled")).to.eql(true);
      });

      it("should render the checkbox as checked when the prop is set", function() {
        view = mountTestComponent({
          checked: true
        });

        var checkbox = view.getDOMNode().querySelector(".checkbox");
        expect(checkbox.classList.contains("checked")).eql(true);
      });

      it("should alter the render state when the props are changed", function() {
        view = mountTestComponent({
          checked: true
        });

        view.setProps({checked: false});

        var checkbox = view.getDOMNode().querySelector(".checkbox");
        expect(checkbox.classList.contains("checked")).eql(false);
      });

      it("should add an ellipsis class when the prop is set", function() {
        view = mountTestComponent({
          label: "Some label",
          useEllipsis: true
        });

        var label = view.getDOMNode().querySelector(".checkbox-label");
        expect(label.classList.contains("ellipsis")).eql(true);
      });

      it("should not add an ellipsis class when the prop is not set", function() {
        view = mountTestComponent({
          label: "Some label",
          useEllipsis: false
        });

        var label = view.getDOMNode().querySelector(".checkbox-label");
        expect(label.classList.contains("ellipsis")).eql(false);
      });
    });

    describe("#_handleClick", function() {
      var onChange;

      beforeEach(function() {
        onChange = sinon.stub();
      });

      afterEach(function() {
        onChange = null;
      });

      it("should invoke the `onChange` function on click", function() {
        view = mountTestComponent({ onChange: onChange });

        expect(view.state.checked).to.eql(false);

        var node = view.getDOMNode();
        TestUtils.Simulate.click(node);

        expect(view.state.checked).to.eql(true);
        sinon.assert.calledOnce(onChange);
        sinon.assert.calledWithExactly(onChange, {
          checked: true,
          value: ""
        });
      });

      it("should signal a value change on click", function() {
        view = mountTestComponent({
          onChange: onChange,
          value: "some-value"
        });

        expect(view.state.value).to.eql("");

        var node = view.getDOMNode();
        TestUtils.Simulate.click(node);

        expect(view.state.value).to.eql("some-value");
        sinon.assert.calledOnce(onChange);
        sinon.assert.calledWithExactly(onChange, {
          checked: true,
          value: "some-value"
        });
      });
    });
  });

  describe("ContextUrlView", function() {
    var view;

    function mountTestComponent(extraProps) {
      var props = _.extend({
        allowClick: false,
        description: "test",
        dispatcher: dispatcher,
        showContextTitle: false,
        useDesktopPaths: false
      }, extraProps);
      return TestUtils.renderIntoDocument(
        React.createElement(sharedViews.ContextUrlView, props));
    }

    it("should set a clicks-allowed class if clicks are allowed", function() {
      view = mountTestComponent({
        allowClick: true,
        url: "http://wonderful.invalid"
      });

      var wrapper = view.getDOMNode().querySelector(".context-wrapper");

      expect(wrapper.classList.contains("clicks-allowed")).eql(true);
    });

    it("should not set a clicks-allowed class if clicks are not allowed", function() {
      view = mountTestComponent({
        allowClick: false,
        url: "http://wonderful.invalid"
      });

      var wrapper = view.getDOMNode().querySelector(".context-wrapper");

      expect(wrapper.classList.contains("clicks-allowed")).eql(false);
    });

    it("should display nothing if the url is invalid", function() {
      view = mountTestComponent({
        url: "fjrTykyw"
      });

      expect(view.getDOMNode()).eql(null);
    });

    it("should use a default thumbnail if one is not supplied", function() {
      view = mountTestComponent({
        url: "http://wonderful.invalid"
      });

      expect(view.getDOMNode().querySelector(".context-preview").getAttribute("src"))
        .eql("shared/img/icons-16x16.svg#globe");
    });

    it("should use a default thumbnail for desktop if one is not supplied", function() {
      view = mountTestComponent({
        useDesktopPaths: true,
        url: "http://wonderful.invalid"
      });

      expect(view.getDOMNode().querySelector(".context-preview").getAttribute("src"))
        .eql("loop/shared/img/icons-16x16.svg#globe");
    });

    it("should not display a title if by default", function() {
      view = mountTestComponent({
        url: "http://wonderful.invalid"
      });

      expect(view.getDOMNode().querySelector(".context-content > p")).eql(null);
    });

    it("should display a title if required", function() {
      view = mountTestComponent({
        showContextTitle: true,
        url: "http://wonderful.invalid"
      });

      expect(view.getDOMNode().querySelector(".context-content > p")).not.eql(null);
    });

    it("should set the href on the link if clicks are allowed", function() {
      view = mountTestComponent({
        allowClick: true,
        url: "http://wonderful.invalid"
      });

      expect(view.getDOMNode().querySelector(".context-wrapper").href)
        .eql("http://wonderful.invalid/");
    });

    it("should dispatch an action to record link clicks", function() {
      view = mountTestComponent({
        allowClick: true,
        url: "http://wonderful.invalid"
      });

      var linkNode = view.getDOMNode().querySelector(".context-wrapper");

      TestUtils.Simulate.click(linkNode);

      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWith(dispatcher.dispatch,
        new sharedActions.RecordClick({
          linkInfo: "Shared URL"
        }));
    });

    it("should not dispatch an action if clicks are not allowed", function() {
      view = mountTestComponent({
        allowClick: false,
        url: "http://wonderful.invalid"
      });

      var linkNode = view.getDOMNode().querySelector(".context-wrapper");

      TestUtils.Simulate.click(linkNode);

      sinon.assert.notCalled(dispatcher.dispatch);
    });
  });

  describe("MediaView", function() {
    var view;

    function mountTestComponent(props) {
      props = _.extend({
        isLoading: false
      }, props || {});
      return TestUtils.renderIntoDocument(
        React.createElement(sharedViews.MediaView, props));
    }

    it("should display an avatar view", function() {
      view = mountTestComponent({
        displayAvatar: true,
        mediaType: "local"
      });

      TestUtils.findRenderedComponentWithType(view,
        sharedViews.AvatarView);
    });

    it("should display a no-video div if no source object is supplied", function() {
      view = mountTestComponent({
        displayAvatar: false,
        mediaType: "local"
      });

      var element = view.getDOMNode();

      expect(element.className).eql("no-video");
    });

    it("should display a video element if a source object is supplied", function() {
      view = mountTestComponent({
        displayAvatar: false,
        mediaType: "local",
        // This doesn't actually get assigned to the video element, but is enough
        // for this test to check display of the video element.
        srcMediaElement: {
          fake: 1
        }
      });

      var element = view.getDOMNode();

      expect(element).not.eql(null);
      expect(element.className).eql("local-video");
      expect(element.muted).eql(true);
    });

    // We test this function by itself, as otherwise we'd be into creating fake
    // streams etc.
    describe("#attachVideo", function() {
      var fakeViewElement;

      beforeEach(function() {
        fakeViewElement = {
          play: sinon.stub(),
          tagName: "VIDEO"
        };

        view = mountTestComponent({
          displayAvatar: false,
          mediaType: "local",
          srcMediaElement: {
            fake: 1
          }
        });
      });

      it("should not throw if no source object is specified", function() {
        expect(function() {
          view.attachVideo(null);
        }).to.not.Throw();
      });

      it("should not throw if the element is not a video object", function() {
        sinon.stub(view, "getDOMNode").returns({
          tagName: "DIV"
        });

        expect(function() {
          view.attachVideo({});
        }).to.not.Throw();
      });

      it("should attach a video object according to the standard", function() {
        fakeViewElement.srcObject = null;

        sinon.stub(view, "getDOMNode").returns(fakeViewElement);

        view.attachVideo({
          srcObject: {fake: 1}
        });

        expect(fakeViewElement.srcObject).eql({fake: 1});
      });

      it("should attach a video object for Firefox", function() {
        fakeViewElement.mozSrcObject = null;

        sinon.stub(view, "getDOMNode").returns(fakeViewElement);

        view.attachVideo({
          mozSrcObject: {fake: 2}
        });

        expect(fakeViewElement.mozSrcObject).eql({fake: 2});
      });

      it("should attach a video object for Chrome", function() {
        fakeViewElement.src = null;

        sinon.stub(view, "getDOMNode").returns(fakeViewElement);

        view.attachVideo({
          src: {fake: 2}
        });

        expect(fakeViewElement.src).eql({fake: 2});
      });
    });
  });

  describe("MediaLayoutView", function() {
    var textChatStore, view;

    function mountTestComponent(extraProps) {
      var defaultProps = {
        dispatcher: dispatcher,
        displayScreenShare: false,
        isLocalLoading: false,
        isRemoteLoading: false,
        isScreenShareLoading: false,
        localVideoMuted: false,
        matchMedia: window.matchMedia,
        renderRemoteVideo: false,
        showContextRoomName: false,
        useDesktopPaths: false
      };

      return TestUtils.renderIntoDocument(
        React.createElement(sharedViews.MediaLayoutView,
          _.extend(defaultProps, extraProps)));
    }

    beforeEach(function() {
      textChatStore = new loop.store.TextChatStore(dispatcher, {
        sdkDriver: {}
      });

      loop.store.StoreMixin.register({textChatStore: textChatStore});
    });

    it("should mark the remote stream as the focus stream when not displaying screen share", function() {
      view = mountTestComponent({
        displayScreenShare: false
      });

      var node = view.getDOMNode();

      expect(node.querySelector(".remote").classList.contains("focus-stream")).eql(true);
      expect(node.querySelector(".screen").classList.contains("focus-stream")).eql(false);
    });

    it("should mark the screen share stream as the focus stream when displaying screen share", function() {
      view = mountTestComponent({
        displayScreenShare: true
      });

      var node = view.getDOMNode();

      expect(node.querySelector(".remote").classList.contains("focus-stream")).eql(false);
      expect(node.querySelector(".screen").classList.contains("focus-stream")).eql(true);
    });

    it("should not mark the wrapper as receiving screen share when not displaying a screen share", function() {
      view = mountTestComponent({
        displayScreenShare: false
      });

      expect(view.getDOMNode().querySelector(".media-wrapper")
        .classList.contains("receiving-screen-share")).eql(false);
    });

    it("should mark the wrapper as receiving screen share when displaying a screen share", function() {
      view = mountTestComponent({
        displayScreenShare: true
      });

      expect(view.getDOMNode().querySelector(".media-wrapper")
        .classList.contains("receiving-screen-share")).eql(true);
    });

    it("should not mark the wrapper as showing local streams when not displaying a stream", function() {
      view = mountTestComponent({
        localSrcMediaElement: null,
        localPosterUrl: null
      });

      expect(view.getDOMNode().querySelector(".media-wrapper")
        .classList.contains("showing-local-streams")).eql(false);
    });

    it("should mark the wrapper as showing local streams when displaying a stream", function() {
      view = mountTestComponent({
        localSrcMediaElement: {},
        localPosterUrl: null
      });

      expect(view.getDOMNode().querySelector(".media-wrapper")
        .classList.contains("showing-local-streams")).eql(true);
    });

    it("should mark the wrapper as showing local streams when displaying a poster url", function() {
      view = mountTestComponent({
        localSrcMediaElement: {},
        localPosterUrl: "fake/url"
      });

      expect(view.getDOMNode().querySelector(".media-wrapper")
        .classList.contains("showing-local-streams")).eql(true);
    });

    it("should not mark the wrapper as showing remote streams when not displaying a stream", function() {
      view = mountTestComponent({
        remoteSrcMediaElement: null,
        remotePosterUrl: null
      });

      expect(view.getDOMNode().querySelector(".media-wrapper")
        .classList.contains("showing-remote-streams")).eql(false);
    });

    it("should mark the wrapper as showing remote streams when displaying a stream", function() {
      view = mountTestComponent({
        remoteSrcMediaElement: {},
        remotePosterUrl: null
      });

      expect(view.getDOMNode().querySelector(".media-wrapper")
        .classList.contains("showing-remote-streams")).eql(true);
    });

    it("should mark the wrapper as showing remote streams when displaying a poster url", function() {
      view = mountTestComponent({
        remoteSrcMediaElement: {},
        remotePosterUrl: "fake/url"
      });

      expect(view.getDOMNode().querySelector(".media-wrapper")
        .classList.contains("showing-remote-streams")).eql(true);
    });
  });
});
