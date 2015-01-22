/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* jshint newcap:false */
/* global loop:true, React */
var loop = loop || {};
loop.shared = loop.shared || {};
loop.shared.views = loop.shared.views || {};
loop.shared.views.FeedbackView = (function(l10n) {
  "use strict";

  var sharedActions = loop.shared.actions;
  var sharedMixins = loop.shared.mixins;

  var WINDOW_AUTOCLOSE_TIMEOUT_IN_SECONDS =
      loop.shared.views.WINDOW_AUTOCLOSE_TIMEOUT_IN_SECONDS = 5;
  var FEEDBACK_STATES = loop.store.FEEDBACK_STATES;

  /**
   * Feedback outer layout.
   *
   * Props:
   * -
   */
  var FeedbackLayout = React.createClass({displayName: "FeedbackLayout",
    propTypes: {
      children: React.PropTypes.component.isRequired,
      title: React.PropTypes.string.isRequired,
      reset: React.PropTypes.func // if not specified, no Back btn is shown
    },

    render: function() {
      var backButton = React.createElement("div", null);
      if (this.props.reset) {
        backButton = (
          React.createElement("button", {className: "fx-embedded-btn-back", type: "button", 
                  onClick: this.props.reset}, 
            "« ", l10n.get("feedback_back_button")
          )
        );
      }
      return (
        React.createElement("div", {className: "feedback"}, 
          backButton, 
          React.createElement("h3", null, this.props.title), 
          this.props.children
        )
      );
    }
  });

  /**
   * Detailed feedback form.
   */
  var FeedbackForm = React.createClass({displayName: "FeedbackForm",
    propTypes: {
      feedbackStore: React.PropTypes.instanceOf(loop.store.FeedbackStore),
      pending:       React.PropTypes.bool,
      reset:         React.PropTypes.func
    },

    getInitialState: function() {
      return {category: "", description: ""};
    },

    getDefaultProps: function() {
      return {pending: false};
    },

    _getCategories: function() {
      return {
        audio_quality: l10n.get("feedback_category_audio_quality"),
        video_quality: l10n.get("feedback_category_video_quality"),
        disconnected : l10n.get("feedback_category_was_disconnected"),
        confusing:     l10n.get("feedback_category_confusing2"),
        other:         l10n.get("feedback_category_other2")
      };
    },

    _getCategoryFields: function() {
      var categories = this._getCategories();
      return Object.keys(categories).map(function(category, key) {
        return (
          React.createElement("label", {key: key, className: "feedback-category-label"}, 
            React.createElement("input", {type: "radio", ref: "category", name: "category", 
                   className: "feedback-category-radio", 
                   value: category, 
                   onChange: this.handleCategoryChange, 
                   checked: this.state.category === category}), 
            categories[category]
          )
        );
      }, this);
    },

    /**
     * Checks if the form is ready for submission:
     *
     * - no feedback submission should be pending.
     * - a category (reason) must be chosen;
     * - if the "other" category is chosen, a custom description must have been
     *   entered by the end user;
     *
     * @return {Boolean}
     */
    _isFormReady: function() {
      if (this.props.pending || !this.state.category) {
        return false;
      }
      if (this.state.category === "other" && !this.state.description) {
        return false;
      }
      return true;
    },

    handleCategoryChange: function(event) {
      var category = event.target.value;
      this.setState({
        category: category
      });
      if (category == "other") {
        this.refs.description.getDOMNode().focus();
      }
    },

    handleDescriptionFieldChange: function(event) {
      this.setState({description: event.target.value});
    },

    handleFormSubmit: function(event) {
      event.preventDefault();
      // XXX this feels ugly, we really want a feedbackActions object here.
      this.props.feedbackStore.dispatchAction(new sharedActions.SendFeedback({
        happy: false,
        category: this.state.category,
        description: this.state.description
      }));
    },

    render: function() {
      return (
        React.createElement(FeedbackLayout, {title: l10n.get("feedback_category_list_heading"), 
                        reset: this.props.reset}, 
          React.createElement("form", {onSubmit: this.handleFormSubmit}, 
            this._getCategoryFields(), 
            React.createElement("p", null, 
              React.createElement("input", {type: "text", ref: "description", name: "description", 
                className: "feedback-description", 
                onChange: this.handleDescriptionFieldChange, 
                value: this.state.description, 
                placeholder: 
                  l10n.get("feedback_custom_category_text_placeholder")})
            ), 
            React.createElement("button", {type: "submit", className: "btn btn-success", 
                    disabled: !this._isFormReady()}, 
              l10n.get("feedback_submit_button")
            )
          )
        )
      );
    }
  });

  /**
   * Feedback received view.
   *
   * Props:
   * - {Function} onAfterFeedbackReceived Function to execute after the
   *   WINDOW_AUTOCLOSE_TIMEOUT_IN_SECONDS timeout has elapsed
   */
  var FeedbackReceived = React.createClass({displayName: "FeedbackReceived",
    propTypes: {
      onAfterFeedbackReceived: React.PropTypes.func
    },

    getInitialState: function() {
      return {countdown: WINDOW_AUTOCLOSE_TIMEOUT_IN_SECONDS};
    },

    componentDidMount: function() {
      this._timer = setInterval(function() {
      if (this.state.countdown == 1) {
        clearInterval(this._timer);
        if (this.props.onAfterFeedbackReceived) {
          this.props.onAfterFeedbackReceived();
        }
        return;
      }
        this.setState({countdown: this.state.countdown - 1});
      }.bind(this), 1000);
    },

    componentWillUnmount: function() {
      if (this._timer) {
        clearInterval(this._timer);
      }
    },

    render: function() {
      return (
        React.createElement(FeedbackLayout, {title: l10n.get("feedback_thank_you_heading")}, 
          React.createElement("p", {className: "info thank-you"}, 
            l10n.get("feedback_window_will_close_in2", {
              countdown: this.state.countdown,
              num: this.state.countdown
            }))
        )
      );
    }
  });

  /**
   * Feedback view.
   */
  var FeedbackView = React.createClass({displayName: "FeedbackView",
    mixins: [
      Backbone.Events,
      loop.store.StoreMixin("feedbackStore")
    ],

    propTypes: {
      onAfterFeedbackReceived: React.PropTypes.func,
      // Used by the UI showcase.
      feedbackState: React.PropTypes.string
    },

    getInitialState: function() {
      var storeState = this.getStoreState();
      return _.extend({}, storeState, {
        feedbackState: this.props.feedbackState || storeState.feedbackState
      });
    },

    reset: function() {
      this.setState(this.getStore().getInitialStoreState());
    },

    handleHappyClick: function() {
      // XXX: If the user is happy, we directly send this information to the
      //      feedback API; this is a behavior we might want to revisit later.
      this.getStore().dispatchAction(new sharedActions.SendFeedback({
        happy: true,
        category: "",
        description: ""
      }));
    },

    handleSadClick: function() {
      this.getStore().dispatchAction(
        new sharedActions.RequireFeedbackDetails());
    },

    _onFeedbackSent: function(err) {
      if (err) {
        // XXX better end user error reporting, see bug 1046738
        console.error("Unable to send user feedback", err);
      }
      this.setState({pending: false, step: "finished"});
    },

    render: function() {
      switch(this.state.feedbackState) {
        default:
        case FEEDBACK_STATES.INIT: {
          return (
            React.createElement(FeedbackLayout, {title: 
              l10n.get("feedback_call_experience_heading2")}, 
              React.createElement("div", {className: "faces"}, 
                React.createElement("button", {className: "face face-happy", 
                        onClick: this.handleHappyClick}), 
                React.createElement("button", {className: "face face-sad", 
                        onClick: this.handleSadClick})
              )
            )
          );
        }
        case FEEDBACK_STATES.DETAILS: {
          return (
            React.createElement(FeedbackForm, {
              feedbackStore: this.getStore(), 
              reset: this.reset, 
              pending: this.state.feedbackState === FEEDBACK_STATES.PENDING})
            );
        }
        case FEEDBACK_STATES.PENDING:
        case FEEDBACK_STATES.SENT:
        case FEEDBACK_STATES.FAILED: {
          if (this.state.error) {
            // XXX better end user error reporting, see bug 1046738
            console.error("Error encountered while submitting feedback",
                          this.state.error);
          }
          return (
            React.createElement(FeedbackReceived, {
              onAfterFeedbackReceived: this.props.onAfterFeedbackReceived})
          );
        }
      }
    }
  });

  return FeedbackView;
})(navigator.mozL10n || document.mozL10n);
