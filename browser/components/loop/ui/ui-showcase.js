/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* jshint newcap:false */
/* global loop:true, React */

(function() {
  "use strict";

  // Stop the default init functions running to avoid conflicts.
  document.removeEventListener('DOMContentLoaded', loop.panel.init);
  document.removeEventListener('DOMContentLoaded', loop.conversation.init);

  // 1. Desktop components
  // 1.1 Panel
  var PanelView = loop.panel.PanelView;
  // 1.2. Conversation Window
  var AcceptCallView = loop.conversationViews.AcceptCallView;
  var DesktopPendingConversationView = loop.conversationViews.PendingConversationView;
  var CallFailedView = loop.conversationViews.CallFailedView;
  var DesktopRoomConversationView = loop.roomViews.DesktopRoomConversationView;

  // 2. Standalone webapp
  var HomeView = loop.webapp.HomeView;
  var UnsupportedBrowserView  = loop.webapp.UnsupportedBrowserView;
  var UnsupportedDeviceView   = loop.webapp.UnsupportedDeviceView;
  var CallUrlExpiredView      = loop.webapp.CallUrlExpiredView;
  var GumPromptConversationView = loop.webapp.GumPromptConversationView;
  var WaitingConversationView = loop.webapp.WaitingConversationView;
  var StartConversationView   = loop.webapp.StartConversationView;
  var FailedConversationView  = loop.webapp.FailedConversationView;
  var EndedConversationView   = loop.webapp.EndedConversationView;
  var StandaloneRoomView      = loop.standaloneRoomViews.StandaloneRoomView;

  // 3. Shared components
  var ConversationToolbar = loop.shared.views.ConversationToolbar;
  var ConversationView = loop.shared.views.ConversationView;
  var FeedbackView = loop.shared.views.FeedbackView;

  // Store constants
  var ROOM_STATES = loop.store.ROOM_STATES;
  var FEEDBACK_STATES = loop.store.FEEDBACK_STATES;
  var CALL_TYPES = loop.shared.utils.CALL_TYPES;

  // Local helpers
  function returnTrue() {
    return true;
  }

  function returnFalse() {
    return false;
  }

  function noop(){}

  // We save the visibility change listeners so that we can fake an event
  // to the panel once we've loaded all the views.
  var visibilityListeners = [];
  var rootObject = window;

  rootObject.document.addEventListener = function(eventName, func) {
    if (eventName === "visibilitychange") {
      visibilityListeners.push(func);
    }
    window.addEventListener(eventName, func);
  };

  rootObject.document.removeEventListener = function(eventName, func) {
    if (eventName === "visibilitychange") {
      var index = visibilityListeners.indexOf(func);
      visibilityListeners.splice(index, 1);
    }
    window.removeEventListener(eventName, func);
  };

  loop.shared.mixins.setRootObject(rootObject);

  // Feedback API client configured to send data to the stage input server,
  // which is available at https://input.allizom.org
  var stageFeedbackApiClient = new loop.FeedbackAPIClient(
    "https://input.allizom.org/api/v1/feedback", {
      product: "Loop"
    }
  );

  var mockSDK = _.extend({}, Backbone.Events);

  var dispatcher = new loop.Dispatcher();
  var activeRoomStore = new loop.store.ActiveRoomStore(dispatcher, {
    mozLoop: navigator.mozLoop,
    sdkDriver: mockSDK
  });
  var roomStore = new loop.store.RoomStore(dispatcher, {
    mozLoop: navigator.mozLoop
  });
  var feedbackStore = new loop.store.FeedbackStore(dispatcher, {
    feedbackClient: stageFeedbackApiClient
  });
  var conversationStore = new loop.store.ConversationStore(dispatcher, {
    client: {},
    mozLoop: navigator.mozLoop,
    sdkDriver: mockSDK
  });

  loop.store.StoreMixin.register({
    conversationStore: conversationStore,
    feedbackStore: feedbackStore
  });

  // Local mocks

  var mockMozLoopRooms = _.extend({}, navigator.mozLoop);

  var mockContact = {
    name: ["Mr Smith"],
    email: [{
      value: "smith@invalid.com"
    }]
  };

  var mockClient = {
    requestCallUrlInfo: noop
  };

  var mockConversationModel = new loop.shared.models.ConversationModel({
    callerId: "Mrs Jones",
    urlCreationDate: (new Date() / 1000).toString()
  }, {
    sdk: mockSDK
  });
  mockConversationModel.startSession = noop;

  var mockWebSocket = new loop.CallConnectionWebSocket({
    url: "fake",
    callId: "fakeId",
    websocketToken: "fakeToken"
  });

  var notifications = new loop.shared.models.NotificationCollection();
  var errNotifications = new loop.shared.models.NotificationCollection();
  errNotifications.add({
    level: "error",
    message: "Could Not Authenticate",
    details: "Did you change your password?",
    detailsButtonLabel: "Retry"
  });

  var SVGIcon = React.createClass({displayName: "SVGIcon",
    render: function() {
      var sizeUnit = this.props.size.split("x")[0] + "px";
      return (
        React.createElement("span", {className: "svg-icon", style: {
          "backgroundImage": "url(../content/shared/img/icons-" + this.props.size +
                              ".svg#" + this.props.shapeId + ")",
          "backgroundSize": sizeUnit + " " + sizeUnit
        }})
      );
    }
  });

  var SVGIcons = React.createClass({displayName: "SVGIcons",
    shapes: {
      "10x10": ["close", "close-active", "close-disabled", "dropdown",
        "dropdown-white", "dropdown-active", "dropdown-disabled", "edit",
        "edit-active", "edit-disabled", "expand", "expand-active", "expand-disabled",
        "minimize", "minimize-active", "minimize-disabled"
      ],
      "14x14": ["audio", "audio-active", "audio-disabled", "facemute",
        "facemute-active", "facemute-disabled", "hangup", "hangup-active",
        "hangup-disabled", "incoming", "incoming-active", "incoming-disabled",
        "link", "link-active", "link-disabled", "mute", "mute-active",
        "mute-disabled", "pause", "pause-active", "pause-disabled", "video",
        "video-white", "video-active", "video-disabled", "volume", "volume-active",
        "volume-disabled"
      ],
      "16x16": ["add", "add-hover", "add-active", "audio", "audio-hover", "audio-active",
        "block", "block-red", "block-hover", "block-active", "contacts", "contacts-hover",
        "contacts-active", "copy", "checkmark", "google", "google-hover", "google-active",
        "history", "history-hover", "history-active", "leave", "precall", "precall-hover",
        "precall-active", "screen-white", "screenmute-white", "settings",
        "settings-hover", "settings-active", "share-darkgrey", "tag", "tag-hover",
        "tag-active", "trash", "unblock", "unblock-hover", "unblock-active", "video",
        "video-hover", "video-active", "tour"
      ]
    },

    render: function() {
      var icons = this.shapes[this.props.size].map(function(shapeId, i) {
        return (
          React.createElement("li", {key: this.props.size + "-" + i, className: "svg-icon-entry"}, 
            React.createElement("p", null, React.createElement(SVGIcon, {shapeId: shapeId, size: this.props.size})), 
            React.createElement("p", null, shapeId)
          )
        );
      }, this);
      return (
        React.createElement("ul", {className: "svg-icon-list"}, icons)
      );
    }
  });

  var Example = React.createClass({displayName: "Example",
    makeId: function(prefix) {
      return (prefix || "") + this.props.summary.toLowerCase().replace(/\s/g, "-");
    },

    render: function() {
      var cx = React.addons.classSet;
      return (
        React.createElement("div", {className: "example"}, 
          React.createElement("h3", {id: this.makeId()}, 
            this.props.summary, 
            React.createElement("a", {href: this.makeId("#")}, " ¶")
          ), 
          React.createElement("div", {className: cx({comp: true, dashed: this.props.dashed}), 
               style: this.props.style}, 
            this.props.children
          )
        )
      );
    }
  });

  var Section = React.createClass({displayName: "Section",
    render: function() {
      return (
        React.createElement("section", {id: this.props.name, className: this.props.className}, 
          React.createElement("h1", null, this.props.name), 
          this.props.children
        )
      );
    }
  });

  var ShowCase = React.createClass({displayName: "ShowCase",
    render: function() {
      return (
        React.createElement("div", {className: "showcase"}, 
          React.createElement("header", null, 
            React.createElement("h1", null, "Loop UI Components Showcase"), 
            React.createElement("nav", {className: "showcase-menu"}, 
              React.Children.map(this.props.children, function(section) {
                return (
                  React.createElement("a", {className: "btn btn-info", href: "#" + section.props.name}, 
                    section.props.name
                  )
                );
              })
            )
          ), 
          this.props.children
        )
      );
    }
  });

  var App = React.createClass({displayName: "App",
    render: function() {
      return (
        React.createElement(ShowCase, null, 
          React.createElement(Section, {name: "PanelView"}, 
            React.createElement("p", {className: "note"}, 
              React.createElement("strong", null, "Note:"), " 332px wide."
            ), 
            React.createElement(Example, {summary: "Room list tab", dashed: "true", style: {width: "332px"}}, 
              React.createElement(PanelView, {client: mockClient, notifications: notifications, 
                         userProfile: {email: "test@example.com"}, 
                         mozLoop: mockMozLoopRooms, 
                         dispatcher: dispatcher, 
                         roomStore: roomStore, 
                         selectedTab: "rooms"})
            ), 
            React.createElement(Example, {summary: "Contact list tab", dashed: "true", style: {width: "332px"}}, 
              React.createElement(PanelView, {client: mockClient, notifications: notifications, 
                         userProfile: {email: "test@example.com"}, 
                         mozLoop: mockMozLoopRooms, 
                         dispatcher: dispatcher, 
                         roomStore: roomStore, 
                         selectedTab: "contacts"})
            ), 
            React.createElement(Example, {summary: "Error Notification", dashed: "true", style: {width: "332px"}}, 
              React.createElement(PanelView, {client: mockClient, notifications: errNotifications, 
                         mozLoop: navigator.mozLoop, 
                         dispatcher: dispatcher, 
                         roomStore: roomStore})
            ), 
            React.createElement(Example, {summary: "Error Notification - authenticated", dashed: "true", style: {width: "332px"}}, 
              React.createElement(PanelView, {client: mockClient, notifications: errNotifications, 
                         userProfile: {email: "test@example.com"}, 
                         mozLoop: navigator.mozLoop, 
                         dispatcher: dispatcher, 
                         roomStore: roomStore})
            ), 
            React.createElement(Example, {summary: "Contact import success", dashed: "true", style: {width: "332px"}}, 
              React.createElement(PanelView, {notifications: new loop.shared.models.NotificationCollection([{level: "success", message: "Import success"}]), 
                         userProfile: {email: "test@example.com"}, 
                         mozLoop: mockMozLoopRooms, 
                         dispatcher: dispatcher, 
                         roomStore: roomStore, 
                         selectedTab: "contacts"})
            ), 
            React.createElement(Example, {summary: "Contact import error", dashed: "true", style: {width: "332px"}}, 
              React.createElement(PanelView, {notifications: new loop.shared.models.NotificationCollection([{level: "error", message: "Import error"}]), 
                         userProfile: {email: "test@example.com"}, 
                         mozLoop: mockMozLoopRooms, 
                         dispatcher: dispatcher, 
                         roomStore: roomStore, 
                         selectedTab: "contacts"})
            )
          ), 

          React.createElement(Section, {name: "AcceptCallView"}, 
            React.createElement(Example, {summary: "Default / incoming video call", dashed: "true", style: {width: "300px", height: "272px"}}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(AcceptCallView, {callType: CALL_TYPES.AUDIO_VIDEO, 
                                callerId: "Mr Smith", 
                                dispatcher: dispatcher, 
                                mozLoop: mockMozLoopRooms})
              )
            ), 

            React.createElement(Example, {summary: "Default / incoming audio only call", dashed: "true", style: {width: "300px", height: "272px"}}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(AcceptCallView, {callType: CALL_TYPES.AUDIO_ONLY, 
                                callerId: "Mr Smith", 
                                dispatcher: dispatcher, 
                                mozLoop: mockMozLoopRooms})
              )
            )
          ), 

          React.createElement(Section, {name: "AcceptCallView-ActiveState"}, 
            React.createElement(Example, {summary: "Default", dashed: "true", style: {width: "300px", height: "272px"}}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(AcceptCallView, {callType: CALL_TYPES.AUDIO_VIDEO, 
                                callerId: "Mr Smith", 
                                dispatcher: dispatcher, 
                                mozLoop: mockMozLoopRooms, 
                                showMenu: true})
              )
            )
          ), 

          React.createElement(Section, {name: "ConversationToolbar"}, 
            React.createElement("h2", null, "Desktop Conversation Window"), 
            React.createElement("div", {className: "fx-embedded override-position"}, 
              React.createElement(Example, {summary: "Default", dashed: "true", style: {width: "300px", height: "272px"}}, 
                React.createElement(ConversationToolbar, {video: {enabled: true}, 
                                     audio: {enabled: true}, 
                                     hangup: noop, 
                                     publishStream: noop})
              ), 
              React.createElement(Example, {summary: "Video muted", style: {width: "300px", height: "272px"}}, 
                React.createElement(ConversationToolbar, {video: {enabled: false}, 
                                     audio: {enabled: true}, 
                                     hangup: noop, 
                                     publishStream: noop})
              ), 
              React.createElement(Example, {summary: "Audio muted", style: {width: "300px", height: "272px"}}, 
                React.createElement(ConversationToolbar, {video: {enabled: true}, 
                                     audio: {enabled: false}, 
                                     hangup: noop, 
                                     publishStream: noop})
              )
            ), 

            React.createElement("h2", null, "Standalone"), 
            React.createElement("div", {className: "standalone override-position"}, 
              React.createElement(Example, {summary: "Default"}, 
                React.createElement(ConversationToolbar, {video: {enabled: true}, 
                                     audio: {enabled: true}, 
                                     hangup: noop, 
                                     publishStream: noop})
              ), 
              React.createElement(Example, {summary: "Video muted"}, 
                React.createElement(ConversationToolbar, {video: {enabled: false}, 
                                     audio: {enabled: true}, 
                                     hangup: noop, 
                                     publishStream: noop})
              ), 
              React.createElement(Example, {summary: "Audio muted"}, 
                React.createElement(ConversationToolbar, {video: {enabled: true}, 
                                     audio: {enabled: false}, 
                                     hangup: noop, 
                                     publishStream: noop})
              )
            )
          ), 

          React.createElement(Section, {name: "GumPromptConversationView"}, 
            React.createElement(Example, {summary: "Gum Prompt conversation view", dashed: "true"}, 
              React.createElement("div", {className: "standalone"}, 
                React.createElement(GumPromptConversationView, null)
              )
            )
          ), 

          React.createElement(Section, {name: "WaitingConversationView"}, 
            React.createElement(Example, {summary: "Waiting conversation view (connecting)", dashed: "true"}, 
              React.createElement("div", {className: "standalone"}, 
                React.createElement(WaitingConversationView, {websocket: mockWebSocket, 
                                         dispatcher: dispatcher})
              )
            ), 
            React.createElement(Example, {summary: "Waiting conversation view (ringing)", dashed: "true"}, 
              React.createElement("div", {className: "standalone"}, 
                React.createElement(WaitingConversationView, {websocket: mockWebSocket, 
                                         dispatcher: dispatcher, 
                                         callState: "ringing"})
              )
            )
          ), 

          React.createElement(Section, {name: "PendingConversationView (Desktop)"}, 
            React.createElement(Example, {summary: "Connecting", dashed: "true", 
                     style: {width: "300px", height: "272px"}}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(DesktopPendingConversationView, {callState: "gather", 
                                                contact: mockContact, 
                                                dispatcher: dispatcher})
              )
            )
          ), 

          React.createElement(Section, {name: "CallFailedView"}, 
            React.createElement(Example, {summary: "Call Failed - Incoming", dashed: "true", 
                     style: {width: "300px", height: "272px"}}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(CallFailedView, {dispatcher: dispatcher, 
                                outgoing: false, 
                                store: conversationStore})
              )
            ), 
            React.createElement(Example, {summary: "Call Failed - Outgoing", dashed: "true", 
                     style: {width: "300px", height: "272px"}}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(CallFailedView, {dispatcher: dispatcher, 
                                outgoing: true, 
                                store: conversationStore})
              )
            ), 
            React.createElement(Example, {summary: "Call Failed — with call URL error", dashed: "true", 
                     style: {width: "300px", height: "272px"}}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(CallFailedView, {dispatcher: dispatcher, emailLinkError: true, 
                                outgoing: true, 
                                store: conversationStore})
              )
            )
          ), 

          React.createElement(Section, {name: "StartConversationView"}, 
            React.createElement(Example, {summary: "Start conversation view", dashed: "true"}, 
              React.createElement("div", {className: "standalone"}, 
                React.createElement(StartConversationView, {conversation: mockConversationModel, 
                                       client: mockClient, 
                                       notifications: notifications})
              )
            )
          ), 

          React.createElement(Section, {name: "FailedConversationView"}, 
            React.createElement(Example, {summary: "Failed conversation view", dashed: "true"}, 
              React.createElement("div", {className: "standalone"}, 
                React.createElement(FailedConversationView, {conversation: mockConversationModel, 
                                        client: mockClient, 
                                        notifications: notifications})
              )
            )
          ), 

          React.createElement(Section, {name: "ConversationView"}, 
            React.createElement(Example, {summary: "Desktop conversation window", dashed: "true", 
                     style: {width: "300px", height: "272px"}}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(ConversationView, {sdk: mockSDK, 
                                  model: mockConversationModel, 
                                  video: {enabled: true}, 
                                  audio: {enabled: true}})
              )
            ), 

            React.createElement(Example, {summary: "Desktop conversation window large", dashed: "true"}, 
              React.createElement("div", {className: "breakpoint", "data-breakpoint-width": "800px", 
                "data-breakpoint-height": "600px"}, 
                React.createElement("div", {className: "fx-embedded"}, 
                  React.createElement(ConversationView, {sdk: mockSDK, 
                    video: {enabled: true}, 
                    audio: {enabled: true}, 
                    model: mockConversationModel})
                )
              )
            ), 

            React.createElement(Example, {summary: "Desktop conversation window local audio stream", 
                     dashed: "true", style: {width: "300px", height: "272px"}}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(ConversationView, {sdk: mockSDK, 
                                  video: {enabled: false}, 
                                  audio: {enabled: true}, 
                                  model: mockConversationModel})
              )
            ), 

            React.createElement(Example, {summary: "Standalone version"}, 
              React.createElement("div", {className: "standalone"}, 
                React.createElement(ConversationView, {sdk: mockSDK, 
                                  video: {enabled: true}, 
                                  audio: {enabled: true}, 
                                  model: mockConversationModel})
              )
            )
          ), 

          React.createElement(Section, {name: "ConversationView-640"}, 
            React.createElement(Example, {summary: "640px breakpoint for conversation view"}, 
              React.createElement("div", {className: "breakpoint", 
                   style: {"text-align":"center"}, 
                   "data-breakpoint-width": "400px", 
                   "data-breakpoint-height": "780px"}, 
                React.createElement("div", {className: "standalone"}, 
                  React.createElement(ConversationView, {sdk: mockSDK, 
                                    video: {enabled: true}, 
                                    audio: {enabled: true}, 
                                    model: mockConversationModel})
                )
              )
            )
          ), 

          React.createElement(Section, {name: "ConversationView-LocalAudio"}, 
            React.createElement(Example, {summary: "Local stream is audio only"}, 
              React.createElement("div", {className: "standalone"}, 
                React.createElement(ConversationView, {sdk: mockSDK, 
                                  video: {enabled: false}, 
                                  audio: {enabled: true}, 
                                  model: mockConversationModel})
              )
            )
          ), 

          React.createElement(Section, {name: "FeedbackView"}, 
            React.createElement("p", {className: "note"}, 
              React.createElement("strong", null, "Note:"), " For the useable demo, you can access submitted data at ", 
              React.createElement("a", {href: "https://input.allizom.org/"}, "input.allizom.org"), "."
            ), 
            React.createElement(Example, {summary: "Default (useable demo)", dashed: "true", style: {width: "300px", height: "272px"}}, 
              React.createElement(FeedbackView, {feedbackStore: feedbackStore})
            ), 
            React.createElement(Example, {summary: "Detailed form", dashed: "true", style: {width: "300px", height: "272px"}}, 
              React.createElement(FeedbackView, {feedbackStore: feedbackStore, feedbackState: FEEDBACK_STATES.DETAILS})
            ), 
            React.createElement(Example, {summary: "Thank you!", dashed: "true", style: {width: "300px", height: "272px"}}, 
              React.createElement(FeedbackView, {feedbackStore: feedbackStore, feedbackState: FEEDBACK_STATES.SENT})
            )
          ), 

          React.createElement(Section, {name: "CallUrlExpiredView"}, 
            React.createElement(Example, {summary: "Firefox User"}, 
              React.createElement(CallUrlExpiredView, {isFirefox: true})
            ), 
            React.createElement(Example, {summary: "Non-Firefox User"}, 
              React.createElement(CallUrlExpiredView, {isFirefox: false})
            )
          ), 

          React.createElement(Section, {name: "EndedConversationView"}, 
            React.createElement(Example, {summary: "Displays the feedback form"}, 
              React.createElement("div", {className: "standalone"}, 
                React.createElement(EndedConversationView, {sdk: mockSDK, 
                                       video: {enabled: true}, 
                                       audio: {enabled: true}, 
                                       conversation: mockConversationModel, 
                                       feedbackStore: feedbackStore, 
                                       onAfterFeedbackReceived: noop})
              )
            )
          ), 

          React.createElement(Section, {name: "AlertMessages"}, 
            React.createElement(Example, {summary: "Various alerts"}, 
              React.createElement("div", {className: "alert alert-warning"}, 
                React.createElement("button", {className: "close"}), 
                React.createElement("p", {className: "message"}, 
                  "The person you were calling has ended the conversation."
                )
              ), 
              React.createElement("br", null), 
              React.createElement("div", {className: "alert alert-error"}, 
                React.createElement("button", {className: "close"}), 
                React.createElement("p", {className: "message"}, 
                  "The person you were calling has ended the conversation."
                )
              )
            )
          ), 

          React.createElement(Section, {name: "HomeView"}, 
            React.createElement(Example, {summary: "Standalone Home View"}, 
              React.createElement("div", {className: "standalone"}, 
                React.createElement(HomeView, null)
              )
            )
          ), 


          React.createElement(Section, {name: "UnsupportedBrowserView"}, 
            React.createElement(Example, {summary: "Standalone Unsupported Browser"}, 
              React.createElement("div", {className: "standalone"}, 
                React.createElement(UnsupportedBrowserView, {isFirefox: false})
              )
            )
          ), 

          React.createElement(Section, {name: "UnsupportedDeviceView"}, 
            React.createElement(Example, {summary: "Standalone Unsupported Device"}, 
              React.createElement("div", {className: "standalone"}, 
                React.createElement(UnsupportedDeviceView, {platform: "ios"})
              )
            )
          ), 

          React.createElement(Section, {name: "DesktopRoomConversationView"}, 
            React.createElement(Example, {summary: "Desktop room conversation (invitation)", dashed: "true", 
                     style: {width: "260px", height: "265px"}}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(DesktopRoomConversationView, {
                  roomStore: roomStore, 
                  dispatcher: dispatcher, 
                  mozLoop: navigator.mozLoop, 
                  roomState: ROOM_STATES.INIT})
              )
            ), 

            React.createElement(Example, {summary: "Desktop room conversation", dashed: "true", 
                     style: {width: "260px", height: "265px"}}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(DesktopRoomConversationView, {
                  roomStore: roomStore, 
                  dispatcher: dispatcher, 
                  mozLoop: navigator.mozLoop, 
                  roomState: ROOM_STATES.HAS_PARTICIPANTS})
              )
            )
          ), 

          React.createElement(Section, {name: "StandaloneRoomView"}, 
            React.createElement(Example, {summary: "Standalone room conversation (ready)"}, 
              React.createElement("div", {className: "standalone"}, 
                React.createElement(StandaloneRoomView, {
                  dispatcher: dispatcher, 
                  activeRoomStore: activeRoomStore, 
                  roomState: ROOM_STATES.READY, 
                  isFirefox: true})
              )
            ), 

            React.createElement(Example, {summary: "Standalone room conversation (joined)"}, 
              React.createElement("div", {className: "standalone"}, 
                React.createElement(StandaloneRoomView, {
                  dispatcher: dispatcher, 
                  activeRoomStore: activeRoomStore, 
                  roomState: ROOM_STATES.JOINED, 
                  isFirefox: true})
              )
            ), 

            React.createElement(Example, {summary: "Standalone room conversation (has-participants)"}, 
              React.createElement("div", {className: "standalone"}, 
                React.createElement(StandaloneRoomView, {
                  dispatcher: dispatcher, 
                  activeRoomStore: activeRoomStore, 
                  roomState: ROOM_STATES.HAS_PARTICIPANTS, 
                  isFirefox: true})
              )
            ), 

            React.createElement(Example, {summary: "Standalone room conversation (full - FFx user)"}, 
              React.createElement("div", {className: "standalone"}, 
                React.createElement(StandaloneRoomView, {
                  dispatcher: dispatcher, 
                  activeRoomStore: activeRoomStore, 
                  roomState: ROOM_STATES.FULL, 
                  isFirefox: true})
              )
            ), 

            React.createElement(Example, {summary: "Standalone room conversation (full - non FFx user)"}, 
              React.createElement("div", {className: "standalone"}, 
                React.createElement(StandaloneRoomView, {
                  dispatcher: dispatcher, 
                  activeRoomStore: activeRoomStore, 
                  roomState: ROOM_STATES.FULL, 
                  isFirefox: false})
              )
            ), 

            React.createElement(Example, {summary: "Standalone room conversation (feedback)"}, 
              React.createElement("div", {className: "standalone"}, 
                React.createElement(StandaloneRoomView, {
                  dispatcher: dispatcher, 
                  activeRoomStore: activeRoomStore, 
                  feedbackStore: feedbackStore, 
                  roomState: ROOM_STATES.ENDED, 
                  isFirefox: false})
              )
            ), 

            React.createElement(Example, {summary: "Standalone room conversation (failed)"}, 
              React.createElement("div", {className: "standalone"}, 
                React.createElement(StandaloneRoomView, {
                  dispatcher: dispatcher, 
                  activeRoomStore: activeRoomStore, 
                  roomState: ROOM_STATES.FAILED, 
                  isFirefox: false})
              )
            )
          ), 

          React.createElement(Section, {name: "SVG icons preview", className: "svg-icons"}, 
            React.createElement(Example, {summary: "10x10"}, 
              React.createElement(SVGIcons, {size: "10x10"})
            ), 
            React.createElement(Example, {summary: "14x14"}, 
              React.createElement(SVGIcons, {size: "14x14"})
            ), 
            React.createElement(Example, {summary: "16x16"}, 
              React.createElement(SVGIcons, {size: "16x16"})
            )
          )

        )
      );
    }
  });

  /**
   * Render components that have different styles across
   * CSS media rules in their own iframe to mimic the viewport
   * */
  function _renderComponentsInIframes() {
    var parents = document.querySelectorAll('.breakpoint');
    [].forEach.call(parents, appendChildInIframe);

    /**
     * Extracts the component from the DOM and appends in the an iframe
     *
     * @type {HTMLElement} parent - Parent DOM node of a component & iframe
     * */
    function appendChildInIframe(parent) {
      var styles     = document.querySelector('head').children;
      var component  = parent.children[0];
      var iframe     = document.createElement('iframe');
      var width      = parent.dataset.breakpointWidth;
      var height     = parent.dataset.breakpointHeight;

      iframe.style.width  = width;
      iframe.style.height = height;

      parent.appendChild(iframe);
      iframe.src    = "about:blank";
      // Workaround for bug 297685
      iframe.onload = function () {
        var iframeHead = iframe.contentDocument.querySelector('head');
        iframe.contentDocument.documentElement.querySelector('body')
                                              .appendChild(component);

        [].forEach.call(styles, function(style) {
          iframeHead.appendChild(style.cloneNode(true));
        });

      };
    }
  }

  window.addEventListener("DOMContentLoaded", function() {
    try {
      React.renderComponent(React.createElement(App, null), document.getElementById("main"));

      for (var listener of visibilityListeners) {
        listener({target: {hidden: false}});
      }
    } catch(err) {
      console.error(err);
      uncaughtError = err;
    }

    _renderComponentsInIframes();

    // Put the title back, in case views changed it.
    document.title = "Loop UI Components Showcase";

    // This simulates the mocha layout for errors which means we can run
    // this alongside our other unit tests but use the same harness.
    if (uncaughtError) {
      $("#results").append("<div class='failures'><em>1</em></div>");
      $("#results").append("<li class='test fail'>" +
        "<h2>Errors rendering UI-Showcase</h2>" +
        "<pre class='error'>" + uncaughtError + "\n" + uncaughtError.stack + "</pre>" +
        "</li>");
    } else {
      $("#results").append("<div class='failures'><em>0</em></div>");
    }
    $("#results").append("<p id='complete'>Complete.</p>");
  });

})();
