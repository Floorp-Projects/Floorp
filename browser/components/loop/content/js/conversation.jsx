/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.conversation = (function(mozL10n) {
  "use strict";

  var sharedMixins = loop.shared.mixins;
  var sharedActions = loop.shared.actions;

  var CallControllerView = loop.conversationViews.CallControllerView;
  var DesktopRoomConversationView = loop.roomViews.DesktopRoomConversationView;
  var FeedbackView = loop.feedbackViews.FeedbackView;
  var GenericFailureView = loop.conversationViews.GenericFailureView;

  /**
   * Master controller view for handling if incoming or outgoing calls are
   * in progress, and hence, which view to display.
   */
  var AppControllerView = React.createClass({
    mixins: [
      Backbone.Events,
      loop.store.StoreMixin("conversationAppStore"),
      sharedMixins.DocumentTitleMixin,
      sharedMixins.WindowCloseMixin
    ],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      mozLoop: React.PropTypes.object.isRequired,
      roomStore: React.PropTypes.instanceOf(loop.store.RoomStore)
    },

    getInitialState: function() {
      return this.getStoreState();
    },

    _renderFeedbackForm: function() {
      this.setTitle(mozL10n.get("conversation_has_ended"));

      return (<FeedbackView
        mozLoop={this.props.mozLoop}
        onAfterFeedbackReceived={this.closeWindow} />);
    },

    /**
     * We only show the feedback for once every 6 months, otherwise close
     * the window.
     */
    handleCallTerminated: function() {
      var delta = new Date() - new Date(this.state.feedbackTimestamp);

      // Show timestamp if feedback period (6 months) passed.
      // 0 is default value for pref. Always show feedback form on first use.
      if (this.state.feedbackTimestamp === 0 ||
          delta >= this.state.feedbackPeriod) {
        this.props.dispatcher.dispatch(new sharedActions.ShowFeedbackForm());
        return;
      }

      this.closeWindow();
    },

    render: function() {
      if (this.state.showFeedbackForm) {
        return this._renderFeedbackForm();
      }

      switch(this.state.windowType) {
        // CallControllerView is used for both.
        case "incoming":
        case "outgoing": {
          return (<CallControllerView
            dispatcher={this.props.dispatcher}
            mozLoop={this.props.mozLoop}
            onCallTerminated={this.handleCallTerminated} />);
        }
        case "room": {
          return (<DesktopRoomConversationView
            dispatcher={this.props.dispatcher}
            mozLoop={this.props.mozLoop}
            onCallTerminated={this.handleCallTerminated}
            roomStore={this.props.roomStore} />);
        }
        case "failed": {
          return <GenericFailureView cancelCall={this.closeWindow} />;
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

    // We want data channels only if the text chat preference is enabled.
    var useDataChannels = loop.shared.utils.getBoolPreference("textChat.enabled");

    var dispatcher = new loop.Dispatcher();
    var client = new loop.Client();
    var sdkDriver = new loop.OTSdkDriver({
      isDesktop: true,
      useDataChannels: useDataChannels,
      dispatcher: dispatcher,
      sdk: OT,
      mozLoop: navigator.mozLoop
    });

    // expose for functional tests
    loop.conversation._sdkDriver = sdkDriver;

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
    var textChatStore = new loop.store.TextChatStore(dispatcher, {
      sdkDriver: sdkDriver
    });

    loop.store.StoreMixin.register({
      conversationAppStore: conversationAppStore,
      conversationStore: conversationStore,
      textChatStore: textChatStore
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

    React.render(
      <AppControllerView
        dispatcher={dispatcher}
        mozLoop={navigator.mozLoop}
        roomStore={roomStore} />, document.querySelector("#main"));

    document.documentElement.setAttribute("lang", mozL10n.getLanguage());
    document.documentElement.setAttribute("dir", mozL10n.getDirection());
    document.body.setAttribute("platform", loop.shared.utils.getPlatform());

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

document.addEventListener("DOMContentLoaded", loop.conversation.init);
