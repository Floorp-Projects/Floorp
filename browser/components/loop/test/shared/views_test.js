/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*global loop, sinon, React */
/* jshint newcap:false */

var expect = chai.expect;
var l10n = navigator.mozL10n || document.mozL10n;
var TestUtils = React.addons.TestUtils;

describe("loop.shared.views", function() {
  "use strict";

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
        if (header === "Content-Type")
          return "audio/ogg";
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
    $("#fixtures").empty();
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

  describe("ConversationToolbar", function() {
    var hangup, publishStream;

    function mountTestComponent(props) {
      return TestUtils.renderIntoDocument(
        React.createElement(sharedViews.ConversationToolbar, props));
    }

    beforeEach(function() {
      hangup = sandbox.stub();
      publishStream = sandbox.stub();
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
        sessionId:    "sessionId",
        sessionToken: "sessionToken",
        apiKey:       "apiKey"
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
        beforeEach(function() {
          sandbox.stub(fakePublisher, "on");
        });

        it("should publish local stream", function() {
          comp.startPublishing();

          sinon.assert.calledOnce(fakeSDK.initPublisher);
          sinon.assert.calledOnce(fakeSession.publish);
        });

        it("should start listening to OT publisher accessDialogOpened and " +
          " accessDenied events",
          function() {
            comp.startPublishing();

            sinon.assert.called(fakePublisher.on);
            sinon.assert.calledWith(fakePublisher.on,
                                    "accessDialogOpened accessDenied");
          });
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
        var comp;

        beforeEach(function() {
          comp = mountTestComponent({
            sdk: fakeSDK,
            model: model,
            video: {enabled: false}
          });
          comp.startPublishing();
        });

        it("should start streaming local audio", function() {
          comp.publishStream("audio", true);

          sinon.assert.calledOnce(fakePublisher.publishAudio);
          sinon.assert.calledWithExactly(fakePublisher.publishAudio, true);
        });

        it("should stop streaming local audio", function() {
          comp.publishStream("audio", false);

          sinon.assert.calledOnce(fakePublisher.publishAudio);
          sinon.assert.calledWithExactly(fakePublisher.publishAudio, false);
        });

        it("should start streaming local video", function() {
          comp.publishStream("video", true);

          sinon.assert.calledOnce(fakePublisher.publishVideo);
          sinon.assert.calledWithExactly(fakePublisher.publishVideo, true);
        });

        it("should stop streaming local video", function() {
          comp.publishStream("video", false);

          sinon.assert.calledOnce(fakePublisher.publishVideo);
          sinon.assert.calledWithExactly(fakePublisher.publishVideo, false);
        });
      });

      describe("Model events", function() {

        describe("for standalone", function() {

          beforeEach(function() {
            // In standalone, navigator.mozLoop does not exists
            if (navigator.hasOwnProperty("mozLoop"))
              sandbox.stub(navigator, "mozLoop", undefined);
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
        expect(node.lastChild.localName).to.eql("label");
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
});

