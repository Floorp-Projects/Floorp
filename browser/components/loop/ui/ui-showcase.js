/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global Frame:false uncaughtError:true fakeManyContacts:true fakeFewerContacts:true */

(function() {
  "use strict";

  // Stop the default init functions running to avoid conflicts.
  document.removeEventListener("DOMContentLoaded", loop.panel.init);
  document.removeEventListener("DOMContentLoaded", loop.conversation.init);

  var sharedActions = loop.shared.actions;

  // 1. Desktop components
  // 1.1 Panel
  var AvailabilityDropdown = loop.panel.AvailabilityDropdown;
  var PanelView = loop.panel.PanelView;
  var SignInRequestView = loop.panel.SignInRequestView;
  var ContactDetailsForm = loop.contacts.ContactDetailsForm;
  var ContactDropdown = loop.contacts.ContactDropdown;
  var ContactDetail = loop.contacts.ContactDetail;
  var GettingStartedView = loop.panel.GettingStartedView;
  // 1.2. Conversation Window
  var AcceptCallView = loop.conversationViews.AcceptCallView;
  var DesktopPendingConversationView = loop.conversationViews.PendingConversationView;
  var OngoingConversationView = loop.conversationViews.OngoingConversationView;
  var CallFailedView = loop.conversationViews.CallFailedView;
  var DesktopRoomConversationView = loop.roomViews.DesktopRoomConversationView;

  // 2. Standalone webapp
  var HomeView = loop.webapp.HomeView;
  var UnsupportedBrowserView  = loop.webapp.UnsupportedBrowserView;
  var UnsupportedDeviceView   = loop.webapp.UnsupportedDeviceView;
  var StandaloneRoomView      = loop.standaloneRoomViews.StandaloneRoomView;

  // 3. Shared components
  var ConversationToolbar = loop.shared.views.ConversationToolbar;
  var FeedbackView = loop.feedbackViews.FeedbackView;
  var Checkbox = loop.shared.views.Checkbox;
  var TextChatView = loop.shared.views.chat.TextChatView;

  // Store constants
  var ROOM_STATES = loop.store.ROOM_STATES;
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

  var dispatcher = new loop.Dispatcher();

  var MockSDK = function() {
    dispatcher.register(this, [
      "setupStreamElements"
    ]);
  };

  MockSDK.prototype = {
    setupStreamElements: function() {
      // Dummy function to stop warnings.
    },

    sendTextChatMessage: function(actionData) {
      dispatcher.dispatch(new loop.shared.actions.ReceivedTextChatMessage({
        contentType: loop.store.CHAT_CONTENT_TYPES.TEXT,
        message: actionData.message,
        receivedTimestamp: actionData.sentTimestamp
      }));
    }
  };

  var mockSDK = new MockSDK();

  /**
   * Every view that uses an activeRoomStore needs its own; if they shared
   * an active store, they'd interfere with each other.
   *
   * @param options
   * @returns {loop.store.ActiveRoomStore}
   */
  function makeActiveRoomStore(options) {
    var roomDispatcher = new loop.Dispatcher();

    var store = new loop.store.ActiveRoomStore(roomDispatcher, {
      mozLoop: navigator.mozLoop,
      sdkDriver: mockSDK
    });

    if (!("remoteVideoEnabled" in options)) {
      options.remoteVideoEnabled = true;
    }

    if (!("mediaConnected" in options)) {
      options.mediaConnected = true;
    }

    store.setStoreState({
      mediaConnected: options.mediaConnected,
      remoteVideoEnabled: options.remoteVideoEnabled,
      roomName: "A Very Long Conversation Name",
      roomState: options.roomState,
      used: !!options.roomUsed,
      videoMuted: !!options.videoMuted
    });

    store.forcedUpdate = function forcedUpdate(contentWindow) {
      // Since this is called by setTimeout, we don't want to lose any
      // exceptions if there's a problem and we need to debug, so...
      try {
        // the dimensions here are taken from the poster images that we're
        // using, since they give the <video> elements their initial intrinsic
        // size.  This ensures that the right aspect ratios are calculated.
        // These are forced to 640x480, because it makes it visually easy to
        // validate that the showcase looks like the real app on a chine
        // (eg MacBook Pro) where that is the default camera resolution.
        var newStoreState = {
          localVideoDimensions: {
            camera: {height: 480, orientation: 0, width: 640}
          },
          mediaConnected: options.mediaConnected,
          receivingScreenShare: !!options.receivingScreenShare,
          remoteVideoDimensions: {
            camera: {height: 480, orientation: 0, width: 640}
          },
          remoteVideoEnabled: options.remoteVideoEnabled,
          // Override the matchMedia, this is so that the correct version is
          // used for the frame.
          //
          // Currently, we use an icky hack, and the showcase conspires with
          // react-frame-component to set iframe.contentWindow.matchMedia onto
          // the store. Once React context matures a bit (somewhere between
          // 0.14 and 1.0, apparently):
          //
          // https://facebook.github.io/react/blog/2015/02/24/streamlining-react-elements.html#solution-make-context-parent-based-instead-of-owner-based
          //
          // we should be able to use those to clean this up.
          matchMedia: contentWindow.matchMedia.bind(contentWindow),
          roomState: options.roomState,
          videoMuted: !!options.videoMuted
        };

        if (options.receivingScreenShare) {
          // Note that the image we're using had to be scaled a bit, and
          // it still ended up a bit narrower than the live thing that
          // WebRTC sends; presumably a different scaling algorithm.
          // For showcase purposes, this shouldn't matter much, as the sizes
          // of things being shared will be fairly arbitrary.
          newStoreState.remoteVideoDimensions.screen =
          {height: 456, orientation: 0, width: 641};
        }

        store.setStoreState(newStoreState);
      } catch (ex) {
        console.error("exception in forcedUpdate:", ex);
      }
    };

    return store;
  }

  var activeRoomStore = makeActiveRoomStore({
    roomState: ROOM_STATES.HAS_PARTICIPANTS
  });

  var joinedRoomStore = makeActiveRoomStore({
    mediaConnected: false,
    roomState: ROOM_STATES.JOINED,
    remoteVideoEnabled: false
  });

  var loadingRemoteVideoRoomStore = makeActiveRoomStore({
    mediaConnected: false,
    roomState: ROOM_STATES.HAS_PARTICIPANTS,
    remoteSrcVideoObject: false
  });

  var readyRoomStore = makeActiveRoomStore({
    mediaConnected: false,
    roomState: ROOM_STATES.READY
  });

  var updatingActiveRoomStore = makeActiveRoomStore({
    roomState: ROOM_STATES.HAS_PARTICIPANTS
  });

  var updatingMobileActiveRoomStore = makeActiveRoomStore({
    roomState: ROOM_STATES.HAS_PARTICIPANTS
  });

  var localFaceMuteRoomStore = makeActiveRoomStore({
    roomState: ROOM_STATES.HAS_PARTICIPANTS,
    videoMuted: true
  });

  var remoteFaceMuteRoomStore = makeActiveRoomStore({
    roomState: ROOM_STATES.HAS_PARTICIPANTS,
    remoteVideoEnabled: false,
    mediaConnected: true
  });

  var updatingSharingRoomStore = makeActiveRoomStore({
    roomState: ROOM_STATES.HAS_PARTICIPANTS,
    receivingScreenShare: true
  });

  var updatingSharingRoomMobileStore = makeActiveRoomStore({
    roomState: ROOM_STATES.HAS_PARTICIPANTS,
    receivingScreenShare: true
  });

  var loadingRemoteLoadingScreenStore = makeActiveRoomStore({
    mediaConnected: false,
    receivingScreenShare: true,
    roomState: ROOM_STATES.HAS_PARTICIPANTS,
    remoteSrcVideoObject: false
  });
  var loadingScreenSharingRoomStore = makeActiveRoomStore({
    receivingScreenShare: true,
    roomState: ROOM_STATES.HAS_PARTICIPANTS
  });

  /* Set up the stores for pending screen sharing */
  loadingScreenSharingRoomStore.receivingScreenShare({
    receiving: true,
    srcVideoObject: false
  });
  loadingRemoteLoadingScreenStore.receivingScreenShare({
    receiving: true,
    srcVideoObject: false
  });

  var fullActiveRoomStore = makeActiveRoomStore({
    roomState: ROOM_STATES.FULL
  });

  var failedRoomStore = makeActiveRoomStore({
    roomState: ROOM_STATES.FAILED
  });

  var endedRoomStore = makeActiveRoomStore({
    roomState: ROOM_STATES.ENDED,
    roomUsed: true
  });

  var invitationRoomStore = new loop.store.RoomStore(dispatcher, {
    mozLoop: navigator.mozLoop,
    activeRoomStore: makeActiveRoomStore({
      roomState: ROOM_STATES.INIT
    })
  });

  var roomStore = new loop.store.RoomStore(dispatcher, {
    mozLoop: navigator.mozLoop,
    activeRoomStore: makeActiveRoomStore({
      roomState: ROOM_STATES.HAS_PARTICIPANTS
    })
  });

  var desktopRoomStoreLoading = new loop.store.RoomStore(dispatcher, {
    mozLoop: navigator.mozLoop,
    activeRoomStore: makeActiveRoomStore({
      roomState: ROOM_STATES.HAS_PARTICIPANTS,
      mediaConnected: false,
      remoteSrcVideoObject: false
    })
  });

  var desktopRoomStoreMedium = new loop.store.RoomStore(dispatcher, {
    mozLoop: navigator.mozLoop,
    activeRoomStore: makeActiveRoomStore({
      roomState: ROOM_STATES.HAS_PARTICIPANTS
    })
  });

  var desktopRoomStoreLarge = new loop.store.RoomStore(dispatcher, {
    mozLoop: navigator.mozLoop,
    activeRoomStore: makeActiveRoomStore({
      roomState: ROOM_STATES.HAS_PARTICIPANTS
    })
  });

  var desktopLocalFaceMuteActiveRoomStore = makeActiveRoomStore({
    roomState: ROOM_STATES.HAS_PARTICIPANTS,
    videoMuted: true
  });
  var desktopLocalFaceMuteRoomStore = new loop.store.RoomStore(dispatcher, {
    mozLoop: navigator.mozLoop,
    activeRoomStore: desktopLocalFaceMuteActiveRoomStore
  });

  var desktopRemoteFaceMuteActiveRoomStore = makeActiveRoomStore({
    roomState: ROOM_STATES.HAS_PARTICIPANTS,
    remoteVideoEnabled: false,
    mediaConnected: true
  });
  var desktopRemoteFaceMuteRoomStore = new loop.store.RoomStore(dispatcher, {
    mozLoop: navigator.mozLoop,
    activeRoomStore: desktopRemoteFaceMuteActiveRoomStore
  });

  var textChatStore = new loop.store.TextChatStore(dispatcher, {
    sdkDriver: mockSDK
  });

  /**
   * Every view that uses an conversationStore needs its own; if they shared
   * a conversation store, they'd interfere with each other.
   *
   * @param options
   * @returns {loop.store.ConversationStore}
   */
  function makeConversationStore() {
    var roomDispatcher = new loop.Dispatcher();

    var store = new loop.store.ConversationStore(dispatcher, {
      client: {},
      mozLoop: navigator.mozLoop,
      sdkDriver: mockSDK
    });

    store.forcedUpdate = function forcedUpdate(contentWindow) {
      // Since this is called by setTimeout, we don't want to lose any
      // exceptions if there's a problem and we need to debug, so...
      try {
        var newStoreState = {
          // Override the matchMedia, this is so that the correct version is
          // used for the frame.
          //
          // Currently, we use an icky hack, and the showcase conspires with
          // react-frame-component to set iframe.contentWindow.matchMedia onto
          // the store. Once React context matures a bit (somewhere between
          // 0.14 and 1.0, apparently):
          //
          // https://facebook.github.io/react/blog/2015/02/24/streamlining-react-elements.html#solution-make-context-parent-based-instead-of-owner-based
          //
          // we should be able to use those to clean this up.
          matchMedia: contentWindow.matchMedia.bind(contentWindow)
        };

        store.setStoreState(newStoreState);
      } catch (ex) {
        console.error("exception in forcedUpdate:", ex);
      }
    };

    return store;
  }

  var conversationStores = [];
  for (var index = 0; index < 5; index++) {
    conversationStores[index] = makeConversationStore();
  }

  // Update the text chat store with the room info.
  textChatStore.updateRoomInfo(new sharedActions.UpdateRoomInfo({
    roomName: "A Very Long Conversation Name",
    roomOwner: "fake",
    roomUrl: "http://showcase",
    urls: [{
      description: "A wonderful page!",
      location: "http://wonderful.invalid"
      // use the fallback thumbnail
    }]
  }));

  textChatStore.setStoreState({textChatEnabled: true});

  dispatcher.dispatch(new sharedActions.SendTextChatMessage({
    contentType: loop.store.CHAT_CONTENT_TYPES.TEXT,
    message: "Rheet!",
    sentTimestamp: "2015-06-23T22:21:45.590Z"
  }));
  dispatcher.dispatch(new sharedActions.ReceivedTextChatMessage({
    contentType: loop.store.CHAT_CONTENT_TYPES.TEXT,
    message: "Hello",
    receivedTimestamp: "2015-06-23T23:24:45.590Z"
  }));
  dispatcher.dispatch(new sharedActions.SendTextChatMessage({
    contentType: loop.store.CHAT_CONTENT_TYPES.TEXT,
    message: "Nowforareallylongwordwithoutspacesorpunctuationwhichshouldcause" +
    "linewrappingissuesifthecssiswrong",
    sentTimestamp: "2015-06-23T22:23:45.590Z"
  }));
  dispatcher.dispatch(new sharedActions.SendTextChatMessage({
    contentType: loop.store.CHAT_CONTENT_TYPES.TEXT,
    message: "Check out this menu from DNA Pizza:" +
    " http://example.com/DNA/pizza/menu/lots-of-different-kinds-of-pizza/" +
    "%8D%E0%B8%88%E0%B8%A1%E0%B8%A3%E0%8D%E0%B8%88%E0%B8%A1%E0%B8%A3%E0%",
    sentTimestamp: "2015-06-23T22:23:45.590Z"
  }));
  dispatcher.dispatch(new sharedActions.ReceivedTextChatMessage({
    contentType: loop.store.CHAT_CONTENT_TYPES.TEXT,
    message: "That avocado monkey-brains pie sounds tasty!",
    receivedTimestamp: "2015-06-23T22:25:45.590Z"
  }));
  dispatcher.dispatch(new sharedActions.SendTextChatMessage({
    contentType: loop.store.CHAT_CONTENT_TYPES.TEXT,
    message: "What time should we meet?",
    sentTimestamp: "2015-06-23T22:27:45.590Z"
  }));
  dispatcher.dispatch(new sharedActions.ReceivedTextChatMessage({
    contentType: loop.store.CHAT_CONTENT_TYPES.TEXT,
    message: "8:00 PM",
    receivedTimestamp: "2015-06-23T22:27:45.590Z"
  }));

  loop.store.StoreMixin.register({
    activeRoomStore: activeRoomStore,
    conversationStore: conversationStores[0],
    textChatStore: textChatStore
  });

  // Local mocks
  var mockMozLoopNoRooms = _.cloneDeep(navigator.mozLoop);
  mockMozLoopNoRooms.rooms.getAll = function(version, callback) {
    callback(null, []);
  };

  var roomStoreNoRooms = new loop.store.RoomStore(new loop.Dispatcher(), {
    mozLoop: mockMozLoopNoRooms,
    activeRoomStore: new loop.store.ActiveRoomStore(new loop.Dispatcher(), {
      mozLoop: mockMozLoopNoRooms,
      sdkDriver: mockSDK
    })
  });

  var mockMozLoopLoggedIn = _.cloneDeep(navigator.mozLoop);
  mockMozLoopLoggedIn.userProfile = {
    email: "text@example.com",
    uid: "0354b278a381d3cb408bb46ffc01266"
  };

  var mockMozLoopLoggedInLongEmail = _.cloneDeep(navigator.mozLoop);
  mockMozLoopLoggedInLongEmail.userProfile = {
    email: "reallyreallylongtext@example.com",
    uid: "0354b278a381d3cb408bb46ffc01266"
  };

  var mockMozLoopRooms = _.extend({}, navigator.mozLoop);

  var mozLoopNoContacts = _.cloneDeep(navigator.mozLoop);
  mozLoopNoContacts.contacts.getAll = function(callback) {
    callback(null, []);
  };
  mozLoopNoContacts.userProfile = {
    email: "reallyreallylongtext@example.com",
    uid: "0354b278a381d3cb408bb46ffc01266"
  };
  mozLoopNoContacts.contacts.getAll = function(callback) {
    callback(null, []);
  };

  var mozLoopNoContactsFilter = _.cloneDeep(navigator.mozLoop);
  mozLoopNoContactsFilter.userProfile = {
    email: "reallyreallylongtext@example.com",
    uid: "0354b278a381d3cb408bb46ffc01266"
  };
  mozLoopNoContactsFilter.contacts.getAll = function(callback) {
    callback(null, fakeFewerContacts); // Defined in fake-mozLoop.js.
  };

  var firstTimeUseMozLoop = _.cloneDeep(navigator.mozLoop);
  firstTimeUseMozLoop.getLoopPref = function(prop) {
    if (prop === "gettingStarted.seen") {
      return false;
    }

    return true;
  };

  var mockContact = {
    name: ["Mr Smith"],
    email: [{
      value: "smith@invalid.com"
    }]
  };

  var mockClient = {
    requestCallUrlInfo: noop
  };

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
    propTypes: {
      shapeId: React.PropTypes.string.isRequired,
      size: React.PropTypes.string.isRequired
    },

    render: function() {
      var sizeUnit = this.props.size.split("x");
      return (
        React.createElement("img", {className: "svg-icon", 
             height: sizeUnit[1], 
             src: "../content/shared/img/icons-" + this.props.size + ".svg#" + this.props.shapeId, 
             width: sizeUnit[0]})
      );
    }
  });

  var SVGIcons = React.createClass({displayName: "SVGIcons",
    propTypes: {
      size: React.PropTypes.string.isRequired
    },

    shapes: {
      "10x10": ["close", "close-active", "close-disabled", "dropdown",
        "dropdown-white", "dropdown-active", "dropdown-disabled", "edit",
        "edit-active", "edit-disabled", "edit-white", "expand", "expand-active",
        "expand-disabled", "minimize", "minimize-active", "minimize-disabled",
        "settings-cog-grey", "settings-cog-white"
      ],
      "14x14": ["audio", "audio-active", "audio-disabled", "facemute",
        "facemute-active", "facemute-disabled", "hangup", "hangup-active",
        "hangup-disabled", "hello", "hello-hover", "hello-active",
        "incoming", "incoming-active", "incoming-disabled",
        "link", "link-active", "link-disabled", "mute", "mute-active",
        "mute-disabled", "pause", "pause-active", "pause-disabled", "video",
        "video-white", "video-active", "video-disabled", "volume", "volume-active",
        "volume-disabled", "clear", "magnifier"
      ],
      "16x16": ["add", "add-hover", "add-active", "audio", "audio-hover", "audio-active",
        "block", "block-red", "block-hover", "block-active", "contacts", "contacts-hover",
        "contacts-active", "copy", "checkmark", "delete", "globe", "google", "google-hover",
        "google-active", "history", "history-hover", "history-active", "leave",
        "screen-white", "screenmute-white", "settings", "settings-hover", "settings-active",
        "share-darkgrey", "tag", "tag-hover", "tag-active", "trash", "unblock",
        "unblock-hover", "unblock-active", "video", "video-hover", "video-active", "tour",
        "status-available", "status-unavailable"
      ]
    },

    render: function() {
      var icons = this.shapes[this.props.size].map(function(shapeId, i) {
        return (
          React.createElement("li", {className: "svg-icon-entry", key: this.props.size + "-" + i}, 
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

  var FramedExample = React.createClass({displayName: "FramedExample",
    propTypes: {
      children: React.PropTypes.element,
      cssClass: React.PropTypes.string,
      dashed: React.PropTypes.bool,
      height: React.PropTypes.number,
      onContentsRendered: React.PropTypes.func,
      summary: React.PropTypes.string.isRequired,
      width: React.PropTypes.number
    },

    makeId: function(prefix) {
      return (prefix || "") + this.props.summary.toLowerCase().replace(/\s/g, "-");
    },

    render: function() {
      var height = this.props.height;
      var width = this.props.width;

      // make room for a 1-pixel border on each edge
      if (this.props.dashed) {
        height += 2;
        width += 2;
      }

      var cx = React.addons.classSet;
      return (
        React.createElement("div", {className: "example"}, 
          React.createElement("h3", {id: this.makeId()}, 
            this.props.summary, 
            React.createElement("a", {href: this.makeId("#")}, " ¶")
          ), 
          React.createElement("div", {className: "comp"}, 
            React.createElement(Frame, {className: cx({dashed: this.props.dashed}), 
                   cssClass: this.props.cssClass, 
                   height: height, 
                   onContentsRendered: this.props.onContentsRendered, 
                   width: width}, 
              this.props.children
            )
          )
        )
      );
    }
  });

  var Section = React.createClass({displayName: "Section",
    propTypes: {
      children: React.PropTypes.oneOfType([
        React.PropTypes.arrayOf(React.PropTypes.element),
        React.PropTypes.element
      ]).isRequired,
      className: React.PropTypes.string,
      name: React.PropTypes.string.isRequired
    },

    render: function() {
      return (
        React.createElement("section", {className: this.props.className, id: this.props.name}, 
          React.createElement("h1", null, this.props.name), 
          this.props.children
        )
      );
    }
  });

  var ShowCase = React.createClass({displayName: "ShowCase",
    propTypes: {
      children: React.PropTypes.arrayOf(React.PropTypes.element).isRequired
    },

    getInitialState: function() {
      // We assume for now that rtl is the only query parameter.
      //
      // Note: this check is repeated in react-frame-component to save passing
      // rtlMode down the props tree.
      var rtlMode = document.location.search === "?rtl=1";

      return {
        rtlMode: rtlMode
      };
    },

    _handleCheckboxChange: function(newState) {
      var newLocation = "";
      if (newState.checked) {
        newLocation = document.location.href.split("#")[0];
        newLocation += "?rtl=1";
      } else {
        newLocation = document.location.href.split("?")[0];
      }
      newLocation += document.location.hash;
      document.location = newLocation;
    },

    render: function() {
      if (this.state.rtlMode) {
        document.documentElement.setAttribute("lang", "ar");
        document.documentElement.setAttribute("dir", "rtl");
      }

      return (
        React.createElement("div", {className: "showcase"}, 
          React.createElement("header", null, 
            React.createElement("h1", null, "Loop UI Components Showcase"), 
            React.createElement(Checkbox, {checked: this.state.rtlMode, label: "RTL mode?", 
              onChange: this._handleCheckboxChange}), 
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
            React.createElement(FramedExample, {cssClass: "fx-embedded-panel", 
                           dashed: true, 
                           height: 410, 
                           summary: "Re-sign-in view", 
                           width: 332}, 
              React.createElement("div", {className: "panel"}, 
                React.createElement(PanelView, {client: mockClient, 
                  dispatcher: dispatcher, 
                  mozLoop: firstTimeUseMozLoop, 
                  notifications: notifications, 
                  roomStore: roomStore, 
                  selectedTab: "rooms"})
              )
            ), 

            React.createElement(FramedExample, {cssClass: "fx-embedded-panel", 
              dashed: true, 
              height: 410, 
              summary: "Re-sign-in view", 
              width: 332}, 
              React.createElement("div", {className: "panel"}, 
                React.createElement(SignInRequestView, {mozLoop: mockMozLoopLoggedIn})
              )
            ), 

            React.createElement(FramedExample, {cssClass: "fx-embedded-panel", 
                           dashed: true, 
                           height: 410, 
                           summary: "Room list tab", 
                           width: 332}, 
              React.createElement("div", {className: "panel"}, 
                React.createElement(PanelView, {client: mockClient, 
                           dispatcher: dispatcher, 
                           mozLoop: mockMozLoopLoggedIn, 
                           notifications: notifications, 
                           roomStore: roomStore, 
                           selectedTab: "rooms"})
              )
            ), 

            React.createElement(FramedExample, {cssClass: "fx-embedded-panel", 
                           dashed: true, 
                           height: 410, 
                           summary: "Room list tab (no rooms)", 
                           width: 332}, 
              React.createElement("div", {className: "panel"}, 
                React.createElement(PanelView, {client: mockClient, 
                           dispatcher: dispatcher, 
                           mozLoop: mockMozLoopNoRooms, 
                           notifications: notifications, 
                           roomStore: roomStoreNoRooms, 
                           selectedTab: "rooms"})
              )
            ), 

            React.createElement(FramedExample, {cssClass: "fx-embedded-panel", 
                           dashed: true, 
                           height: 410, 
                           summary: "Contact list tab", 
                           width: 332}, 
              React.createElement("div", {className: "panel"}, 
                React.createElement(PanelView, {client: mockClient, 
                           dispatcher: dispatcher, 
                           mozLoop: mockMozLoopLoggedIn, 
                           notifications: notifications, 
                           roomStore: roomStore, 
                           selectedTab: "contacts"})
              )
            ), 
            React.createElement(FramedExample, {cssClass: "fx-embedded-panel", 
                           dashed: true, 
                           height: 410, 
                           summary: "Contact list tab (no search filter)", 
                           width: 332}, 
              React.createElement("div", {className: "panel"}, 
                React.createElement(PanelView, {client: mockClient, 
                           dispatcher: dispatcher, 
                           mozLoop: mozLoopNoContactsFilter, 
                           notifications: notifications, 
                           roomStore: roomStore, 
                           selectedTab: "contacts"})
              )
            ), 
            React.createElement(FramedExample, {cssClass: "fx-embedded-panel", 
                           dashed: true, 
                           height: 410, 
                           summary: "Contact list tab long email", 
                           width: 332}, 
              React.createElement("div", {className: "panel"}, 
                React.createElement(PanelView, {client: mockClient, 
                           dispatcher: dispatcher, 
                           mozLoop: mockMozLoopLoggedInLongEmail, 
                           notifications: notifications, 
                           roomStore: roomStore, 
                           selectedTab: "contacts"})
              )
            ), 
            React.createElement(FramedExample, {cssClass: "fx-embedded-panel", 
                           dashed: true, 
                           height: 410, 
                           summary: "Contact list tab (no contacts)", 
                           width: 332}, 
              React.createElement("div", {className: "panel"}, 
                React.createElement(PanelView, {client: mockClient, 
                           dispatcher: dispatcher, 
                           mozLoop: mozLoopNoContacts, 
                           notifications: notifications, 
                           roomStore: roomStore, 
                           selectedTab: "contacts"})
              )
            ), 
            React.createElement(FramedExample, {cssClass: "fx-embedded-panel", 
                           dashed: true, 
                           height: 410, 
                           summary: "Error Notification", 
                           width: 332}, 
              React.createElement("div", {className: "panel"}, 
                React.createElement(PanelView, {client: mockClient, 
                           dispatcher: dispatcher, 
                           mozLoop: navigator.mozLoop, 
                           notifications: errNotifications, 
                           roomStore: roomStore})
              )
            ), 
            React.createElement(FramedExample, {cssClass: "fx-embedded-panel", 
                           dashed: true, 
                           height: 410, 
                           summary: "Error Notification - authenticated", 
                           width: 332}, 
              React.createElement("div", {className: "panel"}, 
                React.createElement(PanelView, {client: mockClient, 
                           dispatcher: dispatcher, 
                           mozLoop: mockMozLoopLoggedIn, 
                           notifications: errNotifications, 
                           roomStore: roomStore})
              )
            ), 
            React.createElement(FramedExample, {cssClass: "fx-embedded-panel", 
                           dashed: true, 
                           height: 410, 
                           summary: "Contact import success", 
                           width: 332}, 
              React.createElement("div", {className: "panel"}, 
                React.createElement(PanelView, {dispatcher: dispatcher, 
                           mozLoop: mockMozLoopLoggedIn, 
                           notifications: new loop.shared.models.NotificationCollection([{level: "success", message: "Import success"}]), 
                           roomStore: roomStore, 
                           selectedTab: "contacts"})
              )
            ), 
            React.createElement(FramedExample, {cssClass: "fx-embedded-panel", 
                           dashed: true, 
                           height: 410, 
                           summary: "Contact import error", 
                           width: 332}, 
              React.createElement("div", {className: "panel"}, 
                React.createElement(PanelView, {dispatcher: dispatcher, 
                           mozLoop: mockMozLoopLoggedIn, 
                           notifications: new loop.shared.models.NotificationCollection([{level: "error", message: "Import error"}]), 
                           roomStore: roomStore, 
                           selectedTab: "contacts"})
              )
            ), 
            React.createElement(FramedExample, {cssClass: "fx-embedded-panel", 
                           dashed: true, 
                           height: 410, 
                           summary: "Contact Form - Add", 
                           width: 332}, 
              React.createElement("div", {className: "panel"}, 
                React.createElement(PanelView, {client: mockClient, 
                           dispatcher: dispatcher, 
                           initialSelectedTabComponent: "contactAdd", 
                           mozLoop: mockMozLoopLoggedIn, 
                           notifications: notifications, 
                           roomStore: roomStore, 
                           selectedTab: "contacts", 
                           userProfile: {email: "test@example.com"}})
              )
            ), 
            React.createElement(FramedExample, {cssClass: "fx-embedded-panel", 
                           dashed: true, 
                           height: 321, 
                           summary: "Contact Form - Edit", 
                           width: 332}, 
              React.createElement("div", {className: "panel"}, 
                React.createElement(ContactDetailsForm, {contactFormData: fakeManyContacts[1], 
                                    mode: "edit", 
                                    mozLoop: mockMozLoopLoggedIn, 
                                    switchToInitialView: noop})
              )
            )
          ), 

          React.createElement(Section, {name: "Availability Dropdown"}, 
            React.createElement("p", {className: "note"}, 
              React.createElement("strong", null, "Note:"), " 332px wide."
            ), 
            React.createElement(FramedExample, {cssClass: "fx-embedded-panel", 
                           dashed: true, 
                           height: 200, 
                           summary: "AvailabilityDropdown", 
                           width: 332}, 
              React.createElement("div", {className: "panel"}, 
                React.createElement(AvailabilityDropdown, null)
              )
            ), 

            React.createElement(FramedExample, {cssClass: "fx-embedded-panel", 
                           dashed: true, 
                           height: 200, 
                           summary: "AvailabilityDropdown Expanded", 
                           width: 332}, 
              React.createElement("div", {className: "panel force-menu-show", style: {"height": "100%", "paddingTop": "50px"}}, 
                React.createElement(AvailabilityDropdown, null)
              )
            )
          ), 

          React.createElement(Section, {name: "ContactDetail"}, 
            React.createElement(FramedExample, {cssClass: "fx-embedded-panel", 
                           dashed: true, 
                           height: 272, 
                           summary: "ContactDetail", 
                           width: 300}, 
              React.createElement("div", {className: "panel force-menu-show"}, 
                React.createElement(ContactDetail, {contact: fakeManyContacts[0], 
                               handleContactAction: function() {}})
              )
            )
          ), 

          React.createElement(Section, {name: "ContactDropdown"}, 
            React.createElement(FramedExample, {cssClass: "fx-embedded-panel", 
                           dashed: true, 
                           height: 272, 
                           summary: "ContactDropdown not blocked can edit", 
                           width: 300}, 
             React.createElement("div", {className: "panel"}, 
               React.createElement(ContactDropdown, {blocked: false, 
                                canEdit: true, 
                                handleAction: function () {}})
             )
            ), 
            React.createElement(FramedExample, {cssClass: "fx-embedded-panel", 
                           dashed: true, 
                           height: 272, 
                           summary: "ContactDropdown blocked can't edit", 
                           width: 300}, 
             React.createElement("div", {className: "panel"}, 
               React.createElement(ContactDropdown, {blocked: true, 
                 canEdit: false, 
                 handleAction: function () {}})
             )
            )
          ), 

          React.createElement(Section, {name: "AcceptCallView"}, 
            React.createElement(FramedExample, {dashed: true, 
                           height: 272, 
                           summary: "Default / incoming video call", 
                           width: 332}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(AcceptCallView, {callType: CALL_TYPES.AUDIO_VIDEO, 
                                callerId: "Mr Smith", 
                                dispatcher: dispatcher, 
                                mozLoop: mockMozLoopLoggedIn})
              )
            ), 

            React.createElement(FramedExample, {dashed: true, 
                           height: 272, 
                           summary: "Default / incoming audio only call", 
                           width: 332}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(AcceptCallView, {callType: CALL_TYPES.AUDIO_ONLY, 
                                callerId: "Mr Smith", 
                                dispatcher: dispatcher, 
                                mozLoop: mockMozLoopLoggedIn})
              )
            )
          ), 

          React.createElement(Section, {name: "AcceptCallView-ActiveState"}, 
            React.createElement(FramedExample, {dashed: true, 
                           height: 272, 
                           summary: "Default", 
                           width: 332}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(AcceptCallView, {callType: CALL_TYPES.AUDIO_VIDEO, 
                                callerId: "Mr Smith", 
                                dispatcher: dispatcher, 
                                mozLoop: mockMozLoopLoggedIn, 
                                showMenu: true})
              )
            )
          ), 

          React.createElement(Section, {name: "ConversationToolbar"}, 
            React.createElement("div", null, 
              React.createElement(FramedExample, {dashed: true, 
                             height: 56, 
                             summary: "Default", 
                             width: 300}, 
                React.createElement("div", {className: "fx-embedded"}, 
                  React.createElement(ConversationToolbar, {audio: { enabled: true, visible: true}, 
                                       hangup: noop, 
                                       publishStream: noop, 
                                       video: { enabled: true, visible: true}})
                )
              ), 
              React.createElement(FramedExample, {dashed: true, 
                             height: 56, 
                             summary: "Video muted", 
                             width: 300}, 
                React.createElement("div", {className: "fx-embedded"}, 
                  React.createElement(ConversationToolbar, {audio: { enabled: true, visible: true}, 
                                       hangup: noop, 
                                       publishStream: noop, 
                                       video: { enabled: false, visible: true}})
                )
              ), 
              React.createElement(FramedExample, {dashed: true, 
                             height: 56, 
                             summary: "Audio muted", 
                             width: 300}, 
                React.createElement("div", {className: "fx-embedded"}, 
                  React.createElement(ConversationToolbar, {audio: { enabled: false, visible: true}, 
                                       hangup: noop, 
                                       publishStream: noop, 
                                       video: { enabled: true, visible: true}})
                )
              )
            )
          ), 

          React.createElement(Section, {name: "PendingConversationView (Desktop)"}, 
            React.createElement(FramedExample, {dashed: true, 
                           height: 272, 
                           summary: "Connecting", 
                           width: 300}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(DesktopPendingConversationView, {callState: "gather", 
                                                contact: mockContact, 
                                                dispatcher: dispatcher})
              )
            )
          ), 

          React.createElement(Section, {name: "CallFailedView"}, 
            React.createElement(FramedExample, {dashed: true, 
                           height: 272, 
                           summary: "Call Failed - Incoming", 
                           width: 300}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(CallFailedView, {dispatcher: dispatcher, 
                                outgoing: false, 
                                store: conversationStores[0]})
              )
            ), 
            React.createElement(FramedExample, {dashed: true, 
                           height: 272, 
                           summary: "Call Failed - Outgoing", 
                           width: 300}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(CallFailedView, {dispatcher: dispatcher, 
                                outgoing: true, 
                                store: conversationStores[1]})
              )
            ), 
            React.createElement(FramedExample, {dashed: true, 
                           height: 272, 
                           summary: "Call Failed — with call URL error", 
                           width: 300}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(CallFailedView, {dispatcher: dispatcher, emailLinkError: true, 
                                outgoing: true, 
                                store: conversationStores[0]})
              )
            )
          ), 

          React.createElement(Section, {name: "OngoingConversationView"}, 
            React.createElement(FramedExample, {dashed: true, 
                           height: 394, 
                           onContentsRendered: conversationStores[0].forcedUpdate, 
                           summary: "Desktop ongoing conversation window", 
                           width: 298}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(OngoingConversationView, {
                  audio: { enabled: true, visible: true}, 
                  conversationStore: conversationStores[0], 
                  dispatcher: dispatcher, 
                  localPosterUrl: "sample-img/video-screen-local.png", 
                  mediaConnected: true, 
                  remotePosterUrl: "sample-img/video-screen-remote.png", 
                  remoteVideoEnabled: true, 
                  video: { enabled: true, visible: true}})
              )
            ), 

            React.createElement(FramedExample, {dashed: true, 
                           height: 400, 
                           onContentsRendered: conversationStores[1].forcedUpdate, 
                           summary: "Desktop ongoing conversation window (medium)", 
                           width: 600}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(OngoingConversationView, {
                  audio: { enabled: true, visible: true}, 
                  conversationStore: conversationStores[1], 
                  dispatcher: dispatcher, 
                  localPosterUrl: "sample-img/video-screen-local.png", 
                  mediaConnected: true, 
                  remotePosterUrl: "sample-img/video-screen-remote.png", 
                  remoteVideoEnabled: true, 
                  video: { enabled: true, visible: true}})
              )
            ), 

            React.createElement(FramedExample, {height: 600, 
                           onContentsRendered: conversationStores[2].forcedUpdate, 
                           summary: "Desktop ongoing conversation window (large)", 
                           width: 800}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(OngoingConversationView, {
                  audio: { enabled: true, visible: true}, 
                  conversationStore: conversationStores[2], 
                  dispatcher: dispatcher, 
                  localPosterUrl: "sample-img/video-screen-local.png", 
                  mediaConnected: true, 
                  remotePosterUrl: "sample-img/video-screen-remote.png", 
                  remoteVideoEnabled: true, 
                  video: { enabled: true, visible: true}})
              )
            ), 

            React.createElement(FramedExample, {dashed: true, 
                           height: 394, 
                           onContentsRendered: conversationStores[3].forcedUpdate, 
                           summary: "Desktop ongoing conversation window - local face mute", 
                           width: 298}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(OngoingConversationView, {
                  audio: { enabled: true, visible: true}, 
                  conversationStore: conversationStores[3], 
                  dispatcher: dispatcher, 
                  localPosterUrl: "sample-img/video-screen-local.png", 
                  mediaConnected: true, 
                  remotePosterUrl: "sample-img/video-screen-remote.png", 
                  remoteVideoEnabled: true, 
                  video: { enabled: true, visible: true}})
              )
            ), 

            React.createElement(FramedExample, {dashed: true, 
                           height: 394, 
                           onContentsRendered: conversationStores[4].forcedUpdate, 
                           summary: "Desktop ongoing conversation window - remote face mute", 
                           width: 298}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(OngoingConversationView, {
                  audio: { enabled: true, visible: true}, 
                  conversationStore: conversationStores[4], 
                  dispatcher: dispatcher, 
                  localPosterUrl: "sample-img/video-screen-local.png", 
                  mediaConnected: true, 
                  remotePosterUrl: "sample-img/video-screen-remote.png", 
                  remoteVideoEnabled: false, 
                  video: { enabled: true, visible: true}})
              )
            )

          ), 

          React.createElement(Section, {name: "FeedbackView"}, 
            React.createElement("p", {className: "note"}
            ), 
            React.createElement(FramedExample, {dashed: true, 
                           height: 272, 
                           summary: "Default (useable demo)", 
                           width: 300}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(FeedbackView, {mozLoop: {}, 
                              onAfterFeedbackReceived: function() {}})
              )
            )
          ), 

          React.createElement(Section, {name: "AlertMessages"}, 
            React.createElement(FramedExample, {dashed: true, 
                           height: 272, 
                           summary: "Various alerts", 
                           width: 300}, 
              React.createElement("div", null, 
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
            )
          ), 

          React.createElement(Section, {name: "UnsupportedBrowserView"}, 
            React.createElement(FramedExample, {dashed: true, 
                           height: 430, 
                           summary: "Standalone Unsupported Browser", 
                           width: 480}, 
              React.createElement("div", {className: "standalone"}, 
                React.createElement(UnsupportedBrowserView, {isFirefox: false})
              )
            )
          ), 

          React.createElement(Section, {name: "UnsupportedDeviceView"}, 
            React.createElement(FramedExample, {dashed: true, 
                           height: 430, 
                           summary: "Standalone Unsupported Device", 
                           width: 480}, 
              React.createElement("div", {className: "standalone"}, 
                React.createElement(UnsupportedDeviceView, {platform: "ios"})
              )
            )
          ), 

          React.createElement(Section, {name: "DesktopRoomConversationView"}, 
            React.createElement(FramedExample, {height: 398, 
                           onContentsRendered: invitationRoomStore.activeRoomStore.forcedUpdate, 
                           summary: "Desktop room conversation (invitation, text-chat inclusion/scrollbars don't happen in real client)", 
                           width: 298}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(DesktopRoomConversationView, {
                  dispatcher: dispatcher, 
                  localPosterUrl: "sample-img/video-screen-local.png", 
                  mozLoop: navigator.mozLoop, 
                  onCallTerminated: function(){}, 
                  roomState: ROOM_STATES.INIT, 
                  roomStore: invitationRoomStore})
              )
            ), 

            React.createElement(FramedExample, {dashed: true, 
                           height: 394, 
                           onContentsRendered: desktopRoomStoreLoading.activeRoomStore.forcedUpdate, 
                           summary: "Desktop room conversation (loading)", 
                           width: 298}, 
              /* Hide scrollbars here. Rotating loading div overflows and causes
               scrollbars to appear */
              React.createElement("div", {className: "fx-embedded overflow-hidden"}, 
                React.createElement(DesktopRoomConversationView, {
                  dispatcher: dispatcher, 
                  localPosterUrl: "sample-img/video-screen-local.png", 
                  mozLoop: navigator.mozLoop, 
                  onCallTerminated: function(){}, 
                  remotePosterUrl: "sample-img/video-screen-remote.png", 
                  roomState: ROOM_STATES.HAS_PARTICIPANTS, 
                  roomStore: desktopRoomStoreLoading})
              )
            ), 

            React.createElement(FramedExample, {dashed: true, 
                           height: 394, 
                           onContentsRendered: roomStore.activeRoomStore.forcedUpdate, 
                           summary: "Desktop room conversation", 
                           width: 298}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(DesktopRoomConversationView, {
                  dispatcher: dispatcher, 
                  localPosterUrl: "sample-img/video-screen-local.png", 
                  mozLoop: navigator.mozLoop, 
                  onCallTerminated: function(){}, 
                  remotePosterUrl: "sample-img/video-screen-remote.png", 
                  roomState: ROOM_STATES.HAS_PARTICIPANTS, 
                  roomStore: roomStore})
              )
            ), 

            React.createElement(FramedExample, {dashed: true, 
                           height: 482, 
                           onContentsRendered: desktopRoomStoreMedium.activeRoomStore.forcedUpdate, 
                           summary: "Desktop room conversation (medium)", 
                           width: 602}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(DesktopRoomConversationView, {
                  dispatcher: dispatcher, 
                  localPosterUrl: "sample-img/video-screen-local.png", 
                  mozLoop: navigator.mozLoop, 
                  onCallTerminated: function(){}, 
                  remotePosterUrl: "sample-img/video-screen-remote.png", 
                  roomState: ROOM_STATES.HAS_PARTICIPANTS, 
                  roomStore: desktopRoomStoreMedium})
              )
            ), 

            React.createElement(FramedExample, {dashed: true, 
                           height: 485, 
                           onContentsRendered: desktopRoomStoreLarge.activeRoomStore.forcedUpdate, 
                           summary: "Desktop room conversation (large)", 
                           width: 646}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(DesktopRoomConversationView, {
                  dispatcher: dispatcher, 
                  localPosterUrl: "sample-img/video-screen-local.png", 
                  mozLoop: navigator.mozLoop, 
                  onCallTerminated: function(){}, 
                  remotePosterUrl: "sample-img/video-screen-remote.png", 
                  roomState: ROOM_STATES.HAS_PARTICIPANTS, 
                  roomStore: desktopRoomStoreLarge})
              )
            ), 

            React.createElement(FramedExample, {dashed: true, 
                           height: 394, 
                           onContentsRendered: desktopLocalFaceMuteRoomStore.activeRoomStore.forcedUpdate, 
                           summary: "Desktop room conversation local face-mute", 
                           width: 298}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(DesktopRoomConversationView, {
                  dispatcher: dispatcher, 
                  mozLoop: navigator.mozLoop, 
                  onCallTerminated: function(){}, 
                  remotePosterUrl: "sample-img/video-screen-remote.png", 
                  roomStore: desktopLocalFaceMuteRoomStore})
              )
            ), 

            React.createElement(FramedExample, {dashed: true, 
                           height: 394, 
                           onContentsRendered: desktopRemoteFaceMuteRoomStore.activeRoomStore.forcedUpdate, 
                           summary: "Desktop room conversation remote face-mute", 
                           width: 298}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(DesktopRoomConversationView, {
                  dispatcher: dispatcher, 
                  localPosterUrl: "sample-img/video-screen-local.png", 
                  mozLoop: navigator.mozLoop, 
                  onCallTerminated: function(){}, 
                  remotePosterUrl: "sample-img/video-screen-remote.png", 
                  roomStore: desktopRemoteFaceMuteRoomStore})
              )
            )
          ), 

          React.createElement(Section, {name: "StandaloneRoomView"}, 
            React.createElement(FramedExample, {cssClass: "standalone", 
                           dashed: true, 
                           height: 483, 
                           summary: "Standalone room conversation (ready)", 
                           width: 644}, 
              React.createElement("div", {className: "standalone"}, 
                React.createElement(StandaloneRoomView, {
                  activeRoomStore: readyRoomStore, 
                  dispatcher: dispatcher, 
                  isFirefox: true, 
                  roomState: ROOM_STATES.READY})
              )
            ), 

            React.createElement(FramedExample, {cssClass: "standalone", 
                           dashed: true, 
                           height: 483, 
                           onContentsRendered: joinedRoomStore.forcedUpdate, 
                           summary: "Standalone room conversation (joined)", 
                           width: 644}, 
              React.createElement("div", {className: "standalone"}, 
                React.createElement(StandaloneRoomView, {
                  activeRoomStore: joinedRoomStore, 
                  dispatcher: dispatcher, 
                  isFirefox: true, 
                  localPosterUrl: "sample-img/video-screen-local.png"})
              )
            ), 

            React.createElement(FramedExample, {cssClass: "standalone", 
                           dashed: true, 
                           height: 483, 
                           onContentsRendered: loadingRemoteVideoRoomStore.forcedUpdate, 
                           summary: "Standalone room conversation (loading remote)", 
                           width: 644}, 
              React.createElement("div", {className: "standalone"}, 
                React.createElement(StandaloneRoomView, {
                  activeRoomStore: loadingRemoteVideoRoomStore, 
                  dispatcher: dispatcher, 
                  isFirefox: true, 
                  localPosterUrl: "sample-img/video-screen-local.png"})
              )
            ), 

            React.createElement(FramedExample, {cssClass: "standalone", 
                           dashed: true, 
                           height: 483, 
                           onContentsRendered: updatingActiveRoomStore.forcedUpdate, 
                           summary: "Standalone room conversation (has-participants, 644x483)", 
                           width: 644}, 
                React.createElement("div", {className: "standalone"}, 
                  React.createElement(StandaloneRoomView, {
                    activeRoomStore: updatingActiveRoomStore, 
                    dispatcher: dispatcher, 
                    isFirefox: true, 
                    localPosterUrl: "sample-img/video-screen-local.png", 
                    remotePosterUrl: "sample-img/video-screen-remote.png", 
                    roomState: ROOM_STATES.HAS_PARTICIPANTS})
                )
            ), 

            React.createElement(FramedExample, {cssClass: "standalone", 
                           dashed: true, 
                           height: 483, 
                           onContentsRendered: localFaceMuteRoomStore.forcedUpdate, 
                           summary: "Standalone room conversation (local face mute, has-participants, 644x483)", 
                           width: 644}, 
              React.createElement("div", {className: "standalone"}, 
                React.createElement(StandaloneRoomView, {
                  activeRoomStore: localFaceMuteRoomStore, 
                  dispatcher: dispatcher, 
                  isFirefox: true, 
                  localPosterUrl: "sample-img/video-screen-local.png", 
                  remotePosterUrl: "sample-img/video-screen-remote.png"})
              )
            ), 

            React.createElement(FramedExample, {cssClass: "standalone", 
                           dashed: true, 
                           height: 483, 
                           onContentsRendered: remoteFaceMuteRoomStore.forcedUpdate, 
                           summary: "Standalone room conversation (remote face mute, has-participants, 644x483)", 
                           width: 644}, 
              React.createElement("div", {className: "standalone"}, 
                React.createElement(StandaloneRoomView, {
                  activeRoomStore: remoteFaceMuteRoomStore, 
                  dispatcher: dispatcher, 
                  isFirefox: true, 
                  localPosterUrl: "sample-img/video-screen-local.png", 
                  remotePosterUrl: "sample-img/video-screen-remote.png"})
              )
            ), 

            React.createElement(FramedExample, {cssClass: "standalone", 
                           dashed: true, 
                           height: 660, 
                           onContentsRendered: loadingRemoteLoadingScreenStore.forcedUpdate, 
                           summary: "Standalone room convo (has-participants, loading screen share, loading remote video, 800x660)", 
                           width: 800}, 
              /* Hide scrollbars here. Rotating loading div overflows and causes
               scrollbars to appear */
               React.createElement("div", {className: "standalone overflow-hidden"}, 
                  React.createElement(StandaloneRoomView, {
                    activeRoomStore: loadingRemoteLoadingScreenStore, 
                    dispatcher: dispatcher, 
                    isFirefox: true, 
                    localPosterUrl: "sample-img/video-screen-local.png", 
                    remotePosterUrl: "sample-img/video-screen-remote.png", 
                    roomState: ROOM_STATES.HAS_PARTICIPANTS})
                )
            ), 

            React.createElement(FramedExample, {cssClass: "standalone", 
                           dashed: true, 
                           height: 660, 
                           onContentsRendered: loadingScreenSharingRoomStore.forcedUpdate, 
                           summary: "Standalone room convo (has-participants, loading screen share, 800x660)", 
                           width: 800}, 
              /* Hide scrollbars here. Rotating loading div overflows and causes
               scrollbars to appear */
               React.createElement("div", {className: "standalone overflow-hidden"}, 
                  React.createElement(StandaloneRoomView, {
                    activeRoomStore: loadingScreenSharingRoomStore, 
                    dispatcher: dispatcher, 
                    isFirefox: true, 
                    localPosterUrl: "sample-img/video-screen-local.png", 
                    remotePosterUrl: "sample-img/video-screen-remote.png", 
                    roomState: ROOM_STATES.HAS_PARTICIPANTS})
                )
            ), 

            React.createElement(FramedExample, {cssClass: "standalone", 
                           dashed: true, 
                           height: 660, 
                           onContentsRendered: updatingSharingRoomStore.forcedUpdate, 
                           summary: "Standalone room convo (has-participants, receivingScreenShare, 800x660)", 
                           width: 800}, 
                React.createElement("div", {className: "standalone"}, 
                  React.createElement(StandaloneRoomView, {
                    activeRoomStore: updatingSharingRoomStore, 
                    dispatcher: dispatcher, 
                    isFirefox: true, 
                    localPosterUrl: "sample-img/video-screen-local.png", 
                    remotePosterUrl: "sample-img/video-screen-remote.png", 
                    roomState: ROOM_STATES.HAS_PARTICIPANTS, 
                    screenSharePosterUrl: "sample-img/video-screen-terminal.png"})
                )
            ), 

            React.createElement(FramedExample, {cssClass: "standalone", 
                           dashed: true, 
                           height: 483, 
                           summary: "Standalone room conversation (full - FFx user)", 
                           width: 644}, 
              React.createElement("div", {className: "standalone"}, 
                React.createElement(StandaloneRoomView, {
                  activeRoomStore: fullActiveRoomStore, 
                  dispatcher: dispatcher, 
                  isFirefox: true})
              )
            ), 

            React.createElement(FramedExample, {cssClass: "standalone", 
                           dashed: true, 
                           height: 483, 
                           summary: "Standalone room conversation (full - non FFx user)", 
                           width: 644}, 
              React.createElement("div", {className: "standalone"}, 
                React.createElement(StandaloneRoomView, {
                  activeRoomStore: fullActiveRoomStore, 
                  dispatcher: dispatcher, 
                  isFirefox: false})
              )
            ), 

            React.createElement(FramedExample, {cssClass: "standalone", 
                           dashed: true, 
                           height: 483, 
                           summary: "Standalone room conversation (failed)", 
                           width: 644}, 
              React.createElement("div", {className: "standalone"}, 
                React.createElement(StandaloneRoomView, {
                  activeRoomStore: failedRoomStore, 
                  dispatcher: dispatcher, 
                  isFirefox: false})
              )
            )
          ), 

          React.createElement(Section, {name: "StandaloneRoomView (Mobile)"}, 
            React.createElement(FramedExample, {cssClass: "standalone", 
                           dashed: true, 
                           height: 480, 
                           onContentsRendered: updatingMobileActiveRoomStore.forcedUpdate, 
                           summary: "Standalone room conversation (has-participants, 600x480)", 
                           width: 600}, 
                React.createElement("div", {className: "standalone"}, 
                  React.createElement(StandaloneRoomView, {
                    activeRoomStore: updatingMobileActiveRoomStore, 
                    dispatcher: dispatcher, 
                    isFirefox: true, 
                    localPosterUrl: "sample-img/video-screen-local.png", 
                    remotePosterUrl: "sample-img/video-screen-remote.png", 
                    roomState: ROOM_STATES.HAS_PARTICIPANTS})
                )
            ), 

            React.createElement(FramedExample, {cssClass: "standalone", 
                           dashed: true, 
                           height: 480, 
                           onContentsRendered: updatingSharingRoomMobileStore.forcedUpdate, 
                           summary: "Standalone room convo (has-participants, receivingScreenShare, 600x480)", 
                           width: 600}, 
                React.createElement("div", {className: "standalone", cssClass: "standalone"}, 
                  React.createElement(StandaloneRoomView, {
                    activeRoomStore: updatingSharingRoomMobileStore, 
                    dispatcher: dispatcher, 
                    isFirefox: true, 
                    localPosterUrl: "sample-img/video-screen-local.png", 
                    remotePosterUrl: "sample-img/video-screen-remote.png", 
                    roomState: ROOM_STATES.HAS_PARTICIPANTS, 
                    screenSharePosterUrl: "sample-img/video-screen-terminal.png"})
                )
            )
          ), 

          React.createElement(Section, {name: "TextChatView"}, 
            React.createElement(FramedExample, {dashed: true, 
                           height: 160, 
                           summary: "TextChatView: desktop embedded", 
                           width: 298}, 
              React.createElement("div", {className: "fx-embedded"}, 
                React.createElement(TextChatView, {dispatcher: dispatcher, 
                              showRoomName: false, 
                              useDesktopPaths: false})
              )
            ), 

            React.createElement(FramedExample, {cssClass: "standalone", 
                           dashed: true, 
                           height: 400, 
                           summary: "Standalone Text Chat conversation (200x400)", 
                           width: 200}, 
              React.createElement("div", {className: "standalone text-chat-example"}, 
                React.createElement("div", {className: "media-wrapper"}, 
                  React.createElement(TextChatView, {
                    dispatcher: dispatcher, 
                    showRoomName: true, 
                    useDesktopPaths: false})
                )
              )
            )
          ), 

          React.createElement(Section, {className: "svg-icons", name: "SVG icons preview"}, 
            React.createElement(FramedExample, {height: 240, 
                           summary: "10x10", 
                           width: 800}, 
              React.createElement(SVGIcons, {size: "10x10"})
            ), 
            React.createElement(FramedExample, {height: 350, 
                            summary: "14x14", 
                            width: 800}, 
              React.createElement(SVGIcons, {size: "14x14"})
            ), 
            React.createElement(FramedExample, {height: 480, 
                            summary: "16x16", 
                            width: 800}, 
              React.createElement(SVGIcons, {size: "16x16"})
            )
          )

        )
      );
    }
  });

  window.addEventListener("DOMContentLoaded", function() {
    var uncaughtError;
    var consoleWarn = console.warn;
    var caughtWarnings = [];
    console.warn = function() {
      var args = Array.slice(arguments);
      caughtWarnings.push(args);
      consoleWarn.apply(console, args);
    };

    try {
      React.renderComponent(React.createElement(App, null), document.getElementById("main"));

      for (var listener of visibilityListeners) {
        listener({target: {hidden: false}});
      }
    } catch(err) {
      console.error(err);
      uncaughtError = err;
    }

    // Wait until all the FramedExamples have been fully loaded.
    setTimeout(function waitForQueuedFrames() {
      if (window.queuedFrames.length !== 0) {
        setTimeout(waitForQueuedFrames, 500);
        return;
      }
      // Put the title back, in case views changed it.
      document.title = "Loop UI Components Showcase";

      // This simulates the mocha layout for errors which means we can run
      // this alongside our other unit tests but use the same harness.
      var expectedWarningsCount = 10;
      var warningsMismatch = caughtWarnings.length !== expectedWarningsCount;
      if (uncaughtError || warningsMismatch) {
        $("#results").append("<div class='failures'><em>" +
          ((uncaughtError && warningsMismatch) ? 2 : 1) + "</em></div>");
        if (warningsMismatch) {
          $("#results").append("<li class='test fail'>" +
            "<h2>Unexpected number of warnings detected in UI-Showcase</h2>" +
            "<pre class='error'>Got: " + caughtWarnings.length + "\n" +
            "Expected: " + expectedWarningsCount + "</pre></li>");
        }
        if (uncaughtError) {
          $("#results").append("<li class='test fail'>" +
            "<h2>Errors rendering UI-Showcase</h2>" +
            "<pre class='error'>" + uncaughtError + "\n" + uncaughtError.stack + "</pre>" +
            "</li>");
        }
      } else {
        $("#results").append("<div class='failures'><em>0</em></div>");
      }
      $("#results").append("<p id='complete'>Complete.</p>");
    }, 1000);
  });

})();
