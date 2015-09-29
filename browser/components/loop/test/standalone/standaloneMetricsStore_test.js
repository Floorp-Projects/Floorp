/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

describe("loop.store.StandaloneMetricsStore", function() {
  "use strict";

  var expect = chai.expect;
  var sandbox, dispatcher, store, fakeActiveRoomStore;

  var sharedActions = loop.shared.actions;
  var METRICS_GA_CATEGORY = loop.store.METRICS_GA_CATEGORY;
  var METRICS_GA_ACTIONS = loop.store.METRICS_GA_ACTIONS;
  var ROOM_STATES = loop.store.ROOM_STATES;
  var FAILURE_DETAILS = loop.shared.utils.FAILURE_DETAILS;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    dispatcher = new loop.Dispatcher();

    window.ga = sinon.stub();

    var fakeStore = loop.store.createStore({
      getInitialStoreState: function() {
        return {
          audioMuted: false,
          roomState: ROOM_STATES.INIT,
          videoMuted: false
        };
      }
    });

    fakeActiveRoomStore = new fakeStore(dispatcher);

    store = new loop.store.StandaloneMetricsStore(dispatcher, {
      activeRoomStore: fakeActiveRoomStore
    });
  });

  afterEach(function() {
    sandbox.restore();
    delete window.ga;
  });

  describe("Action Handlers", function() {
    beforeEach(function() {
    });

    it("should log an event on ConnectedToSdkServers", function() {
      store.connectedToSdkServers();

      sinon.assert.calledOnce(window.ga);
      sinon.assert.calledWithExactly(window.ga,
        "send", "event", METRICS_GA_CATEGORY.general, METRICS_GA_ACTIONS.success,
        "Joined sdk servers");
    });

    it("should log an event on connection failure if media denied", function() {
      store.connectionFailure(new sharedActions.ConnectionFailure({
        reason: FAILURE_DETAILS.MEDIA_DENIED
      }));

      sinon.assert.calledOnce(window.ga);
      sinon.assert.calledWithExactly(window.ga,
        "send", "event", METRICS_GA_CATEGORY.general, METRICS_GA_ACTIONS.failed,
        "Media denied");
    });

    it("should log an event on connection failure if no media was found", function() {
      store.connectionFailure(new sharedActions.ConnectionFailure({
        reason: FAILURE_DETAILS.NO_MEDIA
      }));

      sinon.assert.calledOnce(window.ga);
      sinon.assert.calledWithExactly(window.ga,
        "send", "event", METRICS_GA_CATEGORY.general, METRICS_GA_ACTIONS.failed,
        "No media");
    });

    it("should log an event on connection failure if the room was already open", function() {
      store.connectionFailure(new sharedActions.ConnectionFailure({
        reason: FAILURE_DETAILS.ROOM_ALREADY_OPEN
      }));

      sinon.assert.calledOnce(window.ga);
      sinon.assert.calledWithExactly(window.ga,
        "send", "event", METRICS_GA_CATEGORY.general, METRICS_GA_ACTIONS.failed,
        "Room already open");
    });

    it("should log an event on GotMediaPermission", function() {
      store.gotMediaPermission();

      sinon.assert.calledOnce(window.ga);
      sinon.assert.calledWithExactly(window.ga,
        "send", "event", METRICS_GA_CATEGORY.general, METRICS_GA_ACTIONS.success,
        "Media granted");
    });

    it("should log an event on JoinedRoom", function() {
      store.joinedRoom();

      sinon.assert.calledOnce(window.ga);
      sinon.assert.calledWithExactly(window.ga,
        "send", "event", METRICS_GA_CATEGORY.general, METRICS_GA_ACTIONS.success,
        "Joined room");
    });

    it("should log an event on LeaveRoom", function() {
      store.leaveRoom();

      sinon.assert.calledOnce(window.ga);
      sinon.assert.calledWithExactly(window.ga,
        "send", "event", METRICS_GA_CATEGORY.general, METRICS_GA_ACTIONS.button,
        "Leave conversation");
    });

    it("should log an event on MediaConnected", function() {
      store.mediaConnected();

      sinon.assert.calledOnce(window.ga);
      sinon.assert.calledWithExactly(window.ga,
        "send", "event", METRICS_GA_CATEGORY.general, METRICS_GA_ACTIONS.success,
        "Media connected");
    });

    it("should log an event on RecordClick", function() {
      store.recordClick(new sharedActions.RecordClick({
        linkInfo: "fake"
      }));

      sinon.assert.calledOnce(window.ga);
      sinon.assert.calledWithExactly(window.ga,
        "send", "event", METRICS_GA_CATEGORY.general, METRICS_GA_ACTIONS.linkClick,
        "fake");
    });

    it("should log an event on RemotePeerConnected", function() {
      store.remotePeerConnected();

      sinon.assert.calledOnce(window.ga);
      sinon.assert.calledWithExactly(window.ga,
        "send", "event", METRICS_GA_CATEGORY.general, METRICS_GA_ACTIONS.success,
        "Remote peer connected");
    });

    it("should log an event on RetryAfterRoomFailure", function() {
      store.retryAfterRoomFailure();

      sinon.assert.calledOnce(window.ga);
      sinon.assert.calledWithExactly(window.ga,
        "send", "event", METRICS_GA_CATEGORY.general, METRICS_GA_ACTIONS.button,
        "Retry failed room");
    });

    describe("MetricsLogJoinRoom", function() {
      it("should log a 'Join the conversation' event if not joined by Firefox", function() {
        store.metricsLogJoinRoom({
          userAgentHandledRoom: false
        });

        sinon.assert.calledOnce(window.ga);
        sinon.assert.calledWithExactly(window.ga,
          "send", "event", METRICS_GA_CATEGORY.general, METRICS_GA_ACTIONS.button,
          "Join the conversation");
      });

      it("should log a 'Joined own room in Firefox' event if joining the own room in Firefox", function() {
        store.metricsLogJoinRoom({
          userAgentHandledRoom: true,
          ownRoom: true
        });

        sinon.assert.calledOnce(window.ga);
        sinon.assert.calledWithExactly(window.ga,
          "send", "event", METRICS_GA_CATEGORY.general, METRICS_GA_ACTIONS.button,
          "Joined own room in Firefox");
      });

      it("should log a 'Joined in Firefox' event if joining a non-own room in Firefox", function() {
        store.metricsLogJoinRoom({
          userAgentHandledRoom: true,
          ownRoom: false
        });

        sinon.assert.calledOnce(window.ga);
        sinon.assert.calledWithExactly(window.ga,
          "send", "event", METRICS_GA_CATEGORY.general, METRICS_GA_ACTIONS.button,
          "Joined in Firefox");
      });
    });
  });

  describe("Store Change Handlers", function() {
    it("should log an event on room full", function() {
      fakeActiveRoomStore.setStoreState({roomState: ROOM_STATES.FULL});

      sinon.assert.calledOnce(window.ga);
      sinon.assert.calledWithExactly(window.ga,
        "send", "event", METRICS_GA_CATEGORY.general, METRICS_GA_ACTIONS.pageLoad,
        "Room full");
    });

    it("should log an event when the room is expired or invalid", function() {
      fakeActiveRoomStore.setStoreState({
        roomState: ROOM_STATES.FAILED,
        failureReason: FAILURE_DETAILS.EXPIRED_OR_INVALID
      });

      sinon.assert.calledOnce(window.ga);
      sinon.assert.calledWithExactly(window.ga,
        "send", "event", METRICS_GA_CATEGORY.general, METRICS_GA_ACTIONS.pageLoad,
        "Link expired or invalid");
    });

    it("should log an event when video mute is changed", function() {
      fakeActiveRoomStore.setStoreState({
        videoMuted: true
      });

      sinon.assert.calledOnce(window.ga);
      sinon.assert.calledWithExactly(window.ga,
        "send", "event", METRICS_GA_CATEGORY.general, METRICS_GA_ACTIONS.faceMute,
        "mute");
    });

    it("should log an event when audio mute is changed", function() {
      fakeActiveRoomStore.setStoreState({
        audioMuted: true
      });

      sinon.assert.calledOnce(window.ga);
      sinon.assert.calledWithExactly(window.ga,
        "send", "event", METRICS_GA_CATEGORY.general, METRICS_GA_ACTIONS.audioMute,
        "mute");
    });

    describe("Event listeners", function() {
      it("should call windowUnload when action is dispatched", function() {
        sandbox.stub(store, "windowUnload");

        dispatcher.dispatch(new sharedActions.WindowUnload());
        sinon.assert.calledOnce(store.windowUnload);
      });

      it("should stop listening to activeRoomStore", function() {
        var stopListeningStub = sandbox.stub(store, "stopListening");
        store.windowUnload();

        sinon.assert.calledOnce(stopListeningStub);
        sinon.assert.calledWithExactly(stopListeningStub, store.activeRoomStore);
      });
    });
  });
});
