/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

describe("loop.standaloneRoomViews", function() {
  "use strict";

  var expect = chai.expect;
  var TestUtils = React.addons.TestUtils;

  var ROOM_STATES = loop.store.ROOM_STATES;
  var FEEDBACK_STATES = loop.store.FEEDBACK_STATES;
  var FAILURE_DETAILS = loop.shared.utils.FAILURE_DETAILS;
  var ROOM_INFO_FAILURES = loop.shared.utils.ROOM_INFO_FAILURES;
  var sharedActions = loop.shared.actions;
  var sharedUtils = loop.shared.utils;
  var fixtures = document.querySelector("#fixtures");

  var sandbox, dispatcher, activeRoomStore, dispatch;
  var clock, fakeWindow, view;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    dispatcher = new loop.Dispatcher();
    dispatch = sandbox.stub(dispatcher, "dispatch");
    activeRoomStore = new loop.store.ActiveRoomStore(dispatcher, {
      mozLoop: {},
      sdkDriver: {}
    });
    var textChatStore = new loop.store.TextChatStore(dispatcher, {
      sdkDriver: {}
    });
    loop.store.StoreMixin.register({
      activeRoomStore: activeRoomStore,
      textChatStore: textChatStore
    });

    clock = sandbox.useFakeTimers();
    fakeWindow = {
      close: sandbox.stub(),
      addEventListener: function() {},
      document: { addEventListener: function(){} },
      setTimeout: function(callback) { callback(); }
    };
    loop.shared.mixins.setRootObject(fakeWindow);


    sandbox.stub(navigator.mozL10n, "get", function(key, args) {
      switch(key) {
        case "standalone_title_with_room_name":
          return args.roomName + " — " + args.clientShortname;
        case "legal_text_and_links":
          return args.terms_of_use_url + " " + args.privacy_notice_url;
        default:
          return key;
      }
    });

    // Prevents audio request errors in the test console.
    sandbox.useFakeXMLHttpRequest();
  });

  afterEach(function() {
    loop.shared.mixins.setRootObject(window);
    sandbox.restore();
    clock.restore();
    React.unmountComponentAtNode(fixtures);
    view = null;
  });


  describe("TosView", function() {
    var origConfig, node;

    function mountTestComponent() {
      return TestUtils.renderIntoDocument(
        React.createElement(
          loop.standaloneRoomViews.ToSView, {
            dispatcher: dispatcher
          }));
    }

    beforeEach(function() {
      origConfig = loop.config;
      loop.config = {
        legalWebsiteUrl: "http://fakelegal/",
        privacyWebsiteUrl: "http://fakeprivacy/"
      };

      view = mountTestComponent();
      node = view.getDOMNode();
    });

    afterEach(function() {
      loop.config = origConfig;
    });

    it("should dispatch a link click action when the ToS link is clicked", function() {
      // [0] is the first link, the legal one.
      var link = node.querySelectorAll("a")[0];

      TestUtils.Simulate.click(node, { target: link });

      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWithExactly(dispatcher.dispatch,
        new sharedActions.RecordClick({
          linkInfo: loop.config.legalWebsiteUrl
        }));
    });

    it("should dispatch a link click action when the Privacy link is clicked", function() {
      // [0] is the first link, the legal one.
      var link = node.querySelectorAll("a")[1];

      TestUtils.Simulate.click(node, { target: link });

      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWithExactly(dispatcher.dispatch,
        new sharedActions.RecordClick({
          linkInfo: loop.config.privacyWebsiteUrl
        }));
    });

    it("should not dispatch an action when the text is clicked", function() {
      TestUtils.Simulate.click(node, { target: node });

      sinon.assert.notCalled(dispatcher.dispatch);
    });
  });

  describe("StandaloneHandleUserAgentView", function() {
    function mountTestComponent() {
      return TestUtils.renderIntoDocument(
        React.createElement(
          loop.standaloneRoomViews.StandaloneHandleUserAgentView, {
            dispatcher: dispatcher
          }));
    }

    it("should display a join room button if the state is not ROOM_JOINED", function() {
      activeRoomStore.setStoreState({
        roomState: ROOM_STATES.READY
      });

      view = mountTestComponent();
      var button = view.getDOMNode().querySelector(".info-panel > button");

      expect(button.textContent).eql("rooms_room_join_label");
    });

    it("should dispatch a JoinRoom action when the join room button is clicked", function() {
      activeRoomStore.setStoreState({
        roomState: ROOM_STATES.READY
      });

      view = mountTestComponent();
      var button = view.getDOMNode().querySelector(".info-panel > button");

      TestUtils.Simulate.click(button);

      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWithExactly(dispatcher.dispatch, new sharedActions.JoinRoom());
    });

    it("should display a enjoy your conversation button if the state is ROOM_JOINED", function() {
      activeRoomStore.setStoreState({
        roomState: ROOM_STATES.JOINED
      });

      view = mountTestComponent();
      var button = view.getDOMNode().querySelector(".info-panel > button");

      expect(button.textContent).eql("rooms_room_joined_own_conversation_label");
    });

    it("should disable the enjoy your conversation button if the state is ROOM_JOINED", function() {
      activeRoomStore.setStoreState({
        roomState: ROOM_STATES.JOINED
      });

      view = mountTestComponent();
      var button = view.getDOMNode().querySelector(".info-panel > button");

      expect(button.classList.contains("disabled")).eql(true);
    });

    it("should not display a join button if there is a failure reason", function() {
      activeRoomStore.setStoreState({
        failureReason: FAILURE_DETAILS.ROOM_ALREADY_OPEN
      });

      view = mountTestComponent();
      var button = view.getDOMNode().querySelector(".info-panel > button");

      expect(button).eql(null);
    });

    it("should display a room already joined message if opening failed", function() {
      activeRoomStore.setStoreState({
        failureReason: FAILURE_DETAILS.ROOM_ALREADY_OPEN
      });

      view = mountTestComponent();
      var text = view.getDOMNode().querySelector(".failure");

      expect(text.textContent).eql("rooms_already_joined");
    });
  });

  describe("StandaloneRoomHeader", function() {
    function mountTestComponent() {
      return TestUtils.renderIntoDocument(
        React.createElement(
          loop.standaloneRoomViews.StandaloneRoomHeader, {
            dispatcher: dispatcher
          }));
    }

    it("should dispatch a RecordClick action when the support link is clicked", function() {
      view = mountTestComponent();

      TestUtils.Simulate.click(view.getDOMNode().querySelector("a"));

      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWithExactly(dispatcher.dispatch,
        new sharedActions.RecordClick({
          linkInfo: "Support link click"
        }));
    });
  });

  describe("StandaloneRoomFailureView", function() {
    function mountTestComponent(extraProps) {
      var props = _.extend({
        dispatcher: dispatcher
      }, extraProps);
      return TestUtils.renderIntoDocument(
        React.createElement(
          loop.standaloneRoomViews.StandaloneRoomFailureView, props));
    }

    beforeEach(function() {
      activeRoomStore.setStoreState({ roomState: ROOM_STATES.FAILED });
    });

    it("should display a status error message if not reason is supplied", function() {
      view = mountTestComponent();

      expect(view.getDOMNode().querySelector(".failed-room-message").textContent)
        .eql("status_error");
    });

    it("should display a denied message on MEDIA_DENIED", function() {
      view = mountTestComponent({ failureReason: FAILURE_DETAILS.MEDIA_DENIED });

      expect(view.getDOMNode().querySelector(".failed-room-message").textContent)
        .eql("rooms_media_denied_message");
    });

    it("should display a denied message on NO_MEDIA", function() {
      view = mountTestComponent({ failureReason: FAILURE_DETAILS.NO_MEDIA });

      expect(view.getDOMNode().querySelector(".failed-room-message").textContent)
        .eql("rooms_media_denied_message");
    });

    it("should display an unavailable message on EXPIRED_OR_INVALID", function() {
      view = mountTestComponent({ failureReason: FAILURE_DETAILS.EXPIRED_OR_INVALID });

      expect(view.getDOMNode().querySelector(".failed-room-message").textContent)
        .eql("rooms_unavailable_notification_message");
    });

    it("should display an tos failure message on TOS_FAILURE", function() {
      view = mountTestComponent({ failureReason: FAILURE_DETAILS.TOS_FAILURE });

      expect(view.getDOMNode().querySelector(".failed-room-message").textContent)
        .eql("tos_failure_message");
    });

    it("should not display a retry button when the failure reason is expired or invalid", function() {
      view = mountTestComponent({ failureReason: FAILURE_DETAILS.EXPIRED_OR_INVALID });

      expect(view.getDOMNode().querySelector(".btn-info")).eql(null);
    });

    it("should not display a retry button when the failure reason is tos failure", function() {
      view = mountTestComponent({ failureReason: FAILURE_DETAILS.TOS_FAILURE });

      expect(view.getDOMNode().querySelector(".btn-info")).eql(null);
    });

    it("should display a retry button for any other reason", function() {
      view = mountTestComponent({ failureReason: FAILURE_DETAILS.NO_MEDIA });

      expect(view.getDOMNode().querySelector(".btn-info")).not.eql(null);
    });

    it("should dispatch a RetryAfterRoomFailure action when the retry button is pressed", function() {
      view = mountTestComponent({ failureReason: FAILURE_DETAILS.NO_MEDIA });

      var button = view.getDOMNode().querySelector(".btn-info");

      TestUtils.Simulate.click(button);

      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWithExactly(dispatcher.dispatch,
        new sharedActions.RetryAfterRoomFailure());
    });
  });

  describe("StandaloneRoomInfoArea in fixture", function() {
    it("should dispatch a RecordClick action when the tile is clicked", function(done) {
      // Point the iframe to a page that will auto-"click"
      loop.config.tilesIframeUrl = "data:text/html,<script>parent.postMessage('tile-click', '*');</script>";

      // Render the iframe into the fixture to cause it to load
      view = React.render(
        React.createElement(
          loop.standaloneRoomViews.StandaloneRoomInfoArea, {
            activeRoomStore: activeRoomStore,
            dispatcher: dispatcher,
            isFirefox: true,
            joinRoom: sandbox.stub(),
            roomState: ROOM_STATES.INIT,
            roomUsed: false
          }), fixtures);

      // Change states and move time to get the iframe to load
      view.setProps({ roomState: ROOM_STATES.JOINING });
      clock.tick(loop.standaloneRoomViews.StandaloneRoomInfoArea.RENDER_WAITING_DELAY);

      // Wait for the iframe to load and trigger a message that should also
      // cause the RecordClick action
      window.addEventListener("message", function onMessage() {
        window.removeEventListener("message", onMessage);

        sinon.assert.calledTwice(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.RecordClick({
            linkInfo: "Tiles iframe click"
          }));

        done();
      });
    });
  });

  describe("StandaloneRoomView", function() {
    function mountTestComponent() {
      return TestUtils.renderIntoDocument(
        React.createElement(
          loop.standaloneRoomViews.StandaloneRoomView, {
        dispatcher: dispatcher,
        activeRoomStore: activeRoomStore,
        isFirefox: true
      }));
    }

    function expectActionDispatched() {
      sinon.assert.calledOnce(dispatch);
      sinon.assert.calledWithExactly(dispatch,
        sinon.match.instanceOf(sharedActions.SetupStreamElements));
    }

    describe("#componentWillUpdate", function() {
      it("should set document.title to roomName and brand name when the READY state is dispatched", function() {
        activeRoomStore.setStoreState({roomName: "fakeName", roomState: ROOM_STATES.INIT});
        view = mountTestComponent();
        activeRoomStore.setStoreState({roomState: ROOM_STATES.READY});

        expect(fakeWindow.document.title).to.equal("fakeName — clientShortname2");
      });

      it("should set document.title brand name when there is no context available", function() {
        activeRoomStore.setStoreState({roomState: ROOM_STATES.INIT});
        view = mountTestComponent();
        activeRoomStore.setStoreState({roomState: ROOM_STATES.READY});

        expect(fakeWindow.document.title).to.equal("clientShortname2");
      });

      it("should dispatch a `SetupStreamElements` action when the MEDIA_WAIT state " +
        "is entered", function() {
          activeRoomStore.setStoreState({roomState: ROOM_STATES.READY});
          view = mountTestComponent();

          activeRoomStore.setStoreState({roomState: ROOM_STATES.MEDIA_WAIT});

          expectActionDispatched(view);
        });

      it("should dispatch a `SetupStreamElements` action on MEDIA_WAIT state is " +
        "re-entered", function() {
          activeRoomStore.setStoreState({roomState: ROOM_STATES.ENDED});
          view = mountTestComponent();

          activeRoomStore.setStoreState({roomState: ROOM_STATES.MEDIA_WAIT});

          expectActionDispatched(view);
        });
    });

    describe("#componentDidUpdate", function() {
      beforeEach(function() {
        view = mountTestComponent();
        activeRoomStore.setStoreState({roomState: ROOM_STATES.JOINING});
        activeRoomStore.setStoreState({roomState: ROOM_STATES.JOINED});
      });

      it("should not dispatch a `TileShown` action immediately in the JOINED state",
        function() {
          sinon.assert.notCalled(dispatch);
        });

      it("should dispatch a `TileShown` action after a wait when in the JOINED state",
        function() {
          clock.tick(loop.standaloneRoomViews.StandaloneRoomInfoArea.RENDER_WAITING_DELAY);

          sinon.assert.calledOnce(dispatch);
          sinon.assert.calledWithExactly(dispatch, new sharedActions.TileShown());
        });

      it("should dispatch a single `TileShown` action after a wait when going through multiple waiting states",
        function() {
          activeRoomStore.setStoreState({roomState: ROOM_STATES.SESSION_CONNECTED});
          clock.tick(loop.standaloneRoomViews.StandaloneRoomInfoArea.RENDER_WAITING_DELAY);

          sinon.assert.calledOnce(dispatch);
          sinon.assert.calledWithExactly(dispatch, new sharedActions.TileShown());
        });

      it("should not dispatch a `TileShown` action after a wait when in the HAS_PARTICIPANTS state",
        function() {
          activeRoomStore.setStoreState({roomState: ROOM_STATES.HAS_PARTICIPANTS});
          clock.tick(loop.standaloneRoomViews.StandaloneRoomInfoArea.RENDER_WAITING_DELAY);

          sinon.assert.notCalled(dispatch);
        });

      it("should dispatch a `TileShown` action after a wait when a participant leaves",
        function() {
          activeRoomStore.setStoreState({roomState: ROOM_STATES.HAS_PARTICIPANTS});
          clock.tick(loop.standaloneRoomViews.StandaloneRoomInfoArea.RENDER_WAITING_DELAY);
          activeRoomStore.remotePeerDisconnected();
          clock.tick(loop.standaloneRoomViews.StandaloneRoomInfoArea.RENDER_WAITING_DELAY);

          sinon.assert.calledOnce(dispatch);
          sinon.assert.calledWithExactly(dispatch, new sharedActions.TileShown());
        });
    });

    describe("#componentWillReceiveProps", function() {
      beforeEach(function() {
        view = mountTestComponent();

        // Pretend the user waited a little bit
        activeRoomStore.setStoreState({roomState: ROOM_STATES.JOINING});
        clock.tick(loop.standaloneRoomViews.StandaloneRoomInfoArea.RENDER_WAITING_DELAY - 1);
      });

      describe("Support multiple joins", function() {
        it("should send the first `TileShown` after waiting in JOINING state",
          function() {
            clock.tick(1);

            sinon.assert.calledOnce(dispatch);
            sinon.assert.calledWithExactly(dispatch, new sharedActions.TileShown());
          });

        it("should send the second `TileShown` after ending and rejoining",
          function() {
            // Trigger the first message then rejoin and wait
            clock.tick(1);
            activeRoomStore.setStoreState({roomState: ROOM_STATES.ENDED});
            activeRoomStore.setStoreState({roomState: ROOM_STATES.JOINING});
            clock.tick(loop.standaloneRoomViews.StandaloneRoomInfoArea.RENDER_WAITING_DELAY);

            sinon.assert.calledTwice(dispatch);
            sinon.assert.calledWithExactly(dispatch, new sharedActions.TileShown());
          });
      });

      describe("Handle leaving quickly", function() {
        beforeEach(function() {
          // The user left and rejoined
          activeRoomStore.setStoreState({roomState: ROOM_STATES.ENDED});
          activeRoomStore.setStoreState({roomState: ROOM_STATES.JOINING});
        });

        it("should not dispatch an old `TileShown` action after leaving and rejoining",
          function() {
            clock.tick(1);

            sinon.assert.notCalled(dispatch);
          });

        it("should dispatch a new `TileShown` action after leaving and rejoining and waiting",
          function() {
            clock.tick(loop.standaloneRoomViews.StandaloneRoomInfoArea.RENDER_WAITING_DELAY);

            sinon.assert.calledOnce(dispatch);
            sinon.assert.calledWithExactly(dispatch, new sharedActions.TileShown());
          });
      });
    });

    describe("#publishStream", function() {
      beforeEach(function() {
        view = mountTestComponent();
        view.setState({
          audioMuted: true,
          videoMuted: true
        });
      });

      it("should mute local audio stream", function() {
        TestUtils.Simulate.click(
          view.getDOMNode().querySelector(".btn-mute-audio"));

        sinon.assert.calledOnce(dispatch);
        sinon.assert.calledWithExactly(dispatch, new sharedActions.SetMute({
          type: "audio",
          enabled: true
        }));
      });

      it("should mute local video stream", function() {
        TestUtils.Simulate.click(
          view.getDOMNode().querySelector(".btn-mute-video"));

        sinon.assert.calledOnce(dispatch);
        sinon.assert.calledWithExactly(dispatch, new sharedActions.SetMute({
          type: "video",
          enabled: true
        }));
      });
    });

    describe("#render", function() {
      beforeEach(function() {
        view = mountTestComponent();
        activeRoomStore.setStoreState({roomState: ROOM_STATES.JOINING});
      });

      describe("Empty room message", function() {
        it("should not display an message immediately in the JOINED state",
          function() {
            activeRoomStore.setStoreState({roomState: ROOM_STATES.JOINED});

            expect(view.getDOMNode().querySelector(".empty-room-message"))
              .eql(null);
          });

        it("should display an empty room message after a wait when in the JOINED state",
          function() {
            activeRoomStore.setStoreState({roomState: ROOM_STATES.JOINED});
            clock.tick(loop.standaloneRoomViews.StandaloneRoomInfoArea.RENDER_WAITING_DELAY);

            expect(view.getDOMNode().querySelector(".empty-room-message"))
              .not.eql(null);
          });

        it("should not display an message immediately in the SESSION_CONNECTED state",
          function() {
            activeRoomStore.setStoreState({roomState: ROOM_STATES.SESSION_CONNECTED});

            expect(view.getDOMNode().querySelector(".empty-room-message"))
              .eql(null);
          });

        it("should display an empty room message after a wait when in the SESSION_CONNECTED state",
          function() {
            activeRoomStore.setStoreState({roomState: ROOM_STATES.SESSION_CONNECTED});
            clock.tick(loop.standaloneRoomViews.StandaloneRoomInfoArea.RENDER_WAITING_DELAY);

            expect(view.getDOMNode().querySelector(".empty-room-message"))
              .not.eql(null);
          });

        it("should not display an message immediately in the HAS_PARTICIPANTS state",
          function() {
            activeRoomStore.setStoreState({roomState: ROOM_STATES.HAS_PARTICIPANTS});

            expect(view.getDOMNode().querySelector(".empty-room-message"))
              .eql(null);
          });

        it("should not display an empty room message even after a wait when in the HAS_PARTICIPANTS state",
          function() {
            activeRoomStore.setStoreState({roomState: ROOM_STATES.HAS_PARTICIPANTS});
            clock.tick(loop.standaloneRoomViews.StandaloneRoomInfoArea.RENDER_WAITING_DELAY);

            expect(view.getDOMNode().querySelector(".empty-room-message"))
              .eql(null);
          });
      });

      describe("Empty room tile offer", function() {
        it("should display a waiting room message and tile iframe on JOINED", function() {
          var DUMMY_TILE_URL = "http://tile/";
          loop.config.tilesIframeUrl = DUMMY_TILE_URL;
          activeRoomStore.setStoreState({roomState: ROOM_STATES.JOINED});
          clock.tick(loop.standaloneRoomViews.StandaloneRoomInfoArea.RENDER_WAITING_DELAY);

          expect(view.getDOMNode().querySelector(".room-waiting-area")).not.eql(null);

          var tile = view.getDOMNode().querySelector(".room-waiting-tile");
          expect(tile).not.eql(null);
          expect(tile.src).eql(DUMMY_TILE_URL);
        });

        it("should dispatch a RecordClick action when the tile support link is clicked", function() {
          activeRoomStore.setStoreState({roomState: ROOM_STATES.JOINED});
          clock.tick(loop.standaloneRoomViews.StandaloneRoomInfoArea.RENDER_WAITING_DELAY);

          TestUtils.Simulate.click(view.getDOMNode().querySelector(".room-waiting-area a"));

          sinon.assert.calledTwice(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.RecordClick({
              linkInfo: "Tiles support link click"
            }));
        });

      });

      describe("Prompt media message", function() {
        it("should display a prompt for user media on MEDIA_WAIT",
          function() {
            activeRoomStore.setStoreState({roomState: ROOM_STATES.MEDIA_WAIT});

            expect(view.getDOMNode().querySelector(".prompt-media-message"))
              .not.eql(null);
          });
      });

      describe("Full room message", function() {
        it("should display a full room message on FULL",
          function() {
            activeRoomStore.setStoreState({roomState: ROOM_STATES.FULL});

            expect(view.getDOMNode().querySelector(".full-room-message"))
              .not.eql(null);
          });
      });

      describe("Failed room message", function() {
        it("should display the StandaloneRoomFailureView", function() {
          activeRoomStore.setStoreState({ roomState: ROOM_STATES.FAILED });

          TestUtils.findRenderedComponentWithType(view,
            loop.standaloneRoomViews.StandaloneRoomFailureView);
        });
      });

      describe("Join button", function() {
        function getJoinButton(elem) {
          return elem.getDOMNode().querySelector(".btn-join");
        }

        it("should render the Join button when room isn't active", function() {
          activeRoomStore.setStoreState({roomState: ROOM_STATES.READY});

          expect(getJoinButton(view)).not.eql(null);
        });

        it("should not render the Join button when room is active",
          function() {
            activeRoomStore.setStoreState({roomState: ROOM_STATES.SESSION_CONNECTED});

            expect(getJoinButton(view)).eql(null);
          });

        it("should join the room when clicking the Join button", function() {
          activeRoomStore.setStoreState({roomState: ROOM_STATES.READY});

          TestUtils.Simulate.click(getJoinButton(view));

          sinon.assert.calledOnce(dispatch);
          sinon.assert.calledWithExactly(dispatch, new sharedActions.JoinRoom());
        });
      });

      describe("screenShare", function() {
        it("should show a loading screen if receivingScreenShare is true " +
           "but no screenShareMediaElement is present", function() {
          view.setState({
            "receivingScreenShare": true,
            "screenShareMediaElement": null
          });

          expect(view.getDOMNode().querySelector(".screen .loading-stream"))
              .not.eql(null);
        });

        it("should not show loading screen if receivingScreenShare is false " +
           "and screenShareMediaElement is null", function() {
             view.setState({
               "receivingScreenShare": false,
               "screenShareMediaElement": null
             });

             expect(view.getDOMNode().querySelector(".screen .loading-stream"))
                 .eql(null);
        });

        it("should not show a loading screen if screenShareMediaElement is set",
           function() {
             var videoElement = document.createElement("video");

             view.setState({
               "receivingScreenShare": true,
               "screenShareMediaElement": videoElement
             });

             expect(view.getDOMNode().querySelector(".screen .loading-stream"))
                 .eql(null);
        });
      });

      describe("Participants", function() {
        var videoElement;

        beforeEach(function() {
          videoElement = document.createElement("video");
        });

        it("should render local video when video_muted is false", function() {
          activeRoomStore.setStoreState({
            roomState: ROOM_STATES.HAS_PARTICIPANTS,
            localSrcMediaElement: videoElement,
            videoMuted: false
          });

          expect(view.getDOMNode().querySelector(".local video")).not.eql(null);
        });

        it("should not render a local avatar when video_muted is false", function() {
          activeRoomStore.setStoreState({
            roomState: ROOM_STATES.HAS_PARTICIPANTS,
            videoMuted: false
          });

          expect(view.getDOMNode().querySelector(".local .avatar")).eql(null);
        });

        it("should render local loading screen when no srcMediaElement",
           function() {
             activeRoomStore.setStoreState({
               roomState: ROOM_STATES.MEDIA_WAIT,
               remoteSrcMediaElement: null
             });

             expect(view.getDOMNode().querySelector(".local .loading-stream"))
                 .not.eql(null);
        });

        it("should not render local loading screen when srcMediaElement is set",
           function() {
             activeRoomStore.setStoreState({
               roomState: ROOM_STATES.MEDIA_WAIT,
               localSrcMediaElement: videoElement
             });

             expect(view.getDOMNode().querySelector(".local .loading-stream"))
                  .eql(null);
        });

        it("should not render remote loading screen when srcMediaElement is set",
           function() {
             activeRoomStore.setStoreState({
               roomState: ROOM_STATES.HAS_PARTICIPANTS,
               remoteSrcMediaElement: videoElement
             });

             expect(view.getDOMNode().querySelector(".remote .loading-stream"))
                  .eql(null);
        });

        it("should render remote video when the room HAS_PARTICIPANTS and" +
          " remoteVideoEnabled is true", function() {
          activeRoomStore.setStoreState({
            roomState: ROOM_STATES.HAS_PARTICIPANTS,
            remoteSrcMediaElement: videoElement,
            remoteVideoEnabled: true
          });

          expect(view.getDOMNode().querySelector(".remote video")).not.eql(null);
        });

        it("should render remote video when the room HAS_PARTICIPANTS and" +
          " remoteVideoEnabled is true", function() {
          activeRoomStore.setStoreState({
            roomState: ROOM_STATES.HAS_PARTICIPANTS,
            remoteSrcMediaElement: videoElement,
            remoteVideoEnabled: true
          });

          expect(view.getDOMNode().querySelector(".remote video")).not.eql(null);
        });

        it("should not render remote video when the room HAS_PARTICIPANTS," +
          " remoteVideoEnabled is false, and mediaConnected is true", function() {
          activeRoomStore.setStoreState({
            roomState: ROOM_STATES.HAS_PARTICIPANTS,
            remoteSrcMediaElement: videoElement,
            mediaConnected: true,
            remoteVideoEnabled: false
          });

          expect(view.getDOMNode().querySelector(".remote video")).eql(null);
        });

        it("should render remote video when the room HAS_PARTICIPANTS," +
          " and both remoteVideoEnabled and mediaConnected are false", function() {
          activeRoomStore.setStoreState({
            roomState: ROOM_STATES.HAS_PARTICIPANTS,
            remoteSrcMediaElement: videoElement,
            mediaConnected: false,
            remoteVideoEnabled: false
          });

          expect(view.getDOMNode().querySelector(".remote video")).not.eql(null);
        });

        it("should not render a remote avatar when the room is in MEDIA_WAIT", function() {
          activeRoomStore.setStoreState({
            roomState: ROOM_STATES.MEDIA_WAIT,
            remoteSrcMediaElement: videoElement,
            remoteVideoEnabled: false
          });

          expect(view.getDOMNode().querySelector(".remote .avatar")).eql(null);
        });

        it("should not render a remote avatar when the room is CLOSING and" +
          " remoteVideoEnabled is false", function() {
          activeRoomStore.setStoreState({
            roomState: ROOM_STATES.CLOSING,
            remoteSrcMediaElement: videoElement,
            remoteVideoEnabled: false
          });

          expect(view.getDOMNode().querySelector(".remote .avatar")).eql(null);
        });

        it("should render a remote avatar when the room HAS_PARTICIPANTS, " +
          "remoteVideoEnabled is false, and mediaConnected is true", function() {
          activeRoomStore.setStoreState({
            roomState: ROOM_STATES.HAS_PARTICIPANTS,
            remoteSrcMediaElement: videoElement,
            remoteVideoEnabled: false,
            mediaConnected: true
          });

          expect(view.getDOMNode().querySelector(".remote .avatar")).not.eql(null);
        });

        it("should render a remote avatar when the room HAS_PARTICIPANTS, " +
          "remoteSrcMediaElement is false, mediaConnected is true", function() {
          activeRoomStore.setStoreState({
            roomState: ROOM_STATES.HAS_PARTICIPANTS,
            remoteSrcMediaElement: null,
            remoteVideoEnabled: false,
            mediaConnected: true
          });

          expect(view.getDOMNode().querySelector(".remote .avatar")).not.eql(null);
        });
      });

      describe("Leave button", function() {
        function getLeaveButton(elem) {
          return elem.getDOMNode().querySelector(".btn-hangup");
        }

        it("should disable the Leave button when the room state is READY",
          function() {
            activeRoomStore.setStoreState({roomState: ROOM_STATES.READY});

            expect(getLeaveButton(view).disabled).eql(true);
          });

        it("should disable the Leave button when the room state is FAILED",
          function() {
            activeRoomStore.setStoreState({roomState: ROOM_STATES.FAILED});

            expect(getLeaveButton(view).disabled).eql(true);
          });

        it("should disable the Leave button when the room state is FULL",
          function() {
            activeRoomStore.setStoreState({roomState: ROOM_STATES.FULL});

            expect(getLeaveButton(view).disabled).eql(true);
          });

        it("should enable the Leave button when the room state is SESSION_CONNECTED",
          function() {
            activeRoomStore.setStoreState({roomState: ROOM_STATES.SESSION_CONNECTED});

            expect(getLeaveButton(view).disabled).eql(false);
          });

        it("should enable the Leave button when the room state is JOINED",
          function() {
            activeRoomStore.setStoreState({roomState: ROOM_STATES.JOINED});

            expect(getLeaveButton(view).disabled).eql(false);
          });

        it("should enable the Leave button when the room state is HAS_PARTICIPANTS",
          function() {
            activeRoomStore.setStoreState({roomState: ROOM_STATES.HAS_PARTICIPANTS});

            expect(getLeaveButton(view).disabled).eql(false);
          });

        it("should leave the room when clicking the Leave button", function() {
          activeRoomStore.setStoreState({roomState: ROOM_STATES.HAS_PARTICIPANTS});

          TestUtils.Simulate.click(getLeaveButton(view));

          sinon.assert.calledOnce(dispatch);
          sinon.assert.calledWithExactly(dispatch, new sharedActions.LeaveRoom());
        });
      });

      describe("Mute", function() {
        it("should render a local avatar if video is muted",
          function() {
            activeRoomStore.setStoreState({
              roomState: ROOM_STATES.SESSION_CONNECTED,
              videoMuted: true
            });

            expect(view.getDOMNode().querySelector(".local .avatar"))
              .not.eql(null);
          });

        it("should render a local avatar if the room HAS_PARTICIPANTS and" +
          " .videoMuted is true",
          function() {
            activeRoomStore.setStoreState({
              roomState: ROOM_STATES.HAS_PARTICIPANTS,
              videoMuted: true
            });

            expect(view.getDOMNode().querySelector(".local .avatar")).not.eql(
              null);
          });
      });
    });
  });

  describe("StandaloneRoomControllerView", function() {
    function mountTestComponent() {
      return TestUtils.renderIntoDocument(
        React.createElement(
          loop.standaloneRoomViews.StandaloneRoomControllerView, {
        dispatcher: dispatcher,
        isFirefox: true
      }));
    }

    it("should not display anything if it is not known if Firefox can handle the room", function() {
      activeRoomStore.setStoreState({
        userAgentHandlesRoom: undefined
      });

      view = mountTestComponent();

      expect(view.getDOMNode()).eql(null);
    });

    it("should render StandaloneHandleUserAgentView if Firefox can handle the room", function() {
      activeRoomStore.setStoreState({
        userAgentHandlesRoom: true
      });

      view = mountTestComponent();

      TestUtils.findRenderedComponentWithType(view,
        loop.standaloneRoomViews.StandaloneHandleUserAgentView);
    });

    it("should render StandaloneRoomView if Firefox cannot handle the room", function() {
      activeRoomStore.setStoreState({
        userAgentHandlesRoom: false
      });

      view = mountTestComponent();

      TestUtils.findRenderedComponentWithType(view,
        loop.standaloneRoomViews.StandaloneRoomView);
    });
  });
});
