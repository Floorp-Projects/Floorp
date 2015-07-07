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

  var sandbox, dispatcher, activeRoomStore, feedbackStore, dispatch;

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
    feedbackStore = new loop.store.FeedbackStore(dispatcher, {
      feedbackClient: {}
    });
    loop.store.StoreMixin.register({
      activeRoomStore: activeRoomStore,
      feedbackStore: feedbackStore,
      textChatStore: textChatStore
    });

    sandbox.useFakeTimers();

    // Prevents audio request errors in the test console.
    sandbox.useFakeXMLHttpRequest();
  });

  afterEach(function() {
    sandbox.restore();
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
      var view = mountTestComponent();

      TestUtils.Simulate.click(view.getDOMNode().querySelector("a"));

      sinon.assert.calledOnce(dispatcher.dispatch);
      sinon.assert.calledWithExactly(dispatcher.dispatch,
        new sharedActions.RecordClick({
          linkInfo: "Support link click"
        }));
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

    function expectActionDispatched(view) {
      sinon.assert.calledOnce(dispatch);
      sinon.assert.calledWithExactly(dispatch,
        sinon.match.instanceOf(sharedActions.SetupStreamElements));
    }

    describe("#componentWillUpdate", function() {
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

    describe("#publishStream", function() {
      var view;

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
      var view;

      beforeEach(function() {
        view = mountTestComponent();
      });

      describe("Empty room message", function() {
        it("should display an empty room message on JOINED",
          function() {
            activeRoomStore.setStoreState({roomState: ROOM_STATES.JOINED});

            expect(view.getDOMNode().querySelector(".empty-room-message"))
              .not.eql(null);
          });

        it("should display an empty room message on SESSION_CONNECTED",
          function() {
            activeRoomStore.setStoreState({roomState: ROOM_STATES.SESSION_CONNECTED});

            expect(view.getDOMNode().querySelector(".empty-room-message"))
              .not.eql(null);
          });

        it("shouldn't display an empty room message on HAS_PARTICIPANTS",
          function() {
            activeRoomStore.setStoreState({roomState: ROOM_STATES.HAS_PARTICIPANTS});

            expect(view.getDOMNode().querySelector(".empty-room-message"))
              .eql(null);
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
        beforeEach(function() {
          activeRoomStore.setStoreState({ roomState: ROOM_STATES.FAILED });
        });

        it("should display a failed room message on FAILED", function() {
          expect(view.getDOMNode().querySelector(".failed-room-message"))
            .not.eql(null);
        });

        it("should display a retry button", function() {
          expect(view.getDOMNode().querySelector(".btn-info")).not.eql(null);
        });

        it("should not display a retry button when the failure reason is expired or invalid", function() {
          activeRoomStore.setStoreState({
            failureReason: FAILURE_DETAILS.EXPIRED_OR_INVALID
          });

          expect(view.getDOMNode().querySelector(".btn-info")).eql(null);
        });

        it("should dispatch a RetryAfterRoomFailure action when the retry button is pressed", function() {
          var button = view.getDOMNode().querySelector(".btn-info");

          TestUtils.Simulate.click(button);

          sinon.assert.calledOnce(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.RetryAfterRoomFailure());
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
           "but no screenShareVideoObject is present", function() {
          view.setState({
            "receivingScreenShare": true,
            "screenShareVideoObject": null
          });

          expect(view.getDOMNode().querySelector(".screen .loading-stream"))
              .not.eql(null);
        });

        it("should not show loading screen if receivingScreenShare is false " +
           "and screenShareVideoObject is null", function() {
             view.setState({
               "receivingScreenShare": false,
               "screenShareVideoObject": null
             });

             expect(view.getDOMNode().querySelector(".screen .loading-stream"))
                 .eql(null);
        });

        it("should not show a loading screen if screenShareVideoObject is set",
           function() {
             var videoElement = document.createElement("video");

             view.setState({
               "receivingScreenShare": true,
               "screenShareVideoObject": videoElement
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
            localSrcVideoObject: videoElement,
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

        it("should render local loading screen when no srcVideoObject",
           function() {
             activeRoomStore.setStoreState({
               roomState: ROOM_STATES.MEDIA_WAIT,
               remoteSrcVideoObject: null
             });

             expect(view.getDOMNode().querySelector(".local .loading-stream"))
                 .not.eql(null);
        });

        it("should not render local loading screen when srcVideoObject is set",
           function() {
             activeRoomStore.setStoreState({
               roomState: ROOM_STATES.MEDIA_WAIT,
               localSrcVideoObject: videoElement
             });

             expect(view.getDOMNode().querySelector(".local .loading-stream"))
                  .eql(null);
        });

        it("should not render remote loading screen when srcVideoObject is set",
           function() {
             activeRoomStore.setStoreState({
               roomState: ROOM_STATES.HAS_PARTICIPANTS,
               remoteSrcVideoObject: videoElement
             });

             expect(view.getDOMNode().querySelector(".remote .loading-stream"))
                  .eql(null);
        });

        it("should render remote video when the room HAS_PARTICIPANTS and" +
          " remoteVideoEnabled is true", function() {
          activeRoomStore.setStoreState({
            roomState: ROOM_STATES.HAS_PARTICIPANTS,
            remoteSrcVideoObject: videoElement,
            remoteVideoEnabled: true
          });

          expect(view.getDOMNode().querySelector(".remote video")).not.eql(null);
        });

        it("should render remote video when the room HAS_PARTICIPANTS and" +
          " remoteVideoEnabled is true", function() {
          activeRoomStore.setStoreState({
            roomState: ROOM_STATES.HAS_PARTICIPANTS,
            remoteSrcVideoObject: videoElement,
            remoteVideoEnabled: true
          });

          expect(view.getDOMNode().querySelector(".remote video")).not.eql(null);
        });

        it("should not render remote video when the room HAS_PARTICIPANTS," +
          " remoteVideoEnabled is false, and mediaConnected is true", function() {
          activeRoomStore.setStoreState({
            roomState: ROOM_STATES.HAS_PARTICIPANTS,
            remoteSrcVideoObject: videoElement,
            mediaConnected: true,
            remoteVideoEnabled: false
          });

          expect(view.getDOMNode().querySelector(".remote video")).eql(null);
        });

        it("should render remote video when the room HAS_PARTICIPANTS," +
          " and both remoteVideoEnabled and mediaConnected are false", function() {
          activeRoomStore.setStoreState({
            roomState: ROOM_STATES.HAS_PARTICIPANTS,
            remoteSrcVideoObject: videoElement,
            mediaConnected: false,
            remoteVideoEnabled: false
          });

          expect(view.getDOMNode().querySelector(".remote video")).not.eql(null);
        });

        it("should not render a remote avatar when the room is in MEDIA_WAIT", function() {
          activeRoomStore.setStoreState({
            roomState: ROOM_STATES.MEDIA_WAIT,
            remoteSrcVideoObject: videoElement,
            remoteVideoEnabled: false
          });

          expect(view.getDOMNode().querySelector(".remote .avatar")).eql(null);
        });

        it("should not render a remote avatar when the room is CLOSING and" +
          " remoteVideoEnabled is false", function() {
          activeRoomStore.setStoreState({
            roomState: ROOM_STATES.CLOSING,
            remoteSrcVideoObject: videoElement,
            remoteVideoEnabled: false
          });

          expect(view.getDOMNode().querySelector(".remote .avatar")).eql(null);
        });

        it("should render a remote avatar when the room HAS_PARTICIPANTS, " +
          "remoteVideoEnabled is false, and mediaConnected is true", function() {
          activeRoomStore.setStoreState({
            roomState: ROOM_STATES.HAS_PARTICIPANTS,
            remoteSrcVideoObject: videoElement,
            remoteVideoEnabled: false,
            mediaConnected: true
          });

          expect(view.getDOMNode().querySelector(".remote .avatar")).not.eql(null);
        });

        it("should render a remote avatar when the room HAS_PARTICIPANTS, " +
          "remoteSrcVideoObject is false, mediaConnected is true", function() {
          activeRoomStore.setStoreState({
            roomState: ROOM_STATES.HAS_PARTICIPANTS,
            remoteSrcVideoObject: false,
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

      describe("Feedback", function() {
        beforeEach(function() {
          activeRoomStore.setStoreState({
            roomState: ROOM_STATES.ENDED,
            used: true
          });
        });

        it("should display a feedback form when the user leaves the room",
          function() {
            expect(view.getDOMNode().querySelector(".faces")).not.eql(null);
          });

        it("should dispatch a `FeedbackComplete` action after feedback is sent",
          function() {
            feedbackStore.setStoreState({feedbackState: FEEDBACK_STATES.SENT});

            sandbox.clock.tick(
              loop.shared.views.WINDOW_AUTOCLOSE_TIMEOUT_IN_SECONDS * 1000 + 1000);

            sinon.assert.calledOnce(dispatch);
            sinon.assert.calledWithExactly(dispatch, new sharedActions.FeedbackComplete());
          });

        it("should NOT display a feedback form if the room has not been used",
          function() {
            activeRoomStore.setStoreState({used: false});
            expect(view.getDOMNode().querySelector(".faces")).eql(null);
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

            expect(view.getDOMNode().querySelector(".local .avatar")).
              not.eql(null);
          });
      });

      describe("Marketplace hidden iframe", function() {

        it("should set src when the store state change",
           function(done) {

          var marketplace = view.getDOMNode().querySelector("#marketplace");
          expect(marketplace.src).to.be.equal("");

          activeRoomStore.setStoreState({
            marketplaceSrc: "http://market/",
            onMarketplaceMessage: function () {}
          });

          view.forceUpdate(function() {
            expect(marketplace.src).to.be.equal("http://market/");
            done();
          });
        });
      });
    });
  });
});
