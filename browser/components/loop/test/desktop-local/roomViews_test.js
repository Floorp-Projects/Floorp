var expect = chai.expect;

/* jshint newcap:false */

describe("loop.roomViews", function () {
  "use strict";

  var ROOM_STATES = loop.store.ROOM_STATES;

  var sandbox, dispatcher, roomStore, activeRoomStore, fakeWindow;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();

    dispatcher = new loop.Dispatcher();

    fakeWindow = {
      document: {},
      navigator: {
        mozLoop: {
          getAudioBlob: sinon.stub()
        }
      }
    };
    loop.shared.mixins.setRootObject(fakeWindow);

    // XXX These stubs should be hoisted in a common file
    // Bug 1040968
    sandbox.stub(document.mozL10n, "get", function(x) {
      return x;
    });

    activeRoomStore = new loop.store.ActiveRoomStore({
      dispatcher: dispatcher,
      mozLoop: {},
      sdkDriver: {}
    });
    roomStore = new loop.store.RoomStore({
      dispatcher: dispatcher,
      mozLoop: {},
      activeRoomStore: activeRoomStore
    });
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

      var testView = TestUtils.renderIntoDocument(TestView({
        roomStore: activeRoomStore
      }));

      expect(testView.state).eql({
        roomState: ROOM_STATES.INIT,
        audioMuted: false,
        videoMuted: false,
        foo: "bar"
      });
    });

    it("should listen to store changes", function() {
      var TestView = React.createClass({
        mixins: [loop.roomViews.ActiveRoomStoreMixin],
        render: function() { return React.DOM.div(); }
      });
      var testView = TestUtils.renderIntoDocument(TestView({
        roomStore: activeRoomStore
      }));

      activeRoomStore.setStoreState({roomState: ROOM_STATES.READY});

      expect(testView.state.roomState).eql(ROOM_STATES.READY);
    });
  });

  describe("DesktopRoomConversationView", function() {
    var view;

    beforeEach(function() {
      sandbox.stub(dispatcher, "dispatch");
    });

    function mountTestComponent() {
      return TestUtils.renderIntoDocument(
        new loop.roomViews.DesktopRoomConversationView({
          dispatcher: dispatcher,
          roomStore: roomStore
        }));
    }

    it("should dispatch a setupStreamElements action when the view is created",
      function() {
        view = mountTestComponent();

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithMatch(dispatcher.dispatch,
          sinon.match.hasOwn("name", "setupStreamElements"));
    });

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
            loop.conversation.GenericFailureView);
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
    });
  });
});
