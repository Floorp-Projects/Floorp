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

  var Example = React.createClass({
    render: function() {
      var cx = React.addons.classSet;
      return (
        <div className="example">
          <h3>{this.props.summary}</h3>
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
            <nav className="menu">{
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
            <Example summary="Pending call url retrieval" dashed="true" style={{width: "332px"}}>
              <PanelView />
            </Example>
            <Example summary="Call URL retrieved" dashed="true" style={{width: "332px"}}>
              <PanelView callUrl="http://invalid.example.url/" />
            </Example>
          </Section>

          <Section name="IncomingCallView">
            <Example summary="Default" dashed="true" style={{width: "280px"}}>
              <IncomingCallView />
            </Example>
          </Section>

          <Section name="ConversationToolbar">
            <Example summary="Default">
              <ConversationToolbar video={{enabled: true}} audio={{enabled: true}} />
            </Example>
            <Example summary="Video muted">
              <ConversationToolbar video={{enabled: false}} audio={{enabled: true}} />
            </Example>
            <Example summary="Audio muted">
              <ConversationToolbar video={{enabled: true}} audio={{enabled: false}} />
            </Example>
          </Section>

          <Section name="ConversationView">
            <Example summary="Default">
              <ConversationView video={{enabled: true}} audio={{enabled: true}} />
            </Example>
          </Section>

          <Section name="FeedbackView">
            <p className="note">
              <strong>Note:</strong> For the useable demo, you can access submitted data at&nbsp;
              <a href="https://input.allizom.org/">input.allizom.org</a>.
            </p>
            <Example summary="Default (useable demo)" dashed="true" style={{width: "280px"}}>
              <FeedbackView feedbackApiClient={stageFeedbackApiClient} />
            </Example>
            <Example summary="Detailed form" dashed="true" style={{width: "280px"}}>
              <FeedbackView step="form" />
            </Example>
            <Example summary="Thank you!" dashed="true" style={{width: "280px"}}>
              <FeedbackView step="finished" />
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
        </ShowCase>
      );
    }
  });

  window.addEventListener("DOMContentLoaded", function() {
    React.renderComponent(<App />, document.body);
  });
})();
