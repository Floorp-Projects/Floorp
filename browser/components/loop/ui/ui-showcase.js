/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* jshint newcap:false */
/* global loop:true, React */

(function() {
  "use strict";

  // 1. Desktop components
  // 1.1 Panel
  var PanelView = loop.panel.PanelView;
  // 1.2. Conversation Window
  var IncomingCallView = loop.conversation.IncomingCallView;

  // 2. Standalone webapp
  var HomeView = loop.webapp.HomeView;
  var UnsupportedBrowserView = loop.webapp.UnsupportedBrowserView;
  var UnsupportedDeviceView = loop.webapp.UnsupportedDeviceView;
  var CallUrlExpiredView    = loop.webapp.CallUrlExpiredView;
  var PendingConversationView = loop.webapp.PendingConversationView;
  var StartConversationView = loop.webapp.StartConversationView;

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

  // Local mocks

  var mockClient = {
    requestCallUrl: noop,
    requestCallUrlInfo: noop
  };

  var mockSDK = {};

  var mockConversationModel = new loop.shared.models.ConversationModel({}, {
    sdk: mockSDK
  });
  mockConversationModel.startSession = noop;

  var notifications = new loop.shared.models.NotificationCollection();
  var errNotifications = new loop.shared.models.NotificationCollection();
  errNotifications.error("Error!");

  var Example = React.createClass({displayName: 'Example',
    render: function() {
      var cx = React.addons.classSet;
      return (
        React.DOM.div({className: "example"}, 
          React.DOM.h3(null, this.props.summary), 
          React.DOM.div({className: cx({comp: true, dashed: this.props.dashed}), 
               style: this.props.style || {}}, 
            this.props.children
          )
        )
      );
    }
  });

  var Section = React.createClass({displayName: 'Section',
    render: function() {
      return (
        React.DOM.section({id: this.props.name}, 
          React.DOM.h1(null, this.props.name), 
          this.props.children
        )
      );
    }
  });

  var ShowCase = React.createClass({displayName: 'ShowCase',
    render: function() {
      return (
        React.DOM.div({className: "showcase"}, 
          React.DOM.header(null, 
            React.DOM.h1(null, "Loop UI Components Showcase"), 
            React.DOM.nav({className: "showcase-menu"}, 
              React.Children.map(this.props.children, function(section) {
                return (
                  React.DOM.a({className: "btn btn-info", href: "#" + section.props.name}, 
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

  var App = React.createClass({displayName: 'App',
    render: function() {
      return (
        ShowCase(null, 
          Section({name: "PanelView"}, 
            React.DOM.p({className: "note"}, 
              React.DOM.strong(null, "Note:"), " 332px wide."
            ), 
            Example({summary: "Call URL retrieved", dashed: "true", style: {width: "332px"}}, 
              PanelView({client: mockClient, notifications: notifications, 
                         callUrl: "http://invalid.example.url/"})
            ), 
            Example({summary: "Pending call url retrieval", dashed: "true", style: {width: "332px"}}, 
              PanelView({client: mockClient, notifications: notifications})
            ), 
            Example({summary: "Error Notification", dashed: "true", style: {width: "332px"}}, 
              PanelView({client: mockClient, notifications: errNotifications})
            )
          ), 

          Section({name: "IncomingCallView"}, 
            Example({summary: "Default / incoming video call", dashed: "true", style: {width: "280px"}}, 
              React.DOM.div({className: "fx-embedded"}, 
                IncomingCallView({model: mockConversationModel, 
                                  video: {enabled: true}})
              )
            ), 

            Example({summary: "Default / incoming audio only call", dashed: "true", style: {width: "280px"}}, 
              React.DOM.div({className: "fx-embedded"}, 
                IncomingCallView({model: mockConversationModel, 
                                  video: {enabled: false}})
              )
            )
          ), 

          Section({name: "IncomingCallView-ActiveState"}, 
            Example({summary: "Default", dashed: "true", style: {width: "280px"}}, 
              React.DOM.div({className: "fx-embedded"}, 
                IncomingCallView({model: mockConversationModel, 
                                   showDeclineMenu: true, 
                                   video: {enabled: true}})
              )
            )
          ), 

          Section({name: "ConversationToolbar"}, 
            React.DOM.h2(null, "Desktop Conversation Window"), 
            React.DOM.div({className: "fx-embedded override-position"}, 
              Example({summary: "Default (260x265)", dashed: "true"}, 
                ConversationToolbar({video: {enabled: true}, 
                                     audio: {enabled: true}, 
                                     hangup: noop, 
                                     publishStream: noop})
              ), 
              Example({summary: "Video muted"}, 
                ConversationToolbar({video: {enabled: false}, 
                                     audio: {enabled: true}, 
                                     hangup: noop, 
                                     publishStream: noop})
              ), 
              Example({summary: "Audio muted"}, 
                ConversationToolbar({video: {enabled: true}, 
                                     audio: {enabled: false}, 
                                     hangup: noop, 
                                     publishStream: noop})
              )
            ), 

            React.DOM.h2(null, "Standalone"), 
            React.DOM.div({className: "standalone override-position"}, 
              Example({summary: "Default"}, 
                ConversationToolbar({video: {enabled: true}, 
                                     audio: {enabled: true}, 
                                     hangup: noop, 
                                     publishStream: noop})
              ), 
              Example({summary: "Video muted"}, 
                ConversationToolbar({video: {enabled: false}, 
                                     audio: {enabled: true}, 
                                     hangup: noop, 
                                     publishStream: noop})
              ), 
              Example({summary: "Audio muted"}, 
                ConversationToolbar({video: {enabled: true}, 
                                     audio: {enabled: false}, 
                                     hangup: noop, 
                                     publishStream: noop})
              )
            )
          ), 

          Section({name: "PendingConversationView"}, 
            Example({summary: "Pending conversation view (connecting)", dashed: "true"}, 
              React.DOM.div({className: "standalone"}, 
                PendingConversationView(null)
              )
            ), 
            Example({summary: "Pending conversation view (ringing)", dashed: "true"}, 
              React.DOM.div({className: "standalone"}, 
                PendingConversationView({callState: "ringing"})
              )
            )
          ), 

          Section({name: "StartConversationView"}, 
            Example({summary: "Start conversation view", dashed: "true"}, 
              React.DOM.div({className: "standalone"}, 
                StartConversationView({model: mockConversationModel, 
                                       client: mockClient, 
                                       notifications: notifications, 
                                       showCallOptionsMenu: true})
              )
            )
          ), 

          Section({name: "ConversationView"}, 
            Example({summary: "Desktop conversation window", dashed: "true", 
                     style: {width: "260px", height: "265px"}}, 
              React.DOM.div({className: "fx-embedded"}, 
                ConversationView({sdk: mockSDK, 
                                  model: mockConversationModel, 
                                  video: {enabled: true}, 
                                  audio: {enabled: true}})
              )
            ), 

            Example({summary: "Desktop conversation window large", dashed: "true"}, 
              React.DOM.div({className: "breakpoint", 'data-breakpoint-width': "800px", 
                'data-breakpoint-height': "600px"}, 
                React.DOM.div({className: "fx-embedded"}, 
                  ConversationView({sdk: mockSDK, 
                    video: {enabled: true}, 
                    audio: {enabled: true}, 
                    model: mockConversationModel})
                )
              )
            ), 

            Example({summary: "Desktop conversation window local audio stream", 
                     dashed: "true", style: {width: "260px", height: "265px"}}, 
              React.DOM.div({className: "fx-embedded"}, 
                ConversationView({sdk: mockSDK, 
                                  video: {enabled: false}, 
                                  audio: {enabled: true}, 
                                  model: mockConversationModel})
              )
            ), 

            Example({summary: "Standalone version"}, 
              React.DOM.div({className: "standalone"}, 
                ConversationView({sdk: mockSDK, 
                                  video: {enabled: true}, 
                                  audio: {enabled: true}, 
                                  model: mockConversationModel})
              )
            )
          ), 

          Section({name: "ConversationView-640"}, 
            Example({summary: "640px breakpoint for conversation view"}, 
              React.DOM.div({className: "breakpoint", 
                   style: {"text-align":"center"}, 
                   'data-breakpoint-width': "400px", 
                   'data-breakpoint-height': "780px"}, 
                React.DOM.div({className: "standalone"}, 
                  ConversationView({sdk: mockSDK, 
                                    video: {enabled: true}, 
                                    audio: {enabled: true}, 
                                    model: mockConversationModel})
                )
              )
            )
          ), 

          Section({name: "ConversationView-LocalAudio"}, 
            Example({summary: "Local stream is audio only"}, 
              React.DOM.div({className: "standalone"}, 
                ConversationView({sdk: mockSDK, 
                                  video: {enabled: false}, 
                                  audio: {enabled: true}, 
                                  model: mockConversationModel})
              )
            )
          ), 

          Section({name: "FeedbackView"}, 
            React.DOM.p({className: "note"}, 
              React.DOM.strong(null, "Note:"), " For the useable demo, you can access submitted data at ", 
              React.DOM.a({href: "https://input.allizom.org/"}, "input.allizom.org"), "."
            ), 
            Example({summary: "Default (useable demo)", dashed: "true", style: {width: "280px"}}, 
              FeedbackView({feedbackApiClient: stageFeedbackApiClient})
            ), 
            Example({summary: "Detailed form", dashed: "true", style: {width: "280px"}}, 
              FeedbackView({feedbackApiClient: stageFeedbackApiClient, step: "form"})
            ), 
            Example({summary: "Thank you!", dashed: "true", style: {width: "280px"}}, 
              FeedbackView({feedbackApiClient: stageFeedbackApiClient, step: "finished"})
            )
          ), 

          Section({name: "CallUrlExpiredView"}, 
            Example({summary: "Firefox User"}, 
              CallUrlExpiredView({helper: {isFirefox: returnTrue}})
            ), 
            Example({summary: "Non-Firefox User"}, 
              CallUrlExpiredView({helper: {isFirefox: returnFalse}})
            )
          ), 

          Section({name: "AlertMessages"}, 
            Example({summary: "Various alerts"}, 
              React.DOM.div({className: "alert alert-warning"}, 
                React.DOM.button({className: "close"}), 
                React.DOM.p({className: "message"}, 
                  "The person you were calling has ended the conversation."
                )
              ), 
              React.DOM.br(null), 
              React.DOM.div({className: "alert alert-error"}, 
                React.DOM.button({className: "close"}), 
                React.DOM.p({className: "message"}, 
                  "The person you were calling has ended the conversation."
                )
              )
            )
          ), 

          Section({name: "HomeView"}, 
            Example({summary: "Standalone Home View"}, 
              React.DOM.div({className: "standalone"}, 
                HomeView(null)
              )
            )
          ), 


          Section({name: "UnsupportedBrowserView"}, 
            Example({summary: "Standalone Unsupported Browser"}, 
              React.DOM.div({className: "standalone"}, 
                UnsupportedBrowserView(null)
              )
            )
          ), 

          Section({name: "UnsupportedDeviceView"}, 
            Example({summary: "Standalone Unsupported Device"}, 
              React.DOM.div({className: "standalone"}, 
                UnsupportedDeviceView(null)
              )
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
    var body = document.body;
    body.className = loop.shared.utils.getTargetPlatform();

    React.renderComponent(App(null), body);

    _renderComponentsInIframes();
  });

})();
