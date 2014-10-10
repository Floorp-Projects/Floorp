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
  var IncomingCallView = loop.conversation.IncomingCallView;
  var DesktopPendingConversationView = loop.conversationViews.PendingConversationView;
  var CallFailedView = loop.conversationViews.CallFailedView;

  // 2. Standalone webapp
  var HomeView = loop.webapp.HomeView;
  var UnsupportedBrowserView  = loop.webapp.UnsupportedBrowserView;
  var UnsupportedDeviceView   = loop.webapp.UnsupportedDeviceView;
  var CallUrlExpiredView      = loop.webapp.CallUrlExpiredView;
  var PendingConversationView = loop.webapp.PendingConversationView;
  var StartConversationView   = loop.webapp.StartConversationView;
  var FailedConversationView  = loop.webapp.FailedConversationView;
  var EndedConversationView   = loop.webapp.EndedConversationView;

  // 3. Shared components
  var ConversationToolbar = loop.shared.views.ConversationToolbar;
  var ConversationView = loop.shared.views.ConversationView;
  var FeedbackView = loop.shared.views.FeedbackView;

  // Local helpers
  function returnTrue() {
    return true;
  }

  function returnFalse() {
    return false;
  }

  function noop(){}

  // Feedback API client configured to send data to the stage input server,
  // which is available at https://input.allizom.org
  var stageFeedbackApiClient = new loop.FeedbackAPIClient(
    "https://input.allizom.org/api/v1/feedback", {
      product: "Loop"
    }
  );

  var dispatcher = new loop.Dispatcher();
  var roomListStore = new loop.store.RoomListStore({
    dispatcher: dispatcher,
    mozLoop: {}
  });

  // Local mocks

  var mockContact = {
    name: ["Mr Smith"],
    email: [{
      value: "smith@invalid.com"
    }]
  };

  var mockClient = {
    requestCallUrl: noop,
    requestCallUrlInfo: noop
  };

  var mockSDK = {};

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
               style={this.props.style || {}}>
            {this.props.children}
          </div>
        </div>
      );
    }
  });

  var Section = React.createClass({
    render: function() {
      return (
        <section id={this.props.name}>
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
            <Example summary="Call URL retrieved" dashed="true" style={{width: "332px"}}>
              <PanelView client={mockClient} notifications={notifications}
                         callUrl="http://invalid.example.url/"
                         dispatcher={dispatcher}
                         roomListStore={roomListStore} />
            </Example>
            <Example summary="Call URL retrieved - authenticated" dashed="true" style={{width: "332px"}}>
              <PanelView client={mockClient} notifications={notifications}
                         callUrl="http://invalid.example.url/"
                         userProfile={{email: "test@example.com"}}
                         dispatcher={dispatcher}
                         roomListStore={roomListStore} />
            </Example>
            <Example summary="Pending call url retrieval" dashed="true" style={{width: "332px"}}>
              <PanelView client={mockClient} notifications={notifications}
                         dispatcher={dispatcher}
                         roomListStore={roomListStore} />
            </Example>
            <Example summary="Pending call url retrieval - authenticated" dashed="true" style={{width: "332px"}}>
              <PanelView client={mockClient} notifications={notifications}
                         userProfile={{email: "test@example.com"}}
                         dispatcher={dispatcher}
                         roomListStore={roomListStore} />
            </Example>
            <Example summary="Error Notification" dashed="true" style={{width: "332px"}}>
              <PanelView client={mockClient} notifications={errNotifications}
                         dispatcher={dispatcher}
                         roomListStore={roomListStore} />
            </Example>
            <Example summary="Error Notification - authenticated" dashed="true" style={{width: "332px"}}>
              <PanelView client={mockClient} notifications={errNotifications}
                         userProfile={{email: "test@example.com"}}
                         dispatcher={dispatcher}
                         roomListStore={roomListStore} />
            </Example>
            <Example summary="Room list tab" dashed="true" style={{width: "332px"}}>
              <PanelView client={mockClient} notifications={notifications}
                         userProfile={{email: "test@example.com"}}
                         dispatcher={dispatcher}
                         roomListStore={roomListStore}
                         selectedTab="rooms" />
            </Example>
          </Section>

          <Section name="IncomingCallView">
            <Example summary="Default / incoming video call" dashed="true" style={{width: "260px", height: "254px"}}>
              <div className="fx-embedded">
                <IncomingCallView model={mockConversationModel}
                                  video={true} />
              </div>
            </Example>

            <Example summary="Default / incoming audio only call" dashed="true" style={{width: "260px", height: "254px"}}>
              <div className="fx-embedded">
                <IncomingCallView model={mockConversationModel}
                                  video={false} />
              </div>
            </Example>
          </Section>

          <Section name="IncomingCallView-ActiveState">
            <Example summary="Default" dashed="true" style={{width: "260px", height: "254px"}}>
              <div className="fx-embedded" >
                <IncomingCallView  model={mockConversationModel}
                                   showMenu={true} />
              </div>
            </Example>
          </Section>

          <Section name="ConversationToolbar">
            <h2>Desktop Conversation Window</h2>
            <div className="fx-embedded override-position">
              <Example summary="Default (260x265)" dashed="true">
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

          <Section name="PendingConversationView">
            <Example summary="Pending conversation view (connecting)" dashed="true">
              <div className="standalone">
                <PendingConversationView websocket={mockWebSocket}
                                         dispatcher={dispatcher} />
              </div>
            </Example>
            <Example summary="Pending conversation view (ringing)" dashed="true">
              <div className="standalone">
                <PendingConversationView websocket={mockWebSocket}
                                         dispatcher={dispatcher}
                                         callState="ringing"/>
              </div>
            </Example>
          </Section>

          <Section name="PendingConversationView (Desktop)">
            <Example summary="Connecting" dashed="true"
                     style={{width: "260px", height: "265px"}}>
              <div className="fx-embedded">
                <DesktopPendingConversationView callState={"gather"}
                                                contact={mockContact}
                                                dispatcher={dispatcher} />
              </div>
            </Example>
          </Section>

          <Section name="CallFailedView">
            <Example summary="Call Failed" dashed="true"
                     style={{width: "260px", height: "265px"}}>
              <div className="fx-embedded">
                <CallFailedView dispatcher={dispatcher} />
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
                     style={{width: "260px", height: "265px"}}>
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
                     dashed="true" style={{width: "260px", height: "265px"}}>
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
            <Example summary="Default (useable demo)" dashed="true" style={{width: "260px"}}>
              <FeedbackView feedbackApiClient={stageFeedbackApiClient} />
            </Example>
            <Example summary="Detailed form" dashed="true" style={{width: "260px"}}>
              <FeedbackView feedbackApiClient={stageFeedbackApiClient} step="form" />
            </Example>
            <Example summary="Thank you!" dashed="true" style={{width: "260px"}}>
              <FeedbackView feedbackApiClient={stageFeedbackApiClient} step="finished" />
            </Example>
          </Section>

          <Section name="CallUrlExpiredView">
            <Example summary="Firefox User">
              <CallUrlExpiredView helper={{isFirefox: returnTrue}} />
            </Example>
            <Example summary="Non-Firefox User">
              <CallUrlExpiredView helper={{isFirefox: returnFalse}} />
            </Example>
          </Section>

          <Section name="EndedConversationView">
            <Example summary="Displays the feedback form">
              <div className="standalone">
                <EndedConversationView sdk={mockSDK}
                                       video={{enabled: true}}
                                       audio={{enabled: true}}
                                       conversation={mockConversationModel}
                                       feedbackApiClient={stageFeedbackApiClient}
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
                <UnsupportedBrowserView />
              </div>
            </Example>
          </Section>

          <Section name="UnsupportedDeviceView">
            <Example summary="Standalone Unsupported Device">
              <div className="standalone">
                <UnsupportedDeviceView />
              </div>
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
    var body = document.body;
    body.className = loop.shared.utils.getTargetPlatform();

    React.renderComponent(<App />, body);

    _renderComponentsInIframes();

    // Put the title back, in case views changed it.
    document.title = "Loop UI Components Showcase";
  });

})();
