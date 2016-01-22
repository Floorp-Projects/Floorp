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
  var sandbox, clock, dispatcher, OS, OSVersion;

  beforeEach(function() {
    sandbox = LoopMochaUtils.createSandbox();
    clock = sandbox.useFakeTimers(); // exposes sandbox.clock as a fake timer
    sandbox.stub(l10n, "get", function(x) {
      return "translated:" + x;
    });

    LoopMochaUtils.stubLoopRequest({
      GetLoopPref: function() {}
    });

    dispatcher = new loop.Dispatcher();
    sandbox.stub(dispatcher, "dispatch");

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
    LoopMochaUtils.restore();
  });

  describe("MediaControlButton", function() {
    it("should render an enabled local audio button", function() {
      var comp = TestUtils.renderIntoDocument(
        React.createElement(sharedViews.MediaControlButton, {
          scope: "local",
          type: "audio",
          action: function() {},
          enabled: true
        }));

      expect(comp.getDOMNode().classList.contains("muted")).eql(false);
    });

    it("should render a muted local audio button", function() {
      var comp = TestUtils.renderIntoDocument(
          React.createElement(sharedViews.MediaControlButton, {
          scope: "local",
          type: "audio",
          action: function() {},
          enabled: false
        }));

      expect(comp.getDOMNode().classList.contains("muted")).eql(true);
    });

    it("should render an enabled local video button", function() {
      var comp = TestUtils.renderIntoDocument(
          React.createElement(sharedViews.MediaControlButton, {
          scope: "local",
          type: "video",
          action: function() {},
          enabled: true
        }));

      expect(comp.getDOMNode().classList.contains("muted")).eql(false);
    });

    it("should render a muted local video button", function() {
      var comp = TestUtils.renderIntoDocument(
        React.createElement(sharedViews.MediaControlButton, {
          scope: "local",
          type: "video",
          action: function() {},
          enabled: false
        }));

      expect(comp.getDOMNode().classList.contains("muted")).eql(true);
    });
  });

  describe("ConversationToolbar", function() {
    var hangup, publishStream;

    function mountTestComponent(props) {
      props = _.extend({
        dispatcher: dispatcher
      }, props || {});
      return TestUtils.renderIntoDocument(
        React.createElement(sharedViews.ConversationToolbar, props));
    }

    beforeEach(function() {
      hangup = sandbox.stub();
      publishStream = sandbox.stub();
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

    it("should accept a showHangup optional prop", function() {
      var comp = mountTestComponent({
        showHangup: false,
        hangup: hangup,
        publishStream: publishStream
      });

      expect(comp.getDOMNode().querySelector(".btn-hangup-entry")).to.eql(null);
    });

    it("should hangup when hangup button is clicked", function() {
      var comp = mountTestComponent({
        hangup: hangup,
        publishStream: publishStream,
        audio: { enabled: true }
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
        audio: { enabled: true }
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
        audio: { enabled: false }
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
        video: { enabled: true }
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
        video: { enabled: false }
      });

      TestUtils.Simulate.click(
        comp.getDOMNode().querySelector(".btn-mute-video"));

      sinon.assert.calledOnce(publishStream);
      sinon.assert.calledWithExactly(publishStream, "video", true);
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
      view = mountTestComponent({ notifications: coll });
      testNotif = { level: "warning", message: "foo" };
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

        view.setProps({ checked: false });

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
        .eql("shared/img/icons-16x16.svg#globe");
    });

    it("should not display a title if by default", function() {
      view = mountTestComponent({
        url: "http://wonderful.invalid"
      });

      expect(view.getDOMNode().querySelector(".context-content > p")).eql(null);
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

    it("should display a video element if a source object is supplied", function(done) {
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

      // Google Chrome doesn't seem to set "muted" on the element at creation
      // time, so we need to do the test as async.
      clock.restore();
      setTimeout(function() {
        try {
          expect(element.muted).eql(true);
          done();
        } catch (ex) {
          done(ex);
        }
      }, 10);
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
          srcObject: { fake: 1 }
        });

        expect(fakeViewElement.srcObject).eql({ fake: 1 });
      });

      it("should attach a video object for Firefox", function() {
        fakeViewElement.mozSrcObject = null;

        sinon.stub(view, "getDOMNode").returns(fakeViewElement);

        view.attachVideo({
          mozSrcObject: { fake: 2 }
        });

        expect(fakeViewElement.mozSrcObject).eql({ fake: 2 });
      });

      it("should attach a video object for Chrome", function() {
        fakeViewElement.src = null;

        sinon.stub(view, "getDOMNode").returns(fakeViewElement);

        view.attachVideo({
          src: { fake: 2 }
        });

        expect(fakeViewElement.src).eql({ fake: 2 });
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
        showInitialContext: false,
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

      loop.store.StoreMixin.register({ textChatStore: textChatStore });
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
