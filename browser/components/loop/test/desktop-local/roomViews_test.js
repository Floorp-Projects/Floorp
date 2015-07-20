/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

describe("loop.roomViews", function () {
  "use strict";

  var expect = chai.expect;
  var TestUtils = React.addons.TestUtils;
  var sharedActions = loop.shared.actions;
  var sharedUtils = loop.shared.utils;
  var sharedViews = loop.shared.views;
  var ROOM_STATES = loop.store.ROOM_STATES;
  var SCREEN_SHARE_STATES = loop.shared.utils.SCREEN_SHARE_STATES;

  var sandbox, dispatcher, roomStore, activeRoomStore, fakeWindow,
    fakeMozLoop, fakeContextURL;
  var favicon = "data:image/x-icon;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==";

  beforeEach(function() {
    sandbox = sinon.sandbox.create();

    dispatcher = new loop.Dispatcher();

    fakeMozLoop = {
      getAudioBlob: sinon.stub(),
      getLoopPref: sinon.stub(),
      getSelectedTabMetadata: sinon.stub().callsArgWith(0, {
        favicon: favicon,
        previews: [],
        title: ""
      }),
      openURL: sinon.stub(),
      rooms: {
        get: sinon.stub().callsArgWith(1, null, {
          roomToken: "fakeToken",
          roomName: "fakeName",
          decryptedContext: {
            roomName: "fakeName",
            urls: []
          }
        }),
        update: sinon.stub().callsArgWith(2, null)
      },
      telemetryAddValue: sinon.stub(),
      setLoopPref: sandbox.stub()
    };

    fakeWindow = {
      close: sinon.stub(),
      document: {},
      navigator: {
        mozLoop: fakeMozLoop
      },
      addEventListener: function() {},
      removeEventListener: function() {},
      setTimeout: function(callback) { callback(); }
    };
    loop.shared.mixins.setRootObject(fakeWindow);

    // XXX These stubs should be hoisted in a common file
    // Bug 1040968
    sandbox.stub(document.mozL10n, "get", function(x) {
      return x;
    });

    activeRoomStore = new loop.store.ActiveRoomStore(dispatcher, {
      mozLoop: {},
      sdkDriver: {}
    });
    roomStore = new loop.store.RoomStore(dispatcher, {
      mozLoop: fakeMozLoop,
      activeRoomStore: activeRoomStore
    });
    var textChatStore = new loop.store.TextChatStore(dispatcher, {
      sdkDriver: {}
    });

    loop.store.StoreMixin.register({
      textChatStore: textChatStore
    });

    fakeContextURL = {
      description: "An invalid page",
      location: "http://invalid.com",
      thumbnail: "data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
    };
  });

  afterEach(function() {
    sandbox.restore();
    loop.shared.mixins.setRootObject(window);
  });

  describe("ActiveRoomStoreMixin", function() {
    it("should merge initial state", function() {
      var TestView = React.createClass({
        mixins: [loop.roomViews.ActiveRoomStoreMixin],
        getInitialState: function() {
          return {foo: "bar"};
        },
        render: function() { return React.DOM.div(); }
      });

      var testView = TestUtils.renderIntoDocument(
        React.createElement(TestView, {
          roomStore: roomStore
        }));

      var expectedState = _.extend({foo: "bar", savingContext: false},
        activeRoomStore.getInitialStoreState());

      expect(testView.state).eql(expectedState);
    });

    it("should listen to store changes", function() {
      var TestView = React.createClass({
        mixins: [loop.roomViews.ActiveRoomStoreMixin],
        render: function() { return React.DOM.div(); }
      });
      var testView = TestUtils.renderIntoDocument(
        React.createElement(TestView, {
          roomStore: roomStore
        }));

      activeRoomStore.setStoreState({roomState: ROOM_STATES.READY});

      expect(testView.state.roomState).eql(ROOM_STATES.READY);
    });
  });

  describe("DesktopRoomInvitationView", function() {
    var view;

    beforeEach(function() {
      sandbox.stub(dispatcher, "dispatch");
    });

    afterEach(function() {
      view = null;
    });

    function mountTestComponent(props) {
      props = _.extend({
        dispatcher: dispatcher,
        mozLoop: fakeMozLoop,
        roomData: {},
        savingContext: false,
        show: true,
        showEditContext: false
      }, props);
      return TestUtils.renderIntoDocument(
        React.createElement(loop.roomViews.DesktopRoomInvitationView, props));
    }

    it("should dispatch an EmailRoomUrl action when the email button is pressed",
      function() {
        view = mountTestComponent({
          roomData: { roomUrl: "http://invalid" }
        });

        var emailBtn = view.getDOMNode().querySelector(".btn-email");

        React.addons.TestUtils.Simulate.click(emailBtn);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWith(dispatcher.dispatch,
          new sharedActions.EmailRoomUrl({
            roomUrl: "http://invalid",
            roomDescription: undefined,
            from: "conversation"
          }));
      });

    it("should dispatch a different EmailRoomUrl action for rooms with context",
      function() {
        var url = "http://invalid";
        var description = "Hello, is it me you're looking for?";
        view = mountTestComponent({
          roomData: {
            roomUrl: url,
            roomContextUrls: [{ description: description }]
          }
        });

        var emailBtn = view.getDOMNode().querySelector(".btn-email");

        React.addons.TestUtils.Simulate.click(emailBtn);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWith(dispatcher.dispatch,
          new sharedActions.EmailRoomUrl({
            roomUrl: url,
            roomDescription: description,
            from: "conversation"
          }));
      });

    describe("Copy Button", function() {
      beforeEach(function() {
        view = mountTestComponent({
          roomData: { roomUrl: "http://invalid" }
        });
      });

      it("should dispatch a CopyRoomUrl action when the copy button is pressed", function() {
          var copyBtn = view.getDOMNode().querySelector(".btn-copy");

          React.addons.TestUtils.Simulate.click(copyBtn);

          sinon.assert.calledOnce(dispatcher.dispatch);
          sinon.assert.calledWith(dispatcher.dispatch, new sharedActions.CopyRoomUrl({
            roomUrl: "http://invalid",
            from: "conversation"
          }));
        });

      it("should change the text when the url has been copied", function() {
          var copyBtn = view.getDOMNode().querySelector(".btn-copy");

          React.addons.TestUtils.Simulate.click(copyBtn);

          // copied_url_button is the l10n string.
          expect(copyBtn.textContent).eql("copied_url_button");
      });
    });

    describe("Share button", function() {
      it("should dispatch a AddSocialShareProvider action when the share button is clicked", function() {
        view = mountTestComponent();

        var shareBtn = view.getDOMNode().querySelector(".btn-share");

        React.addons.TestUtils.Simulate.click(shareBtn);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWith(dispatcher.dispatch,
          new sharedActions.AddSocialShareProvider());
      });

      it("should toggle the share dropdown when the share button is clicked", function() {
        view = mountTestComponent({
          socialShareProviders: [{
            name: "foo",
            origin: "https://foo",
            iconURL: "http://example.com/foo.png"
          }]
        });

        var shareBtn = view.getDOMNode().querySelector(".btn-share");

        React.addons.TestUtils.Simulate.click(shareBtn);

        expect(view.state.showMenu).to.eql(true);
        expect(view.refs.menu.props.show).to.eql(true);
      });
    });

    describe("Edit Context", function() {
      it("should show the 'Add some context' link", function() {
        view = mountTestComponent();

        expect(view.getDOMNode().querySelector(".room-invitation-addcontext")).
          to.not.eql(null);
      });

      it("should call a callback when the link is clicked", function() {
        var onAddContextClick = sinon.stub();
        view = mountTestComponent({
          onAddContextClick: onAddContextClick
        });

        var node = view.getDOMNode();
        expect(node.querySelector(".room-context")).to.eql(null);

        var addLink = node.querySelector(".room-invitation-addcontext");

        React.addons.TestUtils.Simulate.click(addLink);

        sinon.assert.calledOnce(onAddContextClick);
      });

      it("should show the edit context view", function() {
        view = mountTestComponent({
          showEditContext: true
        });

        expect(view.getDOMNode().querySelector(".room-context")).to.not.eql(null);
      });
    });
  });

  describe("DesktopRoomConversationView", function() {
    var view, onCallTerminatedStub;

    beforeEach(function() {
      sandbox.stub(dispatcher, "dispatch");
      fakeMozLoop.getLoopPref = function(prefName) {
        if (prefName == "contextInConversations.enabled") {
          return true;
        }
        return "test";
      };
      onCallTerminatedStub = sandbox.stub();
    });

    function mountTestComponent(props) {
      props = _.extend({
        dispatcher: dispatcher,
        roomStore: roomStore,
        mozLoop: fakeMozLoop,
        onCallTerminated: onCallTerminatedStub
      }, props);
      return TestUtils.renderIntoDocument(
        React.createElement(loop.roomViews.DesktopRoomConversationView, props));
    }

    it("should dispatch a setMute action when the audio mute button is pressed",
      function() {
        view = mountTestComponent();

        view.setState({audioMuted: true});

        var muteBtn = view.getDOMNode().querySelector(".btn-mute-audio");

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
        view = mountTestComponent();

        view.setState({videoMuted: false});

        var muteBtn = view.getDOMNode().querySelector(".btn-mute-video");

        React.addons.TestUtils.Simulate.click(muteBtn);

        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "setMute"));
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("enabled", false));
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("type", "video"));
      });

    it("should set the mute button as mute off", function() {
      view = mountTestComponent();

      view.setState({videoMuted: false});

      var muteBtn = view.getDOMNode().querySelector(".btn-mute-video");

      expect(muteBtn.classList.contains("muted")).eql(false);
    });

    it("should set the mute button as mute on", function() {
      view = mountTestComponent();

      view.setState({audioMuted: true});

      var muteBtn = view.getDOMNode().querySelector(".btn-mute-audio");

      expect(muteBtn.classList.contains("muted")).eql(true);
    });

    it("should dispatch a `StartScreenShare` action when sharing is not active and the screen share button is pressed", function() {
      view = mountTestComponent();

      view.setState({screenSharingState: SCREEN_SHARE_STATES.INACTIVE});

      var muteBtn = view.getDOMNode().querySelector(".btn-mute-video");

      React.addons.TestUtils.Simulate.click(muteBtn);

      sinon.assert.calledWithMatch(dispatcher.dispatch,
        sinon.match.hasOwn("name", "setMute"));
    });

    it("should dispatch a `LeaveRoom` action when the hangup button is pressed and the room has been used", function() {
      view = mountTestComponent();

      view.setState({used: true});

      var hangupBtn = view.getDOMNode().querySelector(".btn-hangup");

      React.addons.TestUtils.Simulate.click(hangupBtn);

      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWithExactly(dispatcher.dispatch,
        new sharedActions.LeaveRoom());
    });

    it("should close the window when the hangup button is pressed and the room has not been used", function() {
      view = mountTestComponent();

      view.setState({used: false});

      var hangupBtn = view.getDOMNode().querySelector(".btn-hangup");

      React.addons.TestUtils.Simulate.click(hangupBtn);

      sinon.assert.calledOnce(fakeWindow.close);
    });

    describe("#componentWillUpdate", function() {
      function expectActionDispatched(component) {
        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          sinon.match.instanceOf(sharedActions.SetupStreamElements));
      }

      it("should dispatch a `SetupStreamElements` action when the MEDIA_WAIT state is entered", function() {
          activeRoomStore.setStoreState({roomState: ROOM_STATES.READY});
          var component = mountTestComponent();

          activeRoomStore.setStoreState({roomState: ROOM_STATES.MEDIA_WAIT});

          expectActionDispatched(component);
        });

      it("should dispatch a `SetupStreamElements` action on MEDIA_WAIT state is re-entered", function() {
          activeRoomStore.setStoreState({roomState: ROOM_STATES.ENDED});
          var component = mountTestComponent();

          activeRoomStore.setStoreState({roomState: ROOM_STATES.MEDIA_WAIT});

          expectActionDispatched(component);
        });
    });

    describe("#render", function() {
      it("should set document.title to store.serverData.roomName", function() {
        mountTestComponent();

        activeRoomStore.setStoreState({roomName: "fakeName"});

        expect(fakeWindow.document.title).to.equal("fakeName");
      });

      it("should render the GenericFailureView if the roomState is `FAILED`",
        function() {
          activeRoomStore.setStoreState({roomState: ROOM_STATES.FAILED});

          view = mountTestComponent();

          TestUtils.findRenderedComponentWithType(view,
            loop.conversationViews.GenericFailureView);
        });

      it("should render the GenericFailureView if the roomState is `FULL`",
        function() {
          activeRoomStore.setStoreState({roomState: ROOM_STATES.FULL});

          view = mountTestComponent();

          TestUtils.findRenderedComponentWithType(view,
            loop.conversationViews.GenericFailureView);
        });

      it("should render the DesktopRoomInvitationView if roomState is `JOINED`",
        function() {
          activeRoomStore.setStoreState({roomState: ROOM_STATES.JOINED});

          view = mountTestComponent();

          TestUtils.findRenderedComponentWithType(view,
            loop.roomViews.DesktopRoomInvitationView);
        });

      it("should render the DesktopRoomConversationView if roomState is `HAS_PARTICIPANTS`",
        function() {
          activeRoomStore.setStoreState({roomState: ROOM_STATES.HAS_PARTICIPANTS});

          view = mountTestComponent();

          TestUtils.findRenderedComponentWithType(view,
            loop.roomViews.DesktopRoomConversationView);
        });

      it("should call onCallTerminated when the call ended", function() {
        activeRoomStore.setStoreState({
          roomState: ROOM_STATES.ENDED,
          used: true
        });

        view = mountTestComponent();
        // Force a state change so that it triggers componentDidUpdate
        view.setState({ foo: "bar" });

        sinon.assert.calledOnce(onCallTerminatedStub);
      });

      it("should display loading spinner when localSrcVideoObject is null",
         function() {
           activeRoomStore.setStoreState({
             roomState: ROOM_STATES.MEDIA_WAIT,
             localSrcVideoObject: null
           });

           view = mountTestComponent();

           expect(view.getDOMNode().querySelector(".local .loading-stream"))
               .not.eql(null);
         });

      it("should not display a loading spinner when local stream available",
         function() {
           activeRoomStore.setStoreState({
             roomState: ROOM_STATES.MEDIA_WAIT,
             localSrcVideoObject: { fake: "video" }
           });

           view = mountTestComponent();

           expect(view.getDOMNode().querySelector(".local .loading-stream"))
               .eql(null);
         });

      it("should display loading spinner when remote stream is not available",
         function() {
           activeRoomStore.setStoreState({
             roomState: ROOM_STATES.HAS_PARTICIPANTS,
             remoteSrcVideoObject: null
           });

           view = mountTestComponent();

           expect(view.getDOMNode().querySelector(".remote .loading-stream"))
               .not.eql(null);
         });

      it("should not display a loading spinner when remote stream available",
         function() {
           activeRoomStore.setStoreState({
             roomState: ROOM_STATES.HAS_PARTICIPANTS,
             remoteSrcVideoObject: { fake: "video" }
           });

           view = mountTestComponent();

           expect(view.getDOMNode().querySelector(".remote .loading-stream"))
               .eql(null);
         });

      it("should display an avatar for remote video when the room has participants but video is not enabled",
        function() {
          activeRoomStore.setStoreState({
            roomState: ROOM_STATES.HAS_PARTICIPANTS,
            mediaConnected: true,
            remoteVideoEnabled: false
          });

          view = mountTestComponent();

          TestUtils.findRenderedComponentWithType(view, sharedViews.AvatarView);
        });

      it("should display the remote video when there are participants and video is enabled", function() {
        activeRoomStore.setStoreState({
          roomState: ROOM_STATES.HAS_PARTICIPANTS,
          mediaConnected: true,
          remoteVideoEnabled: true,
          remoteSrcVideoObject: { fake: 1 }
        });

        view = mountTestComponent();

        expect(view.getDOMNode().querySelector(".remote video")).not.eql(null);
      });

      it("should display an avatar for local video when the stream is muted", function() {
        activeRoomStore.setStoreState({
          videoMuted: true
        });

        view = mountTestComponent();

        TestUtils.findRenderedComponentWithType(view, sharedViews.AvatarView);
      });

      it("should display the local video when the stream is enabled", function() {
        activeRoomStore.setStoreState({
          localSrcVideoObject: { fake: 1 },
          videoMuted: false
        });

        view = mountTestComponent();

        expect(view.getDOMNode().querySelector(".local video")).not.eql(null);
      });

    });

    describe("Mute", function() {
      it("should render local media as audio-only if video is muted",
        function() {
          activeRoomStore.setStoreState({
            roomState: ROOM_STATES.SESSION_CONNECTED,
            videoMuted: true
          });

          view = mountTestComponent();

          expect(view.getDOMNode().querySelector(".local-stream-audio"))
            .not.eql(null);
        });
    });

    describe("Edit Context", function() {
      it("should show the form when the edit button is clicked", function() {
        view = mountTestComponent();
        var node = view.getDOMNode();

        expect(node.querySelector(".room-context")).to.eql(null);

        var editButton = node.querySelector(".btn-mute-edit");
        React.addons.TestUtils.Simulate.click(editButton);

        expect(view.getDOMNode().querySelector(".room-context")).to.not.eql(null);
      });

      it("should hide the form when the edit button is clicked again", function() {
        view = mountTestComponent();

        var editButton = view.getDOMNode().querySelector(".btn-mute-edit");
        React.addons.TestUtils.Simulate.click(editButton);

        // Click again.
        React.addons.TestUtils.Simulate.click(editButton);

        expect(view.getDOMNode().querySelector(".room-context")).to.eql(null);
      });
    });
  });

  describe("SocialShareDropdown", function() {
    var view, fakeProvider;

    beforeEach(function() {
      sandbox.stub(dispatcher, "dispatch");

      fakeProvider = {
        name: "foo",
        origin: "https://foo",
        iconURL: "http://example.com/foo.png"
      };
    });

    afterEach(function() {
      view = fakeProvider = null;
    });

    function mountTestComponent(props) {
      props = _.extend({
        dispatcher: dispatcher,
        show: true
      }, props);
      return TestUtils.renderIntoDocument(
        React.createElement(loop.roomViews.SocialShareDropdown, props));
    }

    describe("#render", function() {
      it("should show no contents when the Social Providers have not been fetched yet", function() {
        view = mountTestComponent();

        expect(view.getDOMNode()).to.eql(null);
      });

      it("should show an empty list when no Social Providers are available", function() {
        view = mountTestComponent({
          socialShareProviders: []
        });

        var node = view.getDOMNode();
        expect(node.querySelector(".icon-add-share-service")).to.not.eql(null);
        expect(node.querySelectorAll(".dropdown-menu-item").length).to.eql(1);
      });

      it("should show a list of available Social Providers", function() {
        view = mountTestComponent({
          socialShareProviders: [fakeProvider]
        });

        var node = view.getDOMNode();
        expect(node.querySelector(".icon-add-share-service")).to.not.eql(null);
        expect(node.querySelector(".dropdown-menu-separator")).to.not.eql(null);

        var dropdownNodes = node.querySelectorAll(".dropdown-menu-item");
        expect(dropdownNodes.length).to.eql(2);
        expect(dropdownNodes[1].querySelector("img").src).to.eql(fakeProvider.iconURL);
        expect(dropdownNodes[1].querySelector("span").textContent)
          .to.eql(fakeProvider.name);
      });
    });

    describe("#handleAddServiceClick", function() {
      it("should dispatch an action when the 'add provider' item is clicked", function() {
        view = mountTestComponent({
          socialShareProviders: []
        });

        var addItem = view.getDOMNode().querySelector(".dropdown-menu-item:first-child");
        React.addons.TestUtils.Simulate.click(addItem);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.AddSocialShareProvider());
      });
    });

    describe("#handleProviderClick", function() {
      it("should dispatch an action when a provider item is clicked", function() {
        view = mountTestComponent({
          roomUrl: "http://example.com",
          socialShareProviders: [fakeProvider]
        });

        var providerItem = view.getDOMNode().querySelector(".dropdown-menu-item:last-child");
        React.addons.TestUtils.Simulate.click(providerItem);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.ShareRoomUrl({
            provider: fakeProvider,
            roomUrl: "http://example.com",
            previews: []
          }));
      });
    });
  });

  describe("DesktopRoomEditContextView", function() {
    var view;

    afterEach(function() {
      view = null;
    });

    function mountTestComponent(props) {
      props = _.extend({
        dispatcher: dispatcher,
        mozLoop: fakeMozLoop,
        savingContext: false,
        show: true,
        roomData: {
          roomToken: "fakeToken"
        }
      }, props);
      return TestUtils.renderIntoDocument(
        React.createElement(loop.roomViews.DesktopRoomEditContextView, props));
    }

    describe("#render", function() {
      it("should not render the component when 'show' is false", function() {
        view = mountTestComponent({
          show: false
        });

        expect(view.getDOMNode()).to.eql(null);
      });

      it("should close the view when the close button is clicked", function() {
        view = mountTestComponent({
          roomData: { roomContextUrls: [fakeContextURL] }
        });

        var closeBtn = view.getDOMNode().querySelector(".room-context-btn-close");
        React.addons.TestUtils.Simulate.click(closeBtn);
        expect(view.getDOMNode()).to.eql(null);
      });

      it("should render the view correctly", function() {
        var roomName = "Hello, is it me you're looking for?";
        view = mountTestComponent({
          roomData: {
            roomName: roomName,
            roomContextUrls: [fakeContextURL]
          }
        });

        var node = view.getDOMNode();
        expect(node.querySelector("form")).to.not.eql(null);
        // Check the contents of the form fields.
        expect(node.querySelector(".room-context-name").value).to.eql(roomName);
        expect(node.querySelector(".room-context-url").value).to.eql(fakeContextURL.location);
        expect(node.querySelector(".room-context-comments").value).to.eql(fakeContextURL.description);
      });

      it("should show the checkbox as disabled when context is already set", function() {
        view = mountTestComponent({
          roomData: {
            roomToken: "fakeToken",
            roomName: "fakeName",
            roomContextUrls: [fakeContextURL]
          }
        });

        var checkbox = view.getDOMNode().querySelector(".checkbox");
        expect(checkbox.classList.contains("disabled")).to.eql(true);
      });

      it("should hide the checkbox when no context data is stored or available", function() {
        view = mountTestComponent({
          roomData: {
            roomToken: "fakeToken",
            roomName: "Hello, is it me you're looking for?"
          }
        });

        // First check if availableContext is set correctly.
        expect(view.state.availableContext).to.not.eql(null);
        expect(view.state.availableContext.previewImage).to.eql(favicon);

        var node = view.getDOMNode();
        expect(node.querySelector(".checkbox-wrapper").classList.contains("hide")).to.eql(true);
      });
    });

    describe("Update Room", function() {
      var roomNameBox;

      beforeEach(function() {
        sandbox.stub(dispatcher, "dispatch");

        view = mountTestComponent({
          editMode: true,
          roomData: {
            roomToken: "fakeToken",
            roomName: "fakeName",
            roomContextUrls: [fakeContextURL]
          }
        });

        roomNameBox = view.getDOMNode().querySelector(".room-context-name");
      });

      it("should dispatch a UpdateRoomContext action when the save button is clicked",
        function() {
          React.addons.TestUtils.Simulate.change(roomNameBox, { target: {
            value: "reallyFake"
          }});

          React.addons.TestUtils.Simulate.click(view.getDOMNode().querySelector(".btn-info"));

          sinon.assert.calledOnce(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.UpdateRoomContext({
              roomToken: "fakeToken",
              newRoomName: "reallyFake",
              newRoomDescription: fakeContextURL.description,
              newRoomURL: fakeContextURL.location,
              newRoomThumbnail: fakeContextURL.thumbnail
            }));
        });

      it("should dispatch a UpdateRoomContext action when Enter key is pressed",
        function() {
          React.addons.TestUtils.Simulate.change(roomNameBox, { target: {
            value: "reallyFake"
          }});

          TestUtils.Simulate.keyDown(roomNameBox, {key: "Enter", which: 13});

          sinon.assert.calledOnce(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.UpdateRoomContext({
              roomToken: "fakeToken",
              newRoomName: "reallyFake",
              newRoomDescription: fakeContextURL.description,
              newRoomURL: fakeContextURL.location,
              newRoomThumbnail: fakeContextURL.thumbnail
            }));
        });

      it("should close the edit form when context was saved successfully", function(done) {
        view.setProps({ savingContext: true }, function() {
          var node = view.getDOMNode();
          // The button should show up as disabled.
          expect(node.querySelector(".btn-info").hasAttribute("disabled")).to.eql(true);

          // Now simulate a successful save.
          view.setProps({ savingContext: false }, function() {
            // The 'show flag should be updated.
            expect(view.state.show).to.eql(false);
            done();
          });
        });
      });
    });

    describe("#handleCheckboxChange", function() {
      var node, checkbox;

      beforeEach(function() {
        fakeMozLoop.getSelectedTabMetadata = sinon.stub().callsArgWith(0, {
          favicon: fakeContextURL.thumbnail,
          title: fakeContextURL.description,
          url: fakeContextURL.location
        });
        view = mountTestComponent({
          roomData: {
            roomToken: "fakeToken",
            roomName: "fakeName"
          }
        });

        node = view.getDOMNode();
        checkbox = node.querySelector(".checkbox");
      });

      it("should prefill the form with available context data when clicked", function() {
        React.addons.TestUtils.Simulate.click(checkbox);

        expect(node.querySelector(".room-context-name").value).to.eql("fakeName");
        expect(node.querySelector(".room-context-url").value).to.eql(fakeContextURL.location);
        expect(node.querySelector(".room-context-comments").value).to.eql(fakeContextURL.description);
      });

      it("should undo prefill when clicking the checkbox again", function() {
        React.addons.TestUtils.Simulate.click(checkbox);
        // Twice.
        React.addons.TestUtils.Simulate.click(checkbox);

        expect(node.querySelector(".room-context-name").value).to.eql("fakeName");
        expect(node.querySelector(".room-context-url").value).to.eql("");
        expect(node.querySelector(".room-context-comments").value).to.eql("");
      });
    });

    describe("#handleContextClick", function() {
      var fakeEvent;

      beforeEach(function() {
        fakeEvent = {
          preventDefault: sinon.stub(),
          stopPropagation: sinon.stub()
        };
      });

      it("should not attempt to open a URL when none is attached", function() {
        view = mountTestComponent({
          roomData: {
            roomToken: "fakeToken",
            roomName: "fakeName"
          }
        });

        view.handleContextClick(fakeEvent);

        sinon.assert.calledOnce(fakeEvent.preventDefault);
        sinon.assert.calledOnce(fakeEvent.stopPropagation);

        sinon.assert.notCalled(fakeMozLoop.openURL);
        sinon.assert.notCalled(fakeMozLoop.telemetryAddValue);
      });

      it("should open a URL", function() {
        view = mountTestComponent({
          roomData: {
            roomToken: "fakeToken",
            roomName: "fakeName",
            roomContextUrls: [fakeContextURL]
          }
        });

        view.handleContextClick(fakeEvent);

        sinon.assert.calledOnce(fakeEvent.preventDefault);
        sinon.assert.calledOnce(fakeEvent.stopPropagation);

        sinon.assert.calledOnce(fakeMozLoop.openURL);
        sinon.assert.calledWithExactly(fakeMozLoop.openURL, fakeContextURL.location);
        sinon.assert.calledOnce(fakeMozLoop.telemetryAddValue);
        sinon.assert.calledWithExactly(fakeMozLoop.telemetryAddValue,
          "LOOP_ROOM_CONTEXT_CLICK", 1);
      });
    });
  });
});
