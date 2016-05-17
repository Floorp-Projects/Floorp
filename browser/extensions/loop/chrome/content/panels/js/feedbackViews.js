"use strict"; /* This Source Code Form is subject to the terms of the Mozilla Public
               * License, v. 2.0. If a copy of the MPL was not distributed with this file,
               * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.feedbackViews = function (_, mozL10n) {
  "use strict";

  /**
   * Feedback view is displayed once every 6 months (loop.feedback.periodSec)
   * after a conversation has ended.
   */
  var FeedbackView = React.createClass({ displayName: "FeedbackView", 
    propTypes: { 
      onAfterFeedbackReceived: React.PropTypes.func.isRequired }, 


    /**
     * Pressing the button to leave feedback will open the form in a new page
     * and close the conversation window.
     */
    onFeedbackButtonClick: function onFeedbackButtonClick() {
      loop.requestMulti(
      ["GetLoopPref", "feedback.formURL"], 
      ["GetAddonVersion"]).
      then(function (results) {
        if (results[0] && results[1]) {
          var finalURL = results[0].replace("%APP_VERSION%", results[1]);
          loop.request("OpenURL", finalURL).then(this.props.onAfterFeedbackReceived);}}.

      bind(this));}, 


    render: function render() {
      return (
        React.createElement("div", { className: "feedback-view-container" }, 
        React.createElement("h2", { className: "feedback-heading" }, 
        mozL10n.get("feedback_window_heading")), 

        React.createElement("div", { className: "feedback-hello-logo" }), 
        React.createElement("div", { className: "feedback-button-container" }, 
        React.createElement("button", { onClick: this.onFeedbackButtonClick, 
          ref: "feedbackFormBtn" }, 
        mozL10n.get("feedback_request_button")))));} });







  return { 
    FeedbackView: FeedbackView };}(

_, navigator.mozL10n || document.mozL10n);
