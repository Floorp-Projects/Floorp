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
  var CallUrlExpiredView = loop.webapp.CallUrlExpiredView;

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

  // Feedback API client configured to send data to the stage input server,
  // which is available at https://input.allizom.org
  var stageFeedbackApiClient = new loop.FeedbackAPIClient(
    "https://input.allizom.org/api/v1/feedback", {
      product: "Loop"
    });

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
            React.DOM.nav({className: "menu"}, 
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
            Example({summary: "Pending call url retrieval", dashed: "true", style: {width: "332px"}}, 
              PanelView(null)
            ), 
            Example({summary: "Call URL retrieved", dashed: "true", style: {width: "332px"}}, 
              PanelView({callUrl: "http://invalid.example.url/"})
            )
          ), 

          Section({name: "IncomingCallView"}, 
            Example({summary: "Default", dashed: "true", style: {width: "280px"}}, 
              IncomingCallView(null)
            )
          ), 

          Section({name: "ConversationToolbar"}, 
            Example({summary: "Default"}, 
              ConversationToolbar({video: {enabled: true}, audio: {enabled: true}})
            ), 
            Example({summary: "Video muted"}, 
              ConversationToolbar({video: {enabled: false}, audio: {enabled: true}})
            ), 
            Example({summary: "Audio muted"}, 
              ConversationToolbar({video: {enabled: true}, audio: {enabled: false}})
            )
          ), 

          Section({name: "ConversationView"}, 
            Example({summary: "Default"}, 
              ConversationView({video: {enabled: true}, audio: {enabled: true}})
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
              FeedbackView({step: "form"})
            ), 
            Example({summary: "Thank you!", dashed: "true", style: {width: "280px"}}, 
              FeedbackView({step: "finished"})
            )
          ), 

          Section({name: "CallUrlExpiredView"}, 
            Example({summary: "Firefox User"}, 
              CallUrlExpiredView({helper: {isFirefox: returnTrue}})
            ), 
            Example({summary: "Non-Firefox User"}, 
              CallUrlExpiredView({helper: {isFirefox: returnFalse}})
            )
          )
        )
      );
    }
  });

  window.addEventListener("DOMContentLoaded", function() {
    React.renderComponent(App(null), document.body);
  });
})();
