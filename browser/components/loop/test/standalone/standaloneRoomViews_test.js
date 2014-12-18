/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop, sinon */

var expect = chai.expect;

describe("loop.standaloneRoomViews", function() {
  "use strict";

  var ROOM_STATES = loop.store.ROOM_STATES;
  var FEEDBACK_STATES = loop.store.FEEDBACK_STATES;
  var sharedActions = loop.shared.actions;

  var sandbox, dispatcher, activeRoomStore, feedbackStore, dispatch;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    dispatcher = new loop.Dispatcher();
    dispatch = sandbox.stub(dispatcher, "dispatch");
    activeRoomStore = new loop.store.ActiveRoomStore(dispatcher, {
      mozLoop: {},
      sdkDriver: {}
    });
    feedbackStore = new loop.store.FeedbackStore(dispatcher, {
      feedbackClient: {}
    });

    sandbox.useFakeTimers();

    // Prevents audio request errors in the test console.
    sandbox.useFakeXMLHttpRequest();
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("StandaloneRoomView", function() {
    function mountTestComponent() {
      return TestUtils.renderIntoDocument(
        loop.standaloneRoomViews.StandaloneRoomView({
          dispatcher: dispatcher,
          activeRoomStore: activeRoomStore,
          feedbackStore: feedbackStore,
          helper: new loop.shared.utils.Helper()
        }));
    }

    function expectActionDispatched(view) {
      sinon.assert.calledOnce(dispatch);
      sinon.assert.calledWithExactly(dispatch,
        sinon.match.instanceOf(sharedActions.SetupStreamElements));
      sinon.assert.calledWithExactly(dispatch, sinon.match(function(value) {
        return value.getLocalElementFunc() ===
               view.getDOMNode().querySelector(".local");
      }));
      sinon.assert.calledWithExactly(dispatch, sinon.match(function(value) {
        return value.getRemoteElementFunc() ===
               view.getDOMNode().querySelector(".remote");
      }));
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

      it("should updateVideoContainer when the JOINED state is entered", function() {
          activeRoomStore.setStoreState({roomState: ROOM_STATES.READY});

          var view = mountTestComponent();

          sandbox.stub(view, "updateVideoContainer");

          activeRoomStore.setStoreState({roomState: ROOM_STATES.JOINED});

          sinon.assert.calledOnce(view.updateVideoContainer);
      });

      it("should updateVideoContainer when the JOINED state is re-entered", function() {
          activeRoomStore.setStoreState({roomState: ROOM_STATES.ENDED});

          var view = mountTestComponent();

          sandbox.stub(view, "updateVideoContainer");

          activeRoomStore.setStoreState({roomState: ROOM_STATES.JOINED});

          sinon.assert.calledOnce(view.updateVideoContainer);
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
        it("should display a failed room message on FAILED",
          function() {
            activeRoomStore.setStoreState({roomState: ROOM_STATES.FAILED});

            expect(view.getDOMNode().querySelector(".failed-room-message"))
              .not.eql(null);
          });

        it("should display a retry button",
          function() {
            activeRoomStore.setStoreState({roomState: ROOM_STATES.FAILED});

            expect(view.getDOMNode().querySelector(".btn-info"))
              .not.eql(null);
          });
      });

      describe("Join button", function() {
        function getJoinButton(view) {
          return view.getDOMNode().querySelector(".btn-join");
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

      describe("Leave button", function() {
        function getLeaveButton(view) {
          return view.getDOMNode().querySelector(".btn-hangup");
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
          activeRoomStore.setStoreState({roomState: ROOM_STATES.ENDED});
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
      });

      describe("Mute", function() {
        it("should render local media as audio-only if video is muted",
          function() {
            activeRoomStore.setStoreState({
              roomState: ROOM_STATES.SESSION_CONNECTED,
              videoMuted: true
            });

            expect(view.getDOMNode().querySelector(".local-stream-audio"))
              .not.eql(null);
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
