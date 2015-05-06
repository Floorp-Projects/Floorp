/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.store = loop.store || {};

/**
 * The standalone metrics store is used to log activities to
 * analytics.
 *
 * Where possible we log events via receiving actions. However, some
 * combinations of actions and events require us to listen directly to
 * changes in the activeRoomStore so that we gain the benefit of the logic
 * in that store.
 */
loop.store.StandaloneMetricsStore = (function() {
  "use strict";

  var ROOM_STATES = loop.store.ROOM_STATES;
  var FAILURE_DETAILS = loop.shared.utils.FAILURE_DETAILS;

  loop.store.metrics = loop.store.metrics || {};

  var METRICS_GA_CATEGORY = loop.store.METRICS_GA_CATEGORY = {
    general: "/conversation/ interactions",
    download: "Firefox Downloads"
  };

  var METRICS_GA_ACTIONS = loop.store.METRICS_GA_ACTIONS = {
    audioMute: "audio mute",
    button: "button click",
    download: "download button click",
    faceMute: "face mute",
    link: "link click",
    pageLoad: "page load messages",
    success: "success",
    support: "support link click"
  };

  var StandaloneMetricsStore = loop.store.createStore({
    actions: [
      "gotMediaPermission",
      "joinRoom",
      "leaveRoom",
      "mediaConnected",
      "recordClick"
    ],

    /**
     * Initializes the store and starts listening to the activeRoomStore.
     *
     * @param  {Object} options Options for the store, should include a
     *                          reference to the activeRoomStore.
     */
    initialize: function(options) {
      options = options || {};

      if (!options.activeRoomStore) {
        throw new Error("Missing option activeRoomStore");
      }

      // Don't bother listening if we're not storing metrics.
      // I'd love for ga to be an option, but that messes up the function
      // working properly.
      if (window.ga) {
        this.activeRoomStore = options.activeRoomStore;

        this.listenTo(options.activeRoomStore, "change",
          this._onActiveRoomStoreChange.bind(this));
      }
    },

    /**
     * Returns initial state data for this store. These are mainly reflections
     * of activeRoomStore so we can match initial states for change tracking
     * purposes.
     *
     * @return {Object} The initial store state.
     */
    getInitialStoreState: function() {
      return {
        audioMuted: false,
        roomState: ROOM_STATES.INIT,
        videoMuted: false
      };
    },

    /**
     * Saves an event to ga.
     *
     * @param  {String} category The category for the event.
     * @param  {String} action   The type of action.
     * @param  {String} label    The label detailing the action.
     */
    _storeEvent: function(category, action, label) {
      // ga might not be defined if donottrack is enabled, see index.html.
      if (!window.ga) {
        return;
      }

      // For now all we need to do is forward onto ga.
      window.ga("send", "event", category, action, label);
    },

    /**
     * Handles media permssion being obtained.
     */
    gotMediaPermission: function() {
      this._storeEvent(METRICS_GA_CATEGORY.general, METRICS_GA_ACTIONS.success,
        "Media granted");
    },

    /**
     * Handles the user clicking the join room button.
     */
    joinRoom: function() {
      this._storeEvent(METRICS_GA_CATEGORY.general, METRICS_GA_ACTIONS.button,
        "Join the conversation");
    },

    /**
     * Handles the user clicking the leave room button.
     */
    leaveRoom: function() {
      this._storeEvent(METRICS_GA_CATEGORY.general, METRICS_GA_ACTIONS.button,
        "Leave conversation");
    },

    /**
     * Handles notification that two-way media has been achieved.
     */
    mediaConnected: function() {
      this._storeEvent(METRICS_GA_CATEGORY.general, METRICS_GA_ACTIONS.success,
        "Media connected");
    },

    /**
     * Handles recording link clicks.
     *
     * @param {sharedActions.RecordClick} actionData The data associated with
     *                                               the link.
     */
    recordClick: function(actionData) {
      this._storeEvent(METRICS_GA_CATEGORY.general, METRICS_GA_ACTIONS.linkClick,
        actionData.linkInfo);
    },

    /**
     * Handles notifications that the activeRoomStore has changed, updating
     * the metrics for room state and mute state as necessary.
     */
    _onActiveRoomStoreChange: function() {
      var roomStore = this.activeRoomStore.getStoreState();

      this._checkRoomState(roomStore.roomState, roomStore.failureReason);

      this._checkMuteState("audio", roomStore.audioMuted);
      this._checkMuteState("video", roomStore.videoMuted);
    },

    /**
     * Handles checking of the room state to look for events we need to send.
     *
     * @param {String} roomState     The new room state.
     * @param {String} failureReason Optional, if the room is in the failure
     *                               state, this should contain the reason for
     *                               the failure.
     */
    _checkRoomState: function(roomState, failureReason) {
      if (this._storeState.roomState === roomState) {
        return;
      }
      this._storeState.roomState = roomState;

      if (roomState === ROOM_STATES.FAILED &&
          failureReason === FAILURE_DETAILS.EXPIRED_OR_INVALID) {
        this._storeEvent(METRICS_GA_CATEGORY.general, METRICS_GA_ACTIONS.pageLoad,
          "Link expired or invalid");
      }

      if (roomState === ROOM_STATES.FULL) {
        this._storeEvent(METRICS_GA_CATEGORY.general, METRICS_GA_ACTIONS.pageLoad,
          "Room full");
      }
    },

    /**
     * Handles check of the mute state to look for events we need to send.
     *
     * @param  {String} type  The type of mute being adjusted, i.e. "audio" or
     *                        "video".
     * @param  {Boolean} muted The new state of mute
     */
    _checkMuteState: function(type, muted) {
      var muteItem = type + "Muted";
      if (this._storeState[muteItem] === muted) {
        return;
      }
      this._storeState[muteItem] = muted;

      var muteType = type === "audio" ? METRICS_GA_ACTIONS.audioMute : METRICS_GA_ACTIONS.faceMute;
      var muteState = muted ? "mute" : "unmute";

      this._storeEvent(METRICS_GA_CATEGORY.general, muteType, muteState);
    }
  });

  return StandaloneMetricsStore;
})();
