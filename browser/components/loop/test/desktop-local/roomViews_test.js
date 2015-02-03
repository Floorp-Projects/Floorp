var expect = chai.expect;

/* jshint newcap:false */

describe("loop.roomViews", function () {
  "use strict";

  var ROOM_STATES = loop.store.ROOM_STATES;
  var SCREEN_SHARE_STATES = loop.shared.utils.SCREEN_SHARE_STATES;

  var sandbox, dispatcher, roomStore, activeRoomStore, fakeWindow;
  var fakeMozLoop;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();

    dispatcher = new loop.Dispatcher();

    fakeMozLoop = {
      getAudioBlob: sinon.stub(),
      getLoopPref: sinon.stub()
    };

    fakeWindow = {
      document: {},
      navigator: {
        mozLoop: fakeMozLoop
      },
      addEventListener: function() {},
      removeEventListener: function() {}
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

    function mountTestComponent() {
      return TestUtils.renderIntoDocument(
        React.createElement(
          loop.roomViews.DesktopRoomInvitationView, {
            dispatcher: dispatcher,
            roomStore: roomStore
          }));
    }

    it("should dispatch an EmailRoomUrl action when the email button is " +
      "pressed", function() {
        view = mountTestComponent();

        view.setState({roomUrl: "http://invalid"});

        var emailBtn = view.getDOMNode().querySelector('.btn-email');

        React.addons.TestUtils.Simulate.click(emailBtn);

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWith(dispatcher.dispatch,
          new sharedActions.EmailRoomUrl({roomUrl: "http://invalid"}));
      });

    describe("Rename Room", function() {
      var roomNameBox;

      beforeEach(function() {
        view = mountTestComponent();
        view.setState({
          roomToken: "fakeToken",
          roomName: "fakeName"
        });

        roomNameBox = view.getDOMNode().querySelector('.input-room-name');
      });

      it("should dispatch a RenameRoom action when the focus is lost",
        function() {
          React.addons.TestUtils.Simulate.change(roomNameBox, { target: {
            value: "reallyFake"
          }});

          React.addons.TestUtils.Simulate.blur(roomNameBox);

          sinon.assert.calledOnce(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.RenameRoom({
              roomToken: "fakeToken",
              newRoomName: "reallyFake"
            }));
        });

      it("should dispatch a RenameRoom action when Enter key is pressed",
        function() {
          React.addons.TestUtils.Simulate.change(roomNameBox, { target: {
            value: "reallyFake"
          }});

          TestUtils.Simulate.keyDown(roomNameBox, {key: "Enter", which: 13});

          sinon.assert.calledOnce(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.RenameRoom({
              roomToken: "fakeToken",
              newRoomName: "reallyFake"
            }));
        });
    });

    describe("Copy Button", function() {
      beforeEach(function() {
        view = mountTestComponent();

        view.setState({roomUrl: "http://invalid"});
      });

      it("should dispatch a CopyRoomUrl action when the copy button is " +
        "pressed", function() {
          var copyBtn = view.getDOMNode().querySelector('.btn-copy');

          React.addons.TestUtils.Simulate.click(copyBtn);

          sinon.assert.calledOnce(dispatcher.dispatch);
          sinon.assert.calledWith(dispatcher.dispatch,
            new sharedActions.CopyRoomUrl({roomUrl: "http://invalid"}));
        });

      it("should change the text when the url has been copied", function() {
          var copyBtn = view.getDOMNode().querySelector('.btn-copy');

          React.addons.TestUtils.Simulate.click(copyBtn);

          // copied_url_button is the l10n string.
          expect(copyBtn.textContent).eql("copied_url_button");
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

      it("should NOT render the FeedbackView if the room has not been used",
        function() {
          activeRoomStore.setStoreState({
            roomState: ROOM_STATES.ENDED,
            used: false
          });

          view = mountTestComponent();

          expect(view.getDOMNode()).eql(null);
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
});
