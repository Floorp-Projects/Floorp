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
    loop.store.StoreMixin.register({feedbackStore: feedbackStore});

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

    describe("Local Stream Size Position", function() {
      var view, localElement;

      beforeEach(function() {
        sandbox.stub(window, "matchMedia").returns({
          matches: false
        });
        view = mountTestComponent();
        localElement = view._getElement(".local");
      });

      it("should be a quarter of the width of the main stream", function() {
        sandbox.stub(view, "getRemoteVideoDimensions").returns({
          streamWidth: 640,
          offsetX: 0
        });

        view.updateLocalCameraPosition({
          width: 1,
          height: 0.75
        });

        expect(localElement.style.width).eql("160px");
        expect(localElement.style.height).eql("120px");
      });

      it("should be a quarter of the width reduced for aspect ratio", function() {
        sandbox.stub(view, "getRemoteVideoDimensions").returns({
          streamWidth: 640,
          offsetX: 0
        });

        view.updateLocalCameraPosition({
          width: 0.75,
          height: 1
        });

        expect(localElement.style.width).eql("120px");
        expect(localElement.style.height).eql("160px");
      });

      it("should ensure the height is a minimum of 48px", function() {
        sandbox.stub(view, "getRemoteVideoDimensions").returns({
          streamWidth: 180,
          offsetX: 0
        });

        view.updateLocalCameraPosition({
          width: 1,
          height: 0.75
        });

        expect(localElement.style.width).eql("64px");
        expect(localElement.style.height).eql("48px");
      });

      it("should ensure the width is a minimum of 48px", function() {
        sandbox.stub(view, "getRemoteVideoDimensions").returns({
          streamWidth: 180,
          offsetX: 0
        });

        view.updateLocalCameraPosition({
          width: 0.75,
          height: 1
        });

        expect(localElement.style.width).eql("48px");
        expect(localElement.style.height).eql("64px");
      });

      it("should position the stream to overlap the main stream by a quarter", function() {
        sandbox.stub(view, "getRemoteVideoDimensions").returns({
          streamWidth: 640,
          offsetX: 0
        });

        view.updateLocalCameraPosition({
          width: 1,
          height: 0.75
        });

        expect(localElement.style.width).eql("160px");
        expect(localElement.style.left).eql("600px");
      });

      it("should position the stream to overlap the main stream by a quarter when the aspect ratio is vertical", function() {
        sandbox.stub(view, "getRemoteVideoDimensions").returns({
          streamWidth: 640,
          offsetX: 0
        });

        view.updateLocalCameraPosition({
          width: 0.75,
          height: 1
        });

        expect(localElement.style.width).eql("120px");
        expect(localElement.style.left).eql("610px");
      });
    });

    describe("Remote Stream Size Position", function() {
      var view, localElement, remoteElement;

      beforeEach(function() {
        sandbox.stub(window, "matchMedia").returns({
          matches: false
        });
        view = mountTestComponent();

        localElement = {
          style: {}
        };
        remoteElement = {
          style: {},
          removeAttribute: sinon.spy()
        };

        sandbox.stub(view, "_getElement", function(className) {
          return className === ".local" ? localElement : remoteElement;
        });

        view.setState({"receivingScreenShare": true});
      });

      it("should do nothing if not receiving screenshare", function() {
        view.setState({"receivingScreenShare": false});
        remoteElement.style.width = "10px";

        view.updateRemoteCameraPosition({
          width: 1,
          height: 0.75
        });

        expect(remoteElement.style.width).eql("10px");
      });

      it("should be the same width as the local video", function() {
        localElement.offsetWidth = 100;

        view.updateRemoteCameraPosition({
          width: 1,
          height: 0.75
        });

        expect(remoteElement.style.width).eql("100px");
      });

      it("should be the same left edge as the local video", function() {
        localElement.offsetLeft = 50;

        view.updateRemoteCameraPosition({
          width: 1,
          height: 0.75
        });

        expect(remoteElement.style.left).eql("50px");
      });

      it("should have a height determined by the aspect ratio", function() {
        localElement.offsetWidth = 100;

        view.updateRemoteCameraPosition({
          width: 1,
          height: 0.75
        });

        expect(remoteElement.style.height).eql("75px");
      });

      it("should have the top be set such that the bottom is 10px above the local video", function() {
        localElement.offsetWidth = 100;
        localElement.offsetTop = 200;

        view.updateRemoteCameraPosition({
          width: 1,
          height: 0.75
        });

        // 200 (top) - 75 (height) - 10 (spacing) = 115
        expect(remoteElement.style.top).eql("115px");
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
