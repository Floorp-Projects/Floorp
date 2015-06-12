/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global Frame:false uncaughtError:true */

(function() {
  "use strict";

  // Stop the default init functions running to avoid conflicts.
  document.removeEventListener("DOMContentLoaded", loop.panel.init);
  document.removeEventListener("DOMContentLoaded", loop.conversation.init);

  // 1. Desktop components
  // 1.1 Panel
  var PanelView = loop.panel.PanelView;
  var SignInRequestView = loop.panel.SignInRequestView;
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
  var FeedbackView = loop.shared.views.FeedbackView;
  var Checkbox = loop.shared.views.Checkbox;

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

  var dispatcher = new loop.Dispatcher();

  // Feedback API client configured to send data to the stage input server,
  // which is available at https://input.allizom.org
  var stageFeedbackApiClient = new loop.FeedbackAPIClient(
    "https://input.allizom.org/api/v1/feedback", {
      product: "Loop"
    }
  );

  var mockSDK = _.extend({
    sendTextChatMessage: function(message) {
      dispatcher.dispatch(new loop.shared.actions.ReceivedTextChatMessage({
        message: message
      }));
    }
  }, Backbone.Events);

  /**
   * Every view that uses an activeRoomStore needs its own; if they shared
   * an active store, they'd interfere with each other.
   *
   * @param options
   * @returns {loop.store.ActiveRoomStore}
   */
  function makeActiveRoomStore(options) {
    var dispatcher = new loop.Dispatcher();

    var store = new loop.store.ActiveRoomStore(dispatcher, {
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

  var readyRoomStore = makeActiveRoomStore({
    mediaConnected: false,
    roomState: ROOM_STATES.READY
  });

  var updatingActiveRoomStore = makeActiveRoomStore({
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

  var roomStore = new loop.store.RoomStore(dispatcher, {
    mozLoop: navigator.mozLoop
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

  var feedbackStore = new loop.store.FeedbackStore(dispatcher, {
    feedbackClient: stageFeedbackApiClient
  });
  var conversationStore = new loop.store.ConversationStore(dispatcher, {
    client: {},
    mozLoop: navigator.mozLoop,
    sdkDriver: mockSDK
  });
  var textChatStore = new loop.store.TextChatStore(dispatcher, {
    sdkDriver: mockSDK
  });

  textChatStore.setStoreState({
    // XXX Disabled until we start sorting out some of the layouts.
    textChatEnabled: false
  });

  loop.store.StoreMixin.register({
    conversationStore: conversationStore,
    feedbackStore: feedbackStore,
    textChatStore: textChatStore
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

  var SVGIcon = React.createClass({
    render: function() {
      var sizeUnit = this.props.size.split("x");
      return (
        <img className="svg-icon"
             src={"../content/shared/img/icons-" + this.props.size + ".svg#" + this.props.shapeId}
             width={sizeUnit[0]}
             height={sizeUnit[1]} />
      );
    }
  });

  var SVGIcons = React.createClass({
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
        "contacts-active", "copy", "checkmark", "delete", "globe", "google", "google-hover",
        "google-active", "history", "history-hover", "history-active", "leave",
        "precall", "precall-hover", "precall-active", "screen-white", "screenmute-white",
        "settings", "settings-hover", "settings-active", "share-darkgrey", "tag",
        "tag-hover", "tag-active", "trash", "unblock", "unblock-hover", "unblock-active",
        "video", "video-hover", "video-active", "tour"
      ]
    },

    render: function() {
      var icons = this.shapes[this.props.size].map(function(shapeId, i) {
        return (
          <li key={this.props.size + "-" + i} className="svg-icon-entry">
            <p><SVGIcon shapeId={shapeId} size={this.props.size} /></p>
            <p>{shapeId}</p>
          </li>
        );
      }, this);
      return (
        <ul className="svg-icon-list">{icons}</ul>
      );
    }
  });

  var FramedExample = React.createClass({
    propTypes: {
      width: React.PropTypes.number,
      height: React.PropTypes.number,
      onContentsRendered: React.PropTypes.func
    },

    makeId: function(prefix) {
      return (prefix || "") + this.props.summary.toLowerCase().replace(/\s/g, "-");
    },

    render: function() {
      var cx = React.addons.classSet;
      return (
        <div className="example">
          <h3 id={this.makeId()}>
            {this.props.summary}
            <a href={this.makeId("#")}>&nbsp;¶</a>
          </h3>
          <div className={cx({comp: true, dashed: this.props.dashed})}
               style={this.props.style}>
            <Frame width={this.props.width} height={this.props.height}
                   onContentsRendered={this.props.onContentsRendered}>
              {this.props.children}
            </Frame>
          </div>
        </div>
      );
    }
  });

  var Example = React.createClass({
    makeId: function(prefix) {
      return (prefix || "") + this.props.summary.toLowerCase().replace(/\s/g, "-");
    },

    render: function() {
      var cx = React.addons.classSet;
      return (
        <div className="example">
          <h3 id={this.makeId()}>
            {this.props.summary}
            <a href={this.makeId("#")}>&nbsp;¶</a>
          </h3>
          <div className={cx({comp: true, dashed: this.props.dashed})}
               style={this.props.style}>
            {this.props.children}
          </div>
        </div>
      );
    }
  });

  var Section = React.createClass({
    render: function() {
      return (
        <section id={this.props.name} className={this.props.className}>
          <h1>{this.props.name}</h1>
          {this.props.children}
        </section>
      );
    }
  });

  var ShowCase = React.createClass({
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
        <div className="showcase">
          <header>
            <h1>Loop UI Components Showcase</h1>
            <Checkbox label="RTL mode?" checked={this.state.rtlMode}
              onChange={this._handleCheckboxChange} />
            <nav className="showcase-menu">{
              React.Children.map(this.props.children, function(section) {
                return (
                  <a className="btn btn-info" href={"#" + section.props.name}>
                    {section.props.name}
                  </a>
                );
              })
            }</nav>
          </header>
          {this.props.children}
        </div>
      );
    }
  });

  var App = React.createClass({

    render: function() {
      return (
        <ShowCase>
          <Section name="PanelView">
            <p className="note">
              <strong>Note:</strong> 332px wide.
            </p>
            <Example summary="Re-sign-in view" dashed="true" style={{width: "332px"}}>
              <SignInRequestView mozLoop={mockMozLoopRooms} />
            </Example>
            <Example summary="Room list tab" dashed="true" style={{width: "332px"}}>
              <PanelView client={mockClient} notifications={notifications}
                         userProfile={{email: "test@example.com"}}
                         mozLoop={mockMozLoopRooms}
                         dispatcher={dispatcher}
                         roomStore={roomStore}
                         selectedTab="rooms" />
            </Example>
            <Example summary="Contact list tab" dashed="true" style={{width: "332px"}}>
              <PanelView client={mockClient} notifications={notifications}
                         userProfile={{email: "test@example.com"}}
                         mozLoop={mockMozLoopRooms}
                         dispatcher={dispatcher}
                         roomStore={roomStore}
                         selectedTab="contacts" />
            </Example>
            <Example summary="Error Notification" dashed="true" style={{width: "332px"}}>
              <PanelView client={mockClient} notifications={errNotifications}
                         mozLoop={navigator.mozLoop}
                         dispatcher={dispatcher}
                         roomStore={roomStore} />
            </Example>
            <Example summary="Error Notification - authenticated" dashed="true" style={{width: "332px"}}>
              <PanelView client={mockClient} notifications={errNotifications}
                         userProfile={{email: "test@example.com"}}
                         mozLoop={navigator.mozLoop}
                         dispatcher={dispatcher}
                         roomStore={roomStore} />
            </Example>
            <Example summary="Contact import success" dashed="true" style={{width: "332px"}}>
              <PanelView notifications={new loop.shared.models.NotificationCollection([{level: "success", message: "Import success"}])}
                         userProfile={{email: "test@example.com"}}
                         mozLoop={mockMozLoopRooms}
                         dispatcher={dispatcher}
                         roomStore={roomStore}
                         selectedTab="contacts" />
            </Example>
            <Example summary="Contact import error" dashed="true" style={{width: "332px"}}>
              <PanelView notifications={new loop.shared.models.NotificationCollection([{level: "error", message: "Import error"}])}
                         userProfile={{email: "test@example.com"}}
                         mozLoop={mockMozLoopRooms}
                         dispatcher={dispatcher}
                         roomStore={roomStore}
                         selectedTab="contacts" />
            </Example>
          </Section>

          <Section name="AcceptCallView">
            <Example summary="Default / incoming video call" dashed="true" style={{width: "300px", height: "272px"}}>
              <div className="fx-embedded">
                <AcceptCallView callType={CALL_TYPES.AUDIO_VIDEO}
                                callerId="Mr Smith"
                                dispatcher={dispatcher}
                                mozLoop={mockMozLoopRooms} />
              </div>
            </Example>

            <Example summary="Default / incoming audio only call" dashed="true" style={{width: "300px", height: "272px"}}>
              <div className="fx-embedded">
                <AcceptCallView callType={CALL_TYPES.AUDIO_ONLY}
                                callerId="Mr Smith"
                                dispatcher={dispatcher}
                                mozLoop={mockMozLoopRooms} />
              </div>
            </Example>
          </Section>

          <Section name="AcceptCallView-ActiveState">
            <Example summary="Default" dashed="true" style={{width: "300px", height: "272px"}}>
              <div className="fx-embedded" >
                <AcceptCallView callType={CALL_TYPES.AUDIO_VIDEO}
                                callerId="Mr Smith"
                                dispatcher={dispatcher}
                                mozLoop={mockMozLoopRooms}
                                showMenu={true} />
              </div>
            </Example>
          </Section>

          <Section name="ConversationToolbar">
            <h2>Desktop Conversation Window</h2>
            <div className="fx-embedded override-position">
              <Example summary="Default" style={{width: "300px", height: "26px"}}>
                <ConversationToolbar video={{enabled: true}}
                                     audio={{enabled: true}}
                                     hangup={noop}
                                     publishStream={noop} />
              </Example>
              <Example summary="Video muted" style={{width: "300px", height: "26px"}}>
                <ConversationToolbar video={{enabled: false}}
                                     audio={{enabled: true}}
                                     hangup={noop}
                                     publishStream={noop} />
              </Example>
              <Example summary="Audio muted" style={{width: "300px", height: "26px"}}>
                <ConversationToolbar video={{enabled: true}}
                                     audio={{enabled: false}}
                                     hangup={noop}
                                     publishStream={noop} />
              </Example>
            </div>

            <h2>Standalone</h2>
            <div className="standalone override-position">
              <Example summary="Default">
                <ConversationToolbar video={{enabled: true}}
                                     audio={{enabled: true}}
                                     hangup={noop}
                                     publishStream={noop} />
              </Example>
              <Example summary="Video muted">
                <ConversationToolbar video={{enabled: false}}
                                     audio={{enabled: true}}
                                     hangup={noop}
                                     publishStream={noop} />
              </Example>
              <Example summary="Audio muted">
                <ConversationToolbar video={{enabled: true}}
                                     audio={{enabled: false}}
                                     hangup={noop}
                                     publishStream={noop} />
              </Example>
            </div>
          </Section>

          <Section name="PendingConversationView (Desktop)">
            <Example summary="Connecting" dashed="true"
                     style={{width: "300px", height: "272px"}}>
              <div className="fx-embedded">
                <DesktopPendingConversationView callState={"gather"}
                                                contact={mockContact}
                                                dispatcher={dispatcher} />
              </div>
            </Example>
          </Section>

          <Section name="CallFailedView">
            <Example summary="Call Failed - Incoming" dashed="true"
                     style={{width: "300px", height: "272px"}}>
              <div className="fx-embedded">
                <CallFailedView dispatcher={dispatcher}
                                outgoing={false}
                                store={conversationStore} />
              </div>
            </Example>
            <Example summary="Call Failed - Outgoing" dashed="true"
                     style={{width: "300px", height: "272px"}}>
              <div className="fx-embedded">
                <CallFailedView dispatcher={dispatcher}
                                outgoing={true}
                                store={conversationStore} />
              </div>
            </Example>
            <Example summary="Call Failed — with call URL error" dashed="true"
                     style={{width: "300px", height: "272px"}}>
              <div className="fx-embedded">
                <CallFailedView dispatcher={dispatcher} emailLinkError={true}
                                outgoing={true}
                                store={conversationStore} />
              </div>
            </Example>
          </Section>

          <Section name="OngoingConversationView">
            <FramedExample width={298} height={254}
                           summary="Desktop ongoing conversation window">
              <div className="fx-embedded">
                <OngoingConversationView
                  dispatcher={dispatcher}
                  video={{enabled: true}}
                  audio={{enabled: true}}
                  localPosterUrl="sample-img/video-screen-local.png"
                  remotePosterUrl="sample-img/video-screen-remote.png"
                  remoteVideoEnabled={true}
                  mediaConnected={true} />
              </div>
            </FramedExample>

            <FramedExample width={800} height={600}
                           summary="Desktop ongoing conversation window large">
                <div className="fx-embedded">
                  <OngoingConversationView
                    dispatcher={dispatcher}
                    video={{enabled: true}}
                    audio={{enabled: true}}
                    localPosterUrl="sample-img/video-screen-local.png"
                    remotePosterUrl="sample-img/video-screen-remote.png"
                    remoteVideoEnabled={true}
                    mediaConnected={true} />
                </div>
            </FramedExample>

            <FramedExample width={298} height={254}
              summary="Desktop ongoing conversation window - local face mute">
              <div className="fx-embedded">
                <OngoingConversationView
                  dispatcher={dispatcher}
                  video={{enabled: false}}
                  audio={{enabled: true}}
                  remoteVideoEnabled={true}
                  remotePosterUrl="sample-img/video-screen-remote.png"
                  mediaConnected={true} />
              </div>
            </FramedExample>

            <FramedExample width={298} height={254}
              summary="Desktop ongoing conversation window - remote face mute">
              <div className="fx-embedded">
                <OngoingConversationView
                  dispatcher={dispatcher}
                  video={{enabled: true}}
                  audio={{enabled: true}}
                  remoteVideoEnabled={false}
                  localPosterUrl="sample-img/video-screen-local.png"
                  mediaConnected={true} />
              </div>
            </FramedExample>

          </Section>

          <Section name="FeedbackView">
            <p className="note">
              <strong>Note:</strong> For the useable demo, you can access submitted data at&nbsp;
              <a href="https://input.allizom.org/">input.allizom.org</a>.
            </p>
            <Example summary="Default (useable demo)" dashed="true" style={{width: "300px", height: "272px"}}>
              <FeedbackView feedbackStore={feedbackStore} />
            </Example>
            <Example summary="Detailed form" dashed="true" style={{width: "300px", height: "272px"}}>
              <FeedbackView feedbackStore={feedbackStore} feedbackState={FEEDBACK_STATES.DETAILS} />
            </Example>
            <Example summary="Thank you!" dashed="true" style={{width: "300px", height: "272px"}}>
              <FeedbackView feedbackStore={feedbackStore} feedbackState={FEEDBACK_STATES.SENT} />
            </Example>
          </Section>

          <Section name="AlertMessages">
            <Example summary="Various alerts">
              <div className="alert alert-warning">
                <button className="close"></button>
                <p className="message">
                  The person you were calling has ended the conversation.
                </p>
              </div>
              <br />
              <div className="alert alert-error">
                <button className="close"></button>
                <p className="message">
                  The person you were calling has ended the conversation.
                </p>
              </div>
            </Example>
          </Section>

          <Section name="UnsupportedBrowserView">
            <Example summary="Standalone Unsupported Browser">
              <div className="standalone">
                <UnsupportedBrowserView isFirefox={false}/>
              </div>
            </Example>
          </Section>

          <Section name="UnsupportedDeviceView">
            <Example summary="Standalone Unsupported Device">
              <div className="standalone">
                <UnsupportedDeviceView platform="ios"/>
              </div>
            </Example>
          </Section>

          <Section name="DesktopRoomConversationView">
            <FramedExample width={298} height={254}
              summary="Desktop room conversation (invitation)">
              <div className="fx-embedded">
                <DesktopRoomConversationView
                  roomStore={roomStore}
                  dispatcher={dispatcher}
                  mozLoop={navigator.mozLoop}
                  localPosterUrl="sample-img/video-screen-local.png"
                  roomState={ROOM_STATES.INIT} />
              </div>
            </FramedExample>

            <FramedExample width={298} height={254}
              summary="Desktop room conversation">
              <div className="fx-embedded">
                <DesktopRoomConversationView
                  roomStore={roomStore}
                  dispatcher={dispatcher}
                  mozLoop={navigator.mozLoop}
                  localPosterUrl="sample-img/video-screen-local.png"
                  remotePosterUrl="sample-img/video-screen-remote.png"
                  roomState={ROOM_STATES.HAS_PARTICIPANTS} />
              </div>
            </FramedExample>

            <FramedExample width={298} height={254}
                           summary="Desktop room conversation local face-mute">
              <div className="fx-embedded">
                <DesktopRoomConversationView
                  roomStore={desktopLocalFaceMuteRoomStore}
                  dispatcher={dispatcher}
                  mozLoop={navigator.mozLoop}
                  remotePosterUrl="sample-img/video-screen-remote.png" />
              </div>
            </FramedExample>

            <FramedExample width={298} height={254}
                           summary="Desktop room conversation remote face-mute">
              <div className="fx-embedded">
                <DesktopRoomConversationView
                  roomStore={desktopRemoteFaceMuteRoomStore}
                  dispatcher={dispatcher}
                  mozLoop={navigator.mozLoop}
                  localPosterUrl="sample-img/video-screen-local.png" />
              </div>
            </FramedExample>
          </Section>

          <Section name="StandaloneRoomView">
            <FramedExample width={644} height={483}
              summary="Standalone room conversation (ready)">
              <div className="standalone">
                <StandaloneRoomView
                  dispatcher={dispatcher}
                  activeRoomStore={readyRoomStore}
                  roomState={ROOM_STATES.READY}
                  isFirefox={true} />
              </div>
            </FramedExample>

            <FramedExample width={644} height={483}
              summary="Standalone room conversation (joined)"
              onContentsRendered={joinedRoomStore.forcedUpdate}>
              <div className="standalone">
                <StandaloneRoomView
                  dispatcher={dispatcher}
                  activeRoomStore={joinedRoomStore}
                  localPosterUrl="sample-img/video-screen-local.png"
                  isFirefox={true} />
              </div>
            </FramedExample>

            <FramedExample width={644} height={483}
                           onContentsRendered={updatingActiveRoomStore.forcedUpdate}
                           summary="Standalone room conversation (has-participants, 644x483)">
                <div className="standalone">
                  <StandaloneRoomView
                    dispatcher={dispatcher}
                    activeRoomStore={updatingActiveRoomStore}
                    roomState={ROOM_STATES.HAS_PARTICIPANTS}
                    isFirefox={true}
                    localPosterUrl="sample-img/video-screen-local.png"
                    remotePosterUrl="sample-img/video-screen-remote.png" />
                </div>
            </FramedExample>

            <FramedExample width={644} height={483}
                           onContentsRendered={localFaceMuteRoomStore.forcedUpdate}
                           summary="Standalone room conversation (local face mute, has-participants, 644x483)">
              <div className="standalone">
                <StandaloneRoomView
                  dispatcher={dispatcher}
                  activeRoomStore={localFaceMuteRoomStore}
                  isFirefox={true}
                  localPosterUrl="sample-img/video-screen-local.png"
                  remotePosterUrl="sample-img/video-screen-remote.png" />
              </div>
            </FramedExample>

            <FramedExample width={644} height={483}
                           onContentsRendered={remoteFaceMuteRoomStore.forcedUpdate}
                           summary="Standalone room conversation (remote face mute, has-participants, 644x483)">
              <div className="standalone">
                <StandaloneRoomView
                  dispatcher={dispatcher}
                  activeRoomStore={remoteFaceMuteRoomStore}
                  isFirefox={true}
                  localPosterUrl="sample-img/video-screen-local.png"
                  remotePosterUrl="sample-img/video-screen-remote.png" />
              </div>
            </FramedExample>

            <FramedExample width={800} height={660}
                           onContentsRendered={updatingSharingRoomStore.forcedUpdate}
              summary="Standalone room convo (has-participants, receivingScreenShare, 800x660)">
                <div className="standalone">
                  <StandaloneRoomView
                    dispatcher={dispatcher}
                    activeRoomStore={updatingSharingRoomStore}
                    roomState={ROOM_STATES.HAS_PARTICIPANTS}
                    isFirefox={true}
                    localPosterUrl="sample-img/video-screen-local.png"
                    remotePosterUrl="sample-img/video-screen-remote.png"
                    screenSharePosterUrl="sample-img/video-screen-terminal.png"
                  />
                </div>
            </FramedExample>

            <FramedExample width={644} height={483}
                           summary="Standalone room conversation (full - FFx user)">
              <div className="standalone">
                <StandaloneRoomView
                  dispatcher={dispatcher}
                  activeRoomStore={fullActiveRoomStore}
                  isFirefox={true} />
              </div>
            </FramedExample>

            <FramedExample width={644} height={483}
              summary="Standalone room conversation (full - non FFx user)">
              <div className="standalone">
                <StandaloneRoomView
                  dispatcher={dispatcher}
                  activeRoomStore={fullActiveRoomStore}
                  isFirefox={false} />
              </div>
            </FramedExample>

            <FramedExample width={644} height={483}
              summary="Standalone room conversation (feedback)">
              <div className="standalone">
                <StandaloneRoomView
                  dispatcher={dispatcher}
                  activeRoomStore={endedRoomStore}
                  feedbackStore={feedbackStore}
                  isFirefox={false} />
              </div>
            </FramedExample>

            <FramedExample width={644} height={483}
                           summary="Standalone room conversation (failed)">
              <div className="standalone">
                <StandaloneRoomView
                  dispatcher={dispatcher}
                  activeRoomStore={failedRoomStore}
                  isFirefox={false} />
              </div>
            </FramedExample>
          </Section>

          <Section name="StandaloneRoomView (Mobile)">
            <FramedExample width={600} height={480}
                           onContentsRendered={updatingActiveRoomStore.forcedUpdate}
                           summary="Standalone room conversation (has-participants, 600x480)">
                <div className="standalone">
                  <StandaloneRoomView
                    dispatcher={dispatcher}
                    activeRoomStore={updatingActiveRoomStore}
                    roomState={ROOM_STATES.HAS_PARTICIPANTS}
                    isFirefox={true}
                    localPosterUrl="sample-img/video-screen-local.png"
                    remotePosterUrl="sample-img/video-screen-remote.png" />
                </div>
            </FramedExample>
          </Section>

          <Section name="SVG icons preview" className="svg-icons">
            <Example summary="10x10">
              <SVGIcons size="10x10"/>
            </Example>
            <Example summary="14x14">
              <SVGIcons size="14x14" />
            </Example>
            <Example summary="16x16">
              <SVGIcons size="16x16"/>
            </Example>
          </Section>

        </ShowCase>
      );
    }
  });

  window.addEventListener("DOMContentLoaded", function() {
    try {
      React.renderComponent(<App />, document.getElementById("main"));

      for (var listener of visibilityListeners) {
        listener({target: {hidden: false}});
      }
    } catch(err) {
      console.error(err);
      uncaughtError = err;
    }

    // Wait until all the FramedExamples have been fully loaded.
    setTimeout(function waitForQueuedFrames() {
      if (window.queuedFrames.length != 0) {
        setTimeout(waitForQueuedFrames, 500);
        return;
      }
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
    }, 1000);
  });

})();
