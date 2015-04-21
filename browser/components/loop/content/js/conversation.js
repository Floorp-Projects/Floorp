/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* jshint newcap:false, esnext:true */
/* global loop:true, React */

var loop = loop || {};
loop.conversation = (function(mozL10n) {
  "use strict";

  var sharedViews = loop.shared.views;
  var sharedMixins = loop.shared.mixins;
  var sharedModels = loop.shared.models;
  var sharedActions = loop.shared.actions;

  var CallControllerView = loop.conversationViews.CallControllerView;
  var CallIdentifierView = loop.conversationViews.CallIdentifierView;
  var DesktopRoomConversationView = loop.roomViews.DesktopRoomConversationView;
  var GenericFailureView = loop.conversationViews.GenericFailureView;

  /**
   * Master controller view for handling if incoming or outgoing calls are
   * in progress, and hence, which view to display.
   */
  var AppControllerView = React.createClass({displayName: "AppControllerView",
    mixins: [
      Backbone.Events,
      loop.store.StoreMixin("conversationAppStore"),
      sharedMixins.WindowCloseMixin
    ],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      roomStore: React.PropTypes.instanceOf(loop.store.RoomStore),
      mozLoop: React.PropTypes.object.isRequired
    },

    getInitialState: function() {
      return this.getStoreState();
    },

    render: function() {
      switch(this.state.windowType) {
        // CallControllerView is used for both.
        case "incoming":
        case "outgoing": {
          return (React.createElement(CallControllerView, {
            dispatcher: this.props.dispatcher, 
            mozLoop: this.props.mozLoop}));
        }
        case "room": {
          return (React.createElement(DesktopRoomConversationView, {
            dispatcher: this.props.dispatcher, 
            mozLoop: this.props.mozLoop, 
            roomStore: this.props.roomStore}));
        }
        case "failed": {
          return React.createElement(GenericFailureView, {cancelCall: this.closeWindow});
        }
        default: {
          // If we don't have a windowType, we don't know what we are yet,
          // so don't display anything.
          return null;
        }
      }
    }
  });

  /**
   * Conversation initialisation.
   */
  function init() {
    // Do the initial L10n setup, we do this before anything
    // else to ensure the L10n environment is setup correctly.
    mozL10n.initialize(navigator.mozLoop);

    // Plug in an alternate client ID mechanism, as localStorage and cookies
    // don't work in the conversation window
    window.OT.overrideGuidStorage({
      get: function(callback) {
        callback(null, navigator.mozLoop.getLoopPref("ot.guid"));
      },
      set: function(guid, callback) {
        // See nsIPrefBranch
        const PREF_STRING = 32;
        navigator.mozLoop.setLoopPref("ot.guid", guid, PREF_STRING);
        callback(null);
      }
    });

    var dispatcher = new loop.Dispatcher();
    var client = new loop.Client();
    var sdkDriver = new loop.OTSdkDriver({
      isDesktop: true,
      dispatcher: dispatcher,
      sdk: OT,
      mozLoop: navigator.mozLoop
    });

    // expose for functional tests
    loop.conversation._sdkDriver = sdkDriver;

    var appVersionInfo = navigator.mozLoop.appVersionInfo;
    var feedbackClient = new loop.FeedbackAPIClient(
      navigator.mozLoop.getLoopPref("feedback.baseUrl"), {
      product: navigator.mozLoop.getLoopPref("feedback.product"),
      platform: appVersionInfo.OS,
      channel: appVersionInfo.channel,
      version: appVersionInfo.version
    });

    // Create the stores.
    var conversationAppStore = new loop.store.ConversationAppStore({
      dispatcher: dispatcher,
      mozLoop: navigator.mozLoop
    });
    var conversationStore = new loop.store.ConversationStore(dispatcher, {
      client: client,
      isDesktop: true,
      mozLoop: navigator.mozLoop,
      sdkDriver: sdkDriver
    });
    var activeRoomStore = new loop.store.ActiveRoomStore(dispatcher, {
      isDesktop: true,
      mozLoop: navigator.mozLoop,
      sdkDriver: sdkDriver
    });
    var roomStore = new loop.store.RoomStore(dispatcher, {
      mozLoop: navigator.mozLoop,
      activeRoomStore: activeRoomStore
    });
    var feedbackStore = new loop.store.FeedbackStore(dispatcher, {
      feedbackClient: feedbackClient
    });

    loop.store.StoreMixin.register({
      conversationAppStore: conversationAppStore,
      conversationStore: conversationStore,
      feedbackStore: feedbackStore,
    });

    // Obtain the windowId and pass it through
    var locationHash = loop.shared.utils.locationData().hash;
    var windowId;

    var hash = locationHash.match(/#(.*)/);
    if (hash) {
      windowId = hash[1];
    }

    window.addEventListener("unload", function(event) {
      dispatcher.dispatch(new sharedActions.WindowUnload());
    });

    React.render(React.createElement(AppControllerView, {
      roomStore: roomStore, 
      dispatcher: dispatcher, 
      mozLoop: navigator.mozLoop}
    ), document.querySelector('#main'));

    dispatcher.dispatch(new sharedActions.GetWindowData({
      windowId: windowId
    }));
  }

  return {
    AppControllerView: AppControllerView,
    init: init,

    /**
     * Exposed for the use of functional tests to be able to check
     * metric-related execution as the call sequence progresses.
     *
     * @type loop.OTSdkDriver
     */
    _sdkDriver: null
  };
})(document.mozL10n);

document.addEventListener('DOMContentLoaded', loop.conversation.init);
