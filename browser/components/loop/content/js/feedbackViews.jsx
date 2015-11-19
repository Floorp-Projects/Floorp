/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.feedbackViews = (function(_, mozL10n) {
  "use strict";

  /**
   * Feedback view is displayed once every 6 months (loop.feedback.periodSec)
   * after a conversation has ended.
   */
  var FeedbackView = React.createClass({
    propTypes: {
      onAfterFeedbackReceived: React.PropTypes.func.isRequired
    },

    /**
     * Pressing the button to leave feedback will open the form in a new page
     * and close the conversation window.
     */
    onFeedbackButtonClick: function() {
      loop.request("GetLoopPref", "feedback.formURL").then(function(url) {
        loop.request("OpenURL", url);

        this.props.onAfterFeedbackReceived();
      }.bind(this));
    },

    render: function() {
      return (
        <div className="feedback-view-container">
          <h2 className="feedback-heading">
            {mozL10n.get("feedback_window_heading")}
          </h2>
          <div className="feedback-hello-logo" />
          <div className="feedback-button-container">
            <button onClick={this.onFeedbackButtonClick}
              ref="feedbackFormBtn">
              {mozL10n.get("feedback_request_button")}
            </button>
          </div>
        </div>
      );
    }
  });

  return {
    FeedbackView: FeedbackView
  };
})(_, navigator.mozL10n || document.mozL10n);
