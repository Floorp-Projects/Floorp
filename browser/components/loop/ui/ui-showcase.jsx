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
    detailsButtonLabel: "Retry",
  });

  var SVGIcon = React.createClass({
    render: function() {
      var sizeUnit = this.props.size.split("x")[0] + "px";
      return (
        <span className="svg-icon" style={{
          "backgroundImage": "url(../content/shared/img/icons-" + this.props.size +
                              ".svg#" + this.props.shapeId + ")",
          "backgroundSize": sizeUnit + " " + sizeUnit
        }} />
      );
    }
  });

  var SVGIcons = React.createClass({
    shapes: {
      "10x10": ["close", "close-active", "close-disabled", "dropdown",
        "dropdown-white", "dropdown-active", "dropdown-disabled", "expand",
        "expand-active", "expand-disabled", "minimize", "minimize-active",
        "minimize-disabled"
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
    render: function() {
      return (
        <div className="showcase">
          <header>
            <h1>Loop UI Components Showcase</h1>
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
              <Example summary="Default" dashed="true" style={{width: "300px", height: "272px"}}>
                <ConversationToolbar video={{enabled: true}}
                                     audio={{enabled: true}}
                                     hangup={noop}
                                     publishStream={noop} />
              </Example>
              <Example summary="Video muted" style={{width: "300px", height: "272px"}}>
                <ConversationToolbar video={{enabled: false}}
                                     audio={{enabled: true}}
                                     hangup={noop}
                                     publishStream={noop} />
              </Example>
              <Example summary="Audio muted" style={{width: "300px", height: "272px"}}>
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

          <Section name="GumPromptConversationView">
            <Example summary="Gum Prompt conversation view" dashed="true">
              <div className="standalone">
                <GumPromptConversationView />
              </div>
            </Example>
          </Section>

          <Section name="WaitingConversationView">
            <Example summary="Waiting conversation view (connecting)" dashed="true">
              <div className="standalone">
                <WaitingConversationView websocket={mockWebSocket}
                                         dispatcher={dispatcher} />
              </div>
            </Example>
            <Example summary="Waiting conversation view (ringing)" dashed="true">
              <div className="standalone">
                <WaitingConversationView websocket={mockWebSocket}
                                         dispatcher={dispatcher}
                                         callState="ringing"/>
              </div>
            </Example>
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

          <Section name="StartConversationView">
            <Example summary="Start conversation view" dashed="true">
              <div className="standalone">
                <StartConversationView conversation={mockConversationModel}
                                       client={mockClient}
                                       notifications={notifications} />
              </div>
            </Example>
          </Section>

          <Section name="FailedConversationView">
            <Example summary="Failed conversation view" dashed="true">
              <div className="standalone">
                <FailedConversationView conversation={mockConversationModel}
                                        client={mockClient}
                                        notifications={notifications} />
              </div>
            </Example>
          </Section>

          <Section name="ConversationView">
            <Example summary="Desktop conversation window" dashed="true"
                     style={{width: "300px", height: "272px"}}>
              <div className="fx-embedded">
                <ConversationView sdk={mockSDK}
                                  model={mockConversationModel}
                                  video={{enabled: true}}
                                  audio={{enabled: true}} />
              </div>
            </Example>

            <Example summary="Desktop conversation window large" dashed="true">
              <div className="breakpoint" data-breakpoint-width="800px"
                data-breakpoint-height="600px">
                <div className="fx-embedded">
                  <ConversationView sdk={mockSDK}
                    video={{enabled: true}}
                    audio={{enabled: true}}
                    model={mockConversationModel} />
                </div>
              </div>
            </Example>

            <Example summary="Desktop conversation window local audio stream"
                     dashed="true" style={{width: "300px", height: "272px"}}>
              <div className="fx-embedded">
                <ConversationView sdk={mockSDK}
                                  video={{enabled: false}}
                                  audio={{enabled: true}}
                                  model={mockConversationModel} />
              </div>
            </Example>

            <Example summary="Standalone version">
              <div className="standalone">
                <ConversationView sdk={mockSDK}
                                  video={{enabled: true}}
                                  audio={{enabled: true}}
                                  model={mockConversationModel} />
              </div>
            </Example>
          </Section>

          <Section name="ConversationView-640">
            <Example summary="640px breakpoint for conversation view">
              <div className="breakpoint"
                   style={{"text-align":"center"}}
                   data-breakpoint-width="400px"
                   data-breakpoint-height="780px">
                <div className="standalone">
                  <ConversationView sdk={mockSDK}
                                    video={{enabled: true}}
                                    audio={{enabled: true}}
                                    model={mockConversationModel} />
                </div>
              </div>
            </Example>
          </Section>

          <Section name="ConversationView-LocalAudio">
            <Example summary="Local stream is audio only">
              <div className="standalone">
                <ConversationView sdk={mockSDK}
                                  video={{enabled: false}}
                                  audio={{enabled: true}}
                                  model={mockConversationModel} />
              </div>
            </Example>
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

          <Section name="CallUrlExpiredView">
            <Example summary="Firefox User">
              <CallUrlExpiredView isFirefox={true} />
            </Example>
            <Example summary="Non-Firefox User">
              <CallUrlExpiredView isFirefox={false} />
            </Example>
          </Section>

          <Section name="EndedConversationView">
            <Example summary="Displays the feedback form">
              <div className="standalone">
                <EndedConversationView sdk={mockSDK}
                                       video={{enabled: true}}
                                       audio={{enabled: true}}
                                       conversation={mockConversationModel}
                                       feedbackStore={feedbackStore}
                                       onAfterFeedbackReceived={noop} />
              </div>
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

          <Section name="HomeView">
            <Example summary="Standalone Home View">
              <div className="standalone">
                <HomeView />
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
            <Example summary="Desktop room conversation (invitation)" dashed="true"
                     style={{width: "260px", height: "265px"}}>
              <div className="fx-embedded">
                <DesktopRoomConversationView
                  roomStore={roomStore}
                  dispatcher={dispatcher}
                  mozLoop={navigator.mozLoop}
                  roomState={ROOM_STATES.INIT} />
              </div>
            </Example>

            <Example summary="Desktop room conversation" dashed="true"
                     style={{width: "260px", height: "265px"}}>
              <div className="fx-embedded">
                <DesktopRoomConversationView
                  roomStore={roomStore}
                  dispatcher={dispatcher}
                  mozLoop={navigator.mozLoop}
                  roomState={ROOM_STATES.HAS_PARTICIPANTS} />
              </div>
            </Example>
          </Section>

          <Section name="StandaloneRoomView">
            <Example summary="Standalone room conversation (ready)">
              <div className="standalone">
                <StandaloneRoomView
                  dispatcher={dispatcher}
                  activeRoomStore={activeRoomStore}
                  roomState={ROOM_STATES.READY}
                  isFirefox={true} />
              </div>
            </Example>

            <Example summary="Standalone room conversation (joined)">
              <div className="standalone">
                <StandaloneRoomView
                  dispatcher={dispatcher}
                  activeRoomStore={activeRoomStore}
                  roomState={ROOM_STATES.JOINED}
                  isFirefox={true} />
              </div>
            </Example>

            <Example summary="Standalone room conversation (has-participants)">
              <div className="standalone">
                <StandaloneRoomView
                  dispatcher={dispatcher}
                  activeRoomStore={activeRoomStore}
                  roomState={ROOM_STATES.HAS_PARTICIPANTS}
                  isFirefox={true} />
              </div>
            </Example>

            <Example summary="Standalone room conversation (full - FFx user)">
              <div className="standalone">
                <StandaloneRoomView
                  dispatcher={dispatcher}
                  activeRoomStore={activeRoomStore}
                  roomState={ROOM_STATES.FULL}
                  isFirefox={true} />
              </div>
            </Example>

            <Example summary="Standalone room conversation (full - non FFx user)">
              <div className="standalone">
                <StandaloneRoomView
                  dispatcher={dispatcher}
                  activeRoomStore={activeRoomStore}
                  roomState={ROOM_STATES.FULL}
                  isFirefox={false} />
              </div>
            </Example>

            <Example summary="Standalone room conversation (feedback)">
              <div className="standalone">
                <StandaloneRoomView
                  dispatcher={dispatcher}
                  activeRoomStore={activeRoomStore}
                  feedbackStore={feedbackStore}
                  roomState={ROOM_STATES.ENDED}
                  isFirefox={false} />
              </div>
            </Example>

            <Example summary="Standalone room conversation (failed)">
              <div className="standalone">
                <StandaloneRoomView
                  dispatcher={dispatcher}
                  activeRoomStore={activeRoomStore}
                  roomState={ROOM_STATES.FAILED}
                  isFirefox={false} />
              </div>
            </Example>
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
      React.renderComponent(<App />, document.getElementById("main"));

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
