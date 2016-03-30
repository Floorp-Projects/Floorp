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

    loop.config = {
      tilesIframeUrl: "",
      tilesSupportUrl: ""
    };
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
          muted: false
        }));

      expect(comp.getDOMNode().classList.contains("muted")).eql(false);
    });

    it("should render a muted local audio button", function() {
      var comp = TestUtils.renderIntoDocument(
          React.createElement(sharedViews.MediaControlButton, {
          scope: "local",
          type: "audio",
          action: function() {},
          muted: true
        }));

      expect(comp.getDOMNode().classList.contains("muted")).eql(true);
    });

    it("should render an enabled local video button", function() {
      var comp = TestUtils.renderIntoDocument(
          React.createElement(sharedViews.MediaControlButton, {
          scope: "local",
          type: "video",
          action: function() {},
          muted: false
        }));

      expect(comp.getDOMNode().classList.contains("muted")).eql(false);
    });

    it("should render a muted local video button", function() {
      var comp = TestUtils.renderIntoDocument(
        React.createElement(sharedViews.MediaControlButton, {
          scope: "local",
          type: "video",
          action: function() {},
          muted: true
        }));

      expect(comp.getDOMNode().classList.contains("muted")).eql(true);
    });
  });

  describe("AudioMuteButton", function() {
    it("should set the muted class when not enabled", function() {
      var comp = TestUtils.renderIntoDocument(
        React.createElement(sharedViews.AudioMuteButton, {
          muted: true
        }));

      var node = comp.getDOMNode();
      expect(node.classList.contains("muted")).eql(true);
    });

    it("should not set the muted class when enabled", function() {
      var comp = TestUtils.renderIntoDocument(
        React.createElement(sharedViews.AudioMuteButton, {
          muted: false
        }));

      var node = comp.getDOMNode();
      expect(node.classList.contains("muted")).eql(false);
    });

    it("should dispatch SetMute('audio', false) if clicked when audio is disabled",
      function() {
        var comp = TestUtils.renderIntoDocument(
          React.createElement(sharedViews.AudioMuteButton, {
            dispatcher: dispatcher,
            muted: false
          }));

        TestUtils.Simulate.click(comp.getDOMNode());

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.SetMute({ type: "audio", enabled: false })
        );
      });

    it("should dispatch SetMute('audio', true) if clicked when audio is enabled",
      function() {
        var comp = TestUtils.renderIntoDocument(
          React.createElement(sharedViews.AudioMuteButton, {
            dispatcher: dispatcher,
            muted: true
          }));

        TestUtils.Simulate.click(comp.getDOMNode());

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.SetMute({ type: "audio", enabled: true })
        );
      });
  });

  describe("VideoMuteButton", function() {
    it("should set the muted class when not enabled", function() {
      var comp = TestUtils.renderIntoDocument(
        React.createElement(sharedViews.VideoMuteButton, {
          muted: true
        }));

      var node = comp.getDOMNode();
      expect(node.classList.contains("muted")).eql(true);
    });

    it("should not set the muted class when enabled", function() {
      var comp = TestUtils.renderIntoDocument(
        React.createElement(sharedViews.VideoMuteButton, {
          muted: false
        }));

      var node = comp.getDOMNode();
      expect(node.classList.contains("muted")).eql(false);
    });

    it("should dispatch SetMute('audio', false) if clicked when audio is disabled",
      function() {
        var comp = TestUtils.renderIntoDocument(
          React.createElement(sharedViews.VideoMuteButton, {
            dispatcher: dispatcher,
            muted: false
          }));

        TestUtils.Simulate.click(comp.getDOMNode());

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.SetMute({ type: "video", enabled: false })
        );
      });

    it("should dispatch SetMute('audio', true) if clicked when audio is enabled",
      function() {
        var comp = TestUtils.renderIntoDocument(
          React.createElement(sharedViews.VideoMuteButton, {
            dispatcher: dispatcher,
            muted: true
          }));

        TestUtils.Simulate.click(comp.getDOMNode());

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.SetMute({ type: "video", enabled: true })
        );
      });
  });

  describe("ConversationToolbar", function() {
    var hangup;

    function mountTestComponent(props) {
      props = _.extend({
        dispatcher: dispatcher
      }, props || {});
      return TestUtils.renderIntoDocument(
        React.createElement(sharedViews.ConversationToolbar, props));
    }

    beforeEach(function() {
      hangup = sandbox.stub();
    });

    it("should start no idle", function() {
      var comp = mountTestComponent({
        hangupButtonLabel: "foo",
        hangup: hangup
      });
      expect(comp.getDOMNode().classList.contains("idle")).eql(false);
    });

    it("should be on idle state after 6 seconds", function() {
      var comp = mountTestComponent({
        hangupButtonLabel: "foo",
        hangup: hangup
      });
      expect(comp.getDOMNode().classList.contains("idle")).eql(false);

      clock.tick(6001);
      expect(comp.getDOMNode().classList.contains("idle")).eql(true);
    });

    it("should remove idle state when the user moves the mouse", function() {
      var comp = mountTestComponent({
        hangupButtonLabel: "foo",
        hangup: hangup
      });

      clock.tick(6001);
      expect(comp.getDOMNode().classList.contains("idle")).eql(true);

      document.body.dispatchEvent(new CustomEvent("mousemove"));

      expect(comp.getDOMNode().classList.contains("idle")).eql(false);
    });

    it("should accept a showHangup optional prop", function() {
      var comp = mountTestComponent({
        showHangup: false,
        hangup: hangup
      });

      expect(comp.getDOMNode().querySelector(".btn-hangup-entry")).to.eql(null);
    });

    it("should hangup when hangup button is clicked", function() {
      var comp = mountTestComponent({
        hangup: hangup,
        audio: { enabled: true }
      });

      TestUtils.Simulate.click(
        comp.getDOMNode().querySelector(".btn-hangup"));

      sinon.assert.calledOnce(hangup);
      sinon.assert.calledWithExactly(hangup);
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
        showContextTitle: false
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

    it("should display nothing if it is an about url", function() {
      view = mountTestComponent({
        url: "about:config"
      });

      expect(view.getDOMNode()).eql(null);
    });

    it("should display nothing if it is a javascript url", function() {
      /* eslint-disable no-script-url */
      view = mountTestComponent({
        url: "javascript:alert('hello')"
      });

      expect(view.getDOMNode()).eql(null);
      /* eslint-enable no-script-url */
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
    var remoteCursorStore;

    function mountTestComponent(props) {
      props = _.extend({
        isLoading: false
      }, props || {});
      return TestUtils.renderIntoDocument(
        React.createElement(sharedViews.MediaView, props));
    }

    beforeEach(function() {
      remoteCursorStore = new loop.store.RemoteCursorStore(dispatcher, {
        sdkDriver: {}
      });
      loop.store.StoreMixin.register({ remoteCursorStore: remoteCursorStore });
    });

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

      var element = view.getDOMNode().querySelector("video");

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
      var fakeViewElement,
          fakeVideoElement;

      beforeEach(function() {
        fakeVideoElement = {
          play: sinon.stub(),
          tagName: "VIDEO",
          addEventListener: sinon.stub()
        };

        fakeViewElement = {
          tagName: "DIV",
          querySelector: function() {
            return fakeVideoElement;
          }
        };

        view = mountTestComponent({
          cursorStore: remoteCursorStore,
          displayAvatar: false,
          mediaType: "local",
          shareCursor: true,
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
          tagName: "DIV",
          querySelector: function() {
            return {
              tagName: "DIV",
              addEventListener: sinon.stub()
            };
          }
        });

        expect(function() {
          view.attachVideo({});
        }).to.not.Throw();
      });

      it("should attach a video object according to the standard", function() {
        fakeVideoElement.srcObject = null;

        sinon.stub(view, "getDOMNode").returns(fakeViewElement);

        view.attachVideo({
          srcObject: { fake: 1 }
        });

        expect(fakeVideoElement.srcObject).eql({ fake: 1 });
      });

      it("should attach events to the video", function() {
        fakeVideoElement.srcObject = null;
        sinon.stub(view, "getDOMNode").returns(fakeViewElement);
        view.attachVideo({
          src: { fake: 1 }
        });

        sinon.assert.calledThrice(fakeVideoElement.addEventListener);
        sinon.assert.calledWith(fakeVideoElement.addEventListener, "loadeddata");
        sinon.assert.calledWith(fakeVideoElement.addEventListener, "mousemove");
        sinon.assert.calledWith(fakeVideoElement.addEventListener, "click");
      });

      it("should attach a video object for Firefox", function() {
        fakeVideoElement.mozSrcObject = null;

        sinon.stub(view, "getDOMNode").returns(fakeViewElement);

        view.attachVideo({
          mozSrcObject: { fake: 2 }
        });

        expect(fakeVideoElement.mozSrcObject).eql({ fake: 2 });
      });

      it("should attach a video object for Chrome", function() {
        fakeVideoElement.src = null;

        sinon.stub(view, "getDOMNode").returns(fakeViewElement);

        view.attachVideo({
          src: { fake: 2 }
        });

        expect(fakeVideoElement.src).eql({ fake: 2 });
      });
    });

    describe("#handleVideoDimensions", function() {
      var fakeViewElement, fakeVideoElement;

      beforeEach(function() {
        fakeVideoElement = {
          clientWidth: 1000,
          clientHeight: 1000,
          play: sinon.stub(),
          srcObject: null,
          tagName: "VIDEO"
        };

        fakeViewElement = {
          tagName: "DIV",
          querySelector: function() {
            return fakeVideoElement;
          }
        };

        view = mountTestComponent({
          displayAvatar: false,
          mediaType: "local",
          srcMediaElement: {
            fake: 1
          }
        });

        sinon.stub(view, "getDOMNode").returns(fakeViewElement);
      });

      it("should save the video size", function() {
        view.handleVideoDimensions();

        expect(view.state.videoElementSize).eql({
          clientWidth: fakeVideoElement.clientWidth,
          clientHeight: fakeVideoElement.clientHeight
        });
      });
    });

    describe("handleMouseClick", function() {
      beforeEach(function() {
        view = mountTestComponent({
          dispatcher: dispatcher,
          displayAvatar: false,
          mediaType: "local",
          srcMediaElement: {
            fake: 1
          }
        });
      });

      it("should dispatch cursor click event when video element is clicked", function() {
        view.handleMouseClick();
        sinon.assert.calledOnce(dispatcher.dispatch);
      });
    });
  });

  describe("MediaLayoutView", function() {
    var textChatStore,
        remoteCursorStore,
        view;

    function mountTestComponent(extraProps) {
      var defaultProps = {
        cursorStore: remoteCursorStore,
        dispatcher: dispatcher,
        displayScreenShare: false,
        isLocalLoading: false,
        isRemoteLoading: false,
        isScreenShareLoading: false,
        localVideoMuted: false,
        matchMedia: window.matchMedia,
        renderRemoteVideo: false,
        showInitialContext: false,
        showMediaWait: false,
        showTile: false
      };

      return TestUtils.renderIntoDocument(
        React.createElement(sharedViews.MediaLayoutView,
          _.extend(defaultProps, extraProps)));
    }

    beforeEach(function() {
      loop.config = {
        tilesIframeUrl: "",
        tilesSupportUrl: ""
      };
      textChatStore = new loop.store.TextChatStore(dispatcher, {
        sdkDriver: {}
      });
      remoteCursorStore = new loop.store.RemoteCursorStore(dispatcher, {
        sdkDriver: {}
      });

      loop.store.StoreMixin.register({ textChatStore: textChatStore });

      // Need to stub these methods because when mounting the AdsTileView we are
      // attaching listeners to the window object and "AdsTileView" tests will failed
      sandbox.stub(window, "addEventListener");
      sandbox.stub(window, "removeEventListener");
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

    it("should mark the screen share stream as paused when screen shared has been paused", function() {
      view = mountTestComponent({
        screenSharingPaused: true
      });

      var node = view.getDOMNode();

      expect(node.querySelector(".screen").classList.contains("screen-sharing-paused")).eql(true);
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

    it("should mark the wrapper as showing media wait tile when asking for user media", function() {
      view = mountTestComponent({
        showMediaWait: true
      });

      expect(view.getDOMNode().querySelector(".media-wrapper")
        .classList.contains("showing-media-wait")).eql(true);
    });

    it("should display a media wait tile when asking for user media", function() {
      view = mountTestComponent({
        showMediaWait: true
      });

      expect(view.getDOMNode().querySelector(".prompt-media-message-wrapper"))
        .not.eql(null);
    });
  });

  describe("RemoteCursorView", function() {
    var view;
    var fakeVideoElementSize;
    var remoteCursorStore;

    function mountTestComponent(props) {
      props = props || {};
      var testView = TestUtils.renderIntoDocument(
        React.createElement(sharedViews.RemoteCursorView, props));

      testView.setState(remoteCursorStore.getStoreState());

      return testView;
    }

    beforeEach(function() {
      remoteCursorStore = new loop.store.RemoteCursorStore(dispatcher, {
        sdkDriver: {}
      });

      loop.store.StoreMixin.register({ remoteCursorStore: remoteCursorStore });

      remoteCursorStore.setStoreState({
        realVideoSize: {
          height: 1536,
          width: 2580
        },
        remoteCursorPosition: {
          ratioX: 0,
          ratioY: 0
        }
      });
    });

    it("video element ratio is not wider than stream video ratio", function() {
      fakeVideoElementSize = {
        clientWidth: 1280,
        clientHeight: 768
      };
      view = mountTestComponent({
        videoElementSize: fakeVideoElementSize
      });

      view._calculateVideoLetterboxing();
      var clientWidth = fakeVideoElementSize.clientWidth;
      var clientHeight = fakeVideoElementSize.clientHeight;

      var realVideoWidth = view.state.realVideoSize.width;
      var realVideoHeight = view.state.realVideoSize.height;
      var realVideoRatio = realVideoWidth / realVideoHeight;

      var streamVideoHeight = clientWidth / realVideoRatio;
      var streamVideoWidth = clientWidth;

      expect(view.state.videoLetterboxing).eql({
        left: (clientWidth - streamVideoWidth) / 2,
        top: (clientHeight - streamVideoHeight) / 2
      });
    });

    it("video element ratio is wider than stream video ratio", function() {
      fakeVideoElementSize = {
        clientWidth: 1152,
        clientHeight: 768
      };
      remoteCursorStore.setStoreState({
        realVideoSize: {
          height: 2580,
          width: 2580
        }
      });
      view = mountTestComponent({
        videoElementSize: fakeVideoElementSize
      });

      view._calculateVideoLetterboxing();
      var clientWidth = fakeVideoElementSize.clientWidth;
      var clientHeight = fakeVideoElementSize.clientHeight;

      var realVideoWidth = view.state.realVideoSize.width;
      var realVideoHeight = view.state.realVideoSize.height;
      var realVideoRatio = realVideoWidth / realVideoHeight;

      var streamVideoHeight = clientHeight;
      var streamVideoWidth = clientHeight * realVideoRatio;

      expect(view.state.videoLetterboxing).eql({
        left: (clientWidth - streamVideoWidth) / 2,
        top: (clientHeight - streamVideoHeight) / 2
      });
    });

    describe("#calculateCursorPosition", function() {
      beforeEach(function() {
        remoteCursorStore.setStoreState({
          remoteCursorPosition: {
            ratioX: 0.3,
            ratioY: 0.3
          }
        });
      });

      it("should calculate the cursor position coords in the stream video", function() {
        fakeVideoElementSize = {
          clientWidth: 1280,
          clientHeight: 768
        };
        remoteCursorStore.setStoreState({
          realVideoSize: {
            height: 2580,
            width: 2580
          }
        });
        view = mountTestComponent({
          videoElementSize: fakeVideoElementSize
        });
        view._calculateVideoLetterboxing();

        var cursorPositionX = view.state.streamVideoWidth * view.state.remoteCursorPosition.ratioX;
        var cursorPositionY = view.state.streamVideoHeight * view.state.remoteCursorPosition.ratioY;

        expect(view.calculateCursorPosition()).eql({
          left: cursorPositionX + view.state.videoLetterboxing.left,
          top: cursorPositionY + view.state.videoLetterboxing.top
        });
      });
    });

    describe("resetClickState", function() {
      beforeEach(function() {
        remoteCursorStore.setStoreState({ remoteCursorClick: true });
        view = mountTestComponent({
          videoElementSize: fakeVideoElementSize
        });
      });

      it("should restore the state to its default value", function() {
        view.resetClickState();

        expect(view.getStore().getStoreState().remoteCursorClick).eql(false);
      });
    });

    describe("#render", function() {
      beforeEach(function() {
        remoteCursorStore.setStoreState({ remoteCursorClick: true });
        view = mountTestComponent({
          videoElementSize: fakeVideoElementSize
        });
      });

      it("should add click class to the remote cursor", function() {
        expect(view.getDOMNode().classList.contains("remote-cursor-clicked")).eql(true);
      });

      it("should remove the click class when the animation is completed", function() {
        clock.tick(sharedViews.RemoteCursorView.TRIGGERED_RESET_DELAY);
        expect(view.getDOMNode().classList.contains("remote-cursor-clicked")).eql(false);
      });
    });
  });

  describe("AdsTileView", function() {
    it("should dispatch a RecordClick action when the tile is clicked", function(done) {
      // Point the iframe to a page that will auto-"click"
      loop.config.tilesIframeUrl = "data:text/html,<script>parent.postMessage('tile-click', '*');</script>";

      // Render the iframe into the fixture to cause it to load
      React.render(React.createElement(
        sharedViews.AdsTileView, {
          dispatcher: dispatcher,
          showTile: true
        }), document.querySelector("#fixtures"));

      // Wait for the iframe to load and trigger a message that should also
      // cause the RecordClick action
      window.addEventListener("message", function onMessage() {
        window.removeEventListener("message", onMessage);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.RecordClick({
            linkInfo: "Tiles iframe click"
          }));

        done();
      });
    });

    it("should dispatch a RecordClick action when the support button is clicked", function() {
      var view = TestUtils.renderIntoDocument(
        React.createElement(sharedViews.AdsTileView, {
          dispatcher: dispatcher,
          showTile: true
        }));

      var node = view.getDOMNode().querySelector("a");
      TestUtils.Simulate.click(node);
      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWithExactly(dispatcher.dispatch,
        new sharedActions.RecordClick({
          linkInfo: "Tiles support link click"
        }));
    });
  });
});
