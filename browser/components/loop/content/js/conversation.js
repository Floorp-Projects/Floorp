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

  var IncomingConversationView = loop.conversationViews.IncomingConversationView;
  var OutgoingConversationView = loop.conversationViews.OutgoingConversationView;
  var CallIdentifierView = loop.conversationViews.CallIdentifierView;
  var DesktopRoomConversationView = loop.roomViews.DesktopRoomConversationView;
  var GenericFailureView = loop.conversationViews.GenericFailureView;

  /**
   * Master controller view for handling if incoming or outgoing calls are
   * in progress, and hence, which view to display.
   */
  var AppControllerView = React.createClass({displayName: "AppControllerView",
    mixins: [Backbone.Events, sharedMixins.WindowCloseMixin],

    propTypes: {
      // XXX Old types required for incoming call view.
      client: React.PropTypes.instanceOf(loop.Client).isRequired,
      conversation: React.PropTypes.instanceOf(sharedModels.ConversationModel)
                         .isRequired,
      sdk: React.PropTypes.object.isRequired,

      // XXX New types for flux style
      conversationAppStore: React.PropTypes.instanceOf(
        loop.store.ConversationAppStore).isRequired,
      conversationStore: React.PropTypes.instanceOf(loop.store.ConversationStore)
                              .isRequired,
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      roomStore: React.PropTypes.instanceOf(loop.store.RoomStore)
    },

    getInitialState: function() {
      return this.props.conversationAppStore.getStoreState();
    },

    componentWillMount: function() {
      this.listenTo(this.props.conversationAppStore, "change", function() {
        this.setState(this.props.conversationAppStore.getStoreState());
      }, this);
    },

    componentWillUnmount: function() {
      this.stopListening(this.props.conversationAppStore);
    },

    render: function() {
      switch(this.state.windowType) {
        case "incoming": {
          return (React.createElement(IncomingConversationView, {
            client: this.props.client, 
            conversation: this.props.conversation, 
            sdk: this.props.sdk, 
            conversationAppStore: this.props.conversationAppStore}
          ));
        }
        case "outgoing": {
          return (React.createElement(OutgoingConversationView, {
            store: this.props.conversationStore, 
            dispatcher: this.props.dispatcher}
          ));
        }
        case "room": {
          return (React.createElement(DesktopRoomConversationView, {
            dispatcher: this.props.dispatcher, 
            roomStore: this.props.roomStore}
          ));
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
        navigator.mozLoop.setLoopPref("ot.guid", guid);
        callback(null);
      }
    });

    var dispatcher = new loop.Dispatcher();
    var client = new loop.Client();
    var sdkDriver = new loop.OTSdkDriver({
      dispatcher: dispatcher,
      sdk: OT
    });
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
      mozLoop: navigator.mozLoop,
      sdkDriver: sdkDriver
    });
    var activeRoomStore = new loop.store.ActiveRoomStore(dispatcher, {
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

    loop.store.StoreMixin.register({feedbackStore: feedbackStore});

    // XXX Old class creation for the incoming conversation view, whilst
    // we transition across (bug 1072323).
    var conversation = new sharedModels.ConversationModel({}, {
      sdk: window.OT,
      mozLoop: navigator.mozLoop
    });

    // Obtain the windowId and pass it through
    var helper = new loop.shared.utils.Helper();
    var locationHash = helper.locationData().hash;
    var windowId;

    var hash = locationHash.match(/#(.*)/);
    if (hash) {
      windowId = hash[1];
    }

    conversation.set({windowId: windowId});

    window.addEventListener("unload", function(event) {
      // Handle direct close of dialog box via [x] control.
      // XXX Move to the conversation models, when we transition
      // incoming calls to flux (bug 1088672).
      navigator.mozLoop.calls.clearCallInProgress(windowId);

      dispatcher.dispatch(new sharedActions.WindowUnload());
    });

    React.render(React.createElement(AppControllerView, {
      conversationAppStore: conversationAppStore, 
      roomStore: roomStore, 
      conversationStore: conversationStore, 
      client: client, 
      conversation: conversation, 
      dispatcher: dispatcher, 
      sdk: window.OT}
    ), document.querySelector('#main'));

    dispatcher.dispatch(new sharedActions.GetWindowData({
      windowId: windowId
    }));
  }

  return {
    AppControllerView: AppControllerView,
    init: init
  };
})(document.mozL10n);

document.addEventListener('DOMContentLoaded', loop.conversation.init);
