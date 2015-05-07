var expect = chai.expect;

/* jshint newcap:false */

describe("loop.roomViews", function () {
  "use strict";

  var ROOM_STATES = loop.store.ROOM_STATES;
  var SCREEN_SHARE_STATES = loop.shared.utils.SCREEN_SHARE_STATES;

  var sandbox, dispatcher, roomStore, activeRoomStore, fakeWindow,
    fakeMozLoop, fakeContextURL;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();

    dispatcher = new loop.Dispatcher();

    fakeMozLoop = {
      getAudioBlob: sinon.stub(),
      getLoopPref: sinon.stub(),
      getSelectedTabMetadata: sinon.stub().callsArgWith(0, {
        previews: [],
        title: ""
      }),
      isSocialShareButtonAvailable: sinon.stub()
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
      mozLoop: {},
      activeRoomStore: activeRoomStore
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

      var expectedState = _.extend({foo: "bar"},
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
        show: true,
        showContext: false
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
          new sharedActions.EmailRoomUrl({roomUrl: "http://invalid"}));
      });

    describe("Copy Button", function() {
      beforeEach(function() {
        view = mountTestComponent({
          roomData: { roomUrl: "http://invalid" }
        });
      });

      it("should dispatch a CopyRoomUrl action when the copy button is " +
        "pressed", function() {
          var copyBtn = view.getDOMNode().querySelector(".btn-copy");

          React.addons.TestUtils.Simulate.click(copyBtn);

          sinon.assert.calledOnce(dispatcher.dispatch);
          sinon.assert.calledWith(dispatcher.dispatch,
            new sharedActions.CopyRoomUrl({roomUrl: "http://invalid"}));
        });

      it("should change the text when the url has been copied", function() {
          var copyBtn = view.getDOMNode().querySelector(".btn-copy");

          React.addons.TestUtils.Simulate.click(copyBtn);

          // copied_url_button is the l10n string.
          expect(copyBtn.textContent).eql("copied_url_button");
      });
    });

    describe("Share button", function() {
      beforeEach(function() {
        view = mountTestComponent();
      });

      it("should toggle the share dropdown when the share button is clicked", function() {
        var shareBtn = view.getDOMNode().querySelector(".btn-share");

        React.addons.TestUtils.Simulate.click(shareBtn);

        expect(view.state.showMenu).to.eql(true);
        expect(view.refs.menu.props.show).to.eql(true);
      });
    });

    describe("Context", function() {
      it("should not render the context data when told not to", function() {
        view = mountTestComponent();

        expect(view.getDOMNode().querySelector(".room-context")).to.eql(null);
      });

      it("should render context when data is available", function() {
        view = mountTestComponent({
          showContext: true,
          roomData: {
            roomContextUrls: [fakeContextURL]
          }
        });

        expect(view.getDOMNode().querySelector(".room-context")).to.not.eql(null);
      });

      it("should render the context in editMode when the pencil is clicked", function() {
        view = mountTestComponent({
          showContext: true,
          roomData: {
            roomContextUrls: [fakeContextURL]
          }
        });

        var pencil = view.getDOMNode().querySelector(".room-context-btn-edit");
        expect(pencil).to.not.eql(null);

        React.addons.TestUtils.Simulate.click(pencil);

        expect(view.state.editMode).to.eql(true);
        var node = view.getDOMNode();
        expect(node.querySelector("form")).to.not.eql(null);
        // No text paragraphs should be visible in editMode.
        var visiblePs = Array.slice(node.querySelector("p")).filter(function(p) {
          return p.classList.contains("hide") || p.classList.contains("error");
        });
        expect(visiblePs.length).to.eql(0);
      });

      it("should format the context url for display", function() {
        sandbox.stub(sharedUtils, "formatURL").returns({
          location: "location",
          hostname: "hostname"
        });

        view = mountTestComponent({
          showContext: true,
          roomData: {
            roomContextUrls: [fakeContextURL]
          }
        });

        expect(view.getDOMNode().querySelector(".room-context-url").textContent)
          .eql("hostname");
      });
    });
  });

  describe("DesktopRoomConversationView", function() {
    var view;

    beforeEach(function() {
      loop.store.StoreMixin.register({
        feedbackStore: new loop.store.FeedbackStore(dispatcher, {
          feedbackClient: {}
        })
      });
      sandbox.stub(dispatcher, "dispatch");
    });

    function mountTestComponent() {
      return TestUtils.renderIntoDocument(
        React.createElement(loop.roomViews.DesktopRoomConversationView, {
          dispatcher: dispatcher,
          roomStore: roomStore,
          mozLoop: fakeMozLoop
        }));
    }

    it("should dispatch a setMute action when the audio mute button is pressed",
      function() {
        view = mountTestComponent();

        view.setState({audioMuted: true});

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
        view = mountTestComponent();

        view.setState({videoMuted: false});

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
      view = mountTestComponent();

      view.setState({videoMuted: false});

      var muteBtn = view.getDOMNode().querySelector('.btn-mute-video');

      expect(muteBtn.classList.contains("muted")).eql(false);
    });

    it("should set the mute button as mute on", function() {
      view = mountTestComponent();

      view.setState({audioMuted: true});

      var muteBtn = view.getDOMNode().querySelector('.btn-mute-audio');

      expect(muteBtn.classList.contains("muted")).eql(true);
    });

    it("should dispatch a `StartScreenShare` action when sharing is not active " +
       "and the screen share button is pressed", function() {
      view = mountTestComponent();

      view.setState({screenSharingState: SCREEN_SHARE_STATES.INACTIVE});

      var muteBtn = view.getDOMNode().querySelector('.btn-mute-video');

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
      function expectActionDispatched(view) {
        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          sinon.match.instanceOf(sharedActions.SetupStreamElements));
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          sinon.match(function(value) {
            return value.getLocalElementFunc() ===
                   view.getDOMNode().querySelector(".local");
          }));
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          sinon.match(function(value) {
            return value.getRemoteElementFunc() ===
                   view.getDOMNode().querySelector(".remote");
          }));
      }

      it("should dispatch a `SetupStreamElements` action when the MEDIA_WAIT state " +
        "is entered", function() {
          activeRoomStore.setStoreState({roomState: ROOM_STATES.READY});
          var view = mountTestComponent();

          activeRoomStore.setStoreState({roomState: ROOM_STATES.MEDIA_WAIT});

          expectActionDispatched(view);
        });

      it("should dispatch a `SetupStreamElements` action on MEDIA_WAIT state is " +
        "re-entered", function() {
          activeRoomStore.setStoreState({roomState: ROOM_STATES.ENDED});
          var view = mountTestComponent();

          activeRoomStore.setStoreState({roomState: ROOM_STATES.MEDIA_WAIT});

          expectActionDispatched(view);
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

      it("should render the FeedbackView if roomState is `ENDED`",
        function() {
          activeRoomStore.setStoreState({
            roomState: ROOM_STATES.ENDED,
            used: true
          });

          view = mountTestComponent();

          TestUtils.findRenderedComponentWithType(view,
            loop.shared.views.FeedbackView);
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

      it("should show different contents when the Share XUL button is not available", function() {
        view = mountTestComponent({
          socialShareProviders: []
        });

        var node = view.getDOMNode();
        expect(node.querySelector(".share-panel-header")).to.not.eql(null);
      });

      it("should show an empty list when no Social Providers are available", function() {
        view = mountTestComponent({
          socialShareButtonAvailable: true,
          socialShareProviders: []
        });

        var node = view.getDOMNode();
        expect(node.querySelector(".icon-add-share-service")).to.not.eql(null);
        expect(node.querySelectorAll(".dropdown-menu-item").length).to.eql(1);
      });

      it("should show a list of available Social Providers", function() {
        view = mountTestComponent({
          socialShareButtonAvailable: true,
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

    describe("#handleToolbarAddButtonClick", function() {
      it("should dispatch an action when the 'add to toolbar' button is clicked", function() {
        view = mountTestComponent({
          socialShareProviders: []
        });

        var addButton = view.getDOMNode().querySelector(".btn-toolbar-add");
        React.addons.TestUtils.Simulate.click(addButton);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.AddSocialShareButton());
      });
    });

    describe("#handleAddServiceClick", function() {
      it("should dispatch an action when the 'add provider' item is clicked", function() {
        view = mountTestComponent({
          socialShareProviders: [],
          socialShareButtonAvailable: true
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
          socialShareButtonAvailable: true,
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

  describe("DesktopRoomContextView", function() {
    var view;

    afterEach(function() {
      view = null;
    });

    function mountTestComponent(props) {
      props = _.extend({
        dispatcher: dispatcher,
        mozLoop: fakeMozLoop,
        show: true,
        roomData: {
          roomToken: "fakeToken"
        }
      }, props);
      return TestUtils.renderIntoDocument(
        React.createElement(loop.roomViews.DesktopRoomContextView, props));
    }

    describe("#render", function() {
      it("should show the context information properly when available", function() {
        view = mountTestComponent({
          roomData: {
            roomDescription: "Hello, is it me you're looking for?",
            roomContextUrls: [fakeContextURL]
          }
        });

        var node = view.getDOMNode();
        expect(node).to.not.eql(null);
        expect(node.querySelector(".room-context-thumbnail").src).to.
          eql(fakeContextURL.thumbnail);
        expect(node.querySelector(".room-context-description").textContent).to.
          eql(fakeContextURL.description);
        expect(node.querySelector(".room-context-comment").textContent).to.
          eql(view.props.roomData.roomDescription);
      });

      it("should not render optional data", function() {
        view = mountTestComponent({
          roomData: { roomContextUrls: [fakeContextURL] }
        });

        expect(view.getDOMNode().querySelector(".room-context-comment")).to.
          eql(null);
      });

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

      it("should render the view in editMode when appropriate", function() {
        var roomName = "Hello, is it me you're looking for?";
        view = mountTestComponent({
          editMode: true,
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

      it("should show the checkbox as disabled when no context is available", function() {
        view = mountTestComponent({
          editMode: true,
          roomData: {
            roomToken: "fakeToken",
            roomName: "fakeName"
          }
        });

        var checkbox = view.getDOMNode().querySelector(".checkbox");
        expect(checkbox.classList.contains("disabled")).to.eql(true);
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

      it("should dispatch a UpdateRoomContext action when the focus is lost",
        function() {
          React.addons.TestUtils.Simulate.change(roomNameBox, { target: {
            value: "reallyFake"
          }});

          React.addons.TestUtils.Simulate.blur(roomNameBox);

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
    });

    describe("#handleCheckboxChange", function() {
      var node, checkbox;

      beforeEach(function() {
        view = mountTestComponent({
          availableContext: {
            description: fakeContextURL.description,
            previewImage: fakeContextURL.thumbnail,
            url: fakeContextURL.location
          },
          editMode: true,
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
  });
});
