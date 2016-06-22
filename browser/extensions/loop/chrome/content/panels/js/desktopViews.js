"use strict"; /* This Source Code Form is subject to the terms of the Mozilla Public
               * License, v. 2.0. If a copy of the MPL was not distributed with this file,
               * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.shared = loop.shared || {};
loop.shared.desktopViews = function (mozL10n) {
  "use strict";

  var sharedActions = loop.shared.actions;
  var sharedMixins = loop.shared.mixins;
  var sharedUtils = loop.shared.utils;

  var CopyLinkButton = React.createClass({ displayName: "CopyLinkButton", 
    statics: { 
      TRIGGERED_RESET_DELAY: 2000 }, 


    mixins: [React.addons.PureRenderMixin], 

    propTypes: { 
      callback: React.PropTypes.func, 
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired, 
      locationForMetrics: React.PropTypes.string.isRequired, 
      roomData: React.PropTypes.object.isRequired }, 


    getInitialState: function getInitialState() {
      return { 
        copiedUrl: false };}, 



    handleCopyButtonClick: function handleCopyButtonClick(event) {
      event.preventDefault();

      this.props.dispatcher.dispatch(new sharedActions.CopyRoomUrl({ 
        roomUrl: this.props.roomData.roomUrl, 
        from: this.props.locationForMetrics }));


      this.setState({ copiedUrl: true });
      setTimeout(this.resetTriggeredButtons, this.constructor.TRIGGERED_RESET_DELAY);}, 


    /**
     * Reset state of triggered buttons if necessary
     */
    resetTriggeredButtons: function resetTriggeredButtons() {
      if (this.state.copiedUrl) {
        this.setState({ copiedUrl: false });
        this.props.callback && this.props.callback();}}, 



    render: function render() {
      var cx = classNames;
      return (
        React.createElement("div", { className: cx({ 
            "group-item-bottom": true, 
            "btn": true, 
            "invite-button": true, 
            "btn-copy": true, 
            "triggered": this.state.copiedUrl }), 

          onClick: this.handleCopyButtonClick }, mozL10n.get(this.state.copiedUrl ? 
        "invite_copied_link_button" : "invite_copy_link_button")));} });





  var EmailLinkButton = React.createClass({ displayName: "EmailLinkButton", 
    mixins: [React.addons.PureRenderMixin], 

    propTypes: { 
      callback: React.PropTypes.func, 
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired, 
      locationForMetrics: React.PropTypes.string.isRequired, 
      roomData: React.PropTypes.object.isRequired }, 


    handleEmailButtonClick: function handleEmailButtonClick(event) {
      event.preventDefault();

      var roomData = this.props.roomData;
      var contextURL = roomData.roomContextUrls && roomData.roomContextUrls[0];
      if (contextURL) {
        if (contextURL.location === null) {
          contextURL = undefined;} else 
        {
          contextURL = sharedUtils.formatURL(contextURL.location).hostname;}}



      this.props.dispatcher.dispatch(
      new sharedActions.EmailRoomUrl({ 
        roomUrl: roomData.roomUrl, 
        roomDescription: contextURL, 
        from: this.props.locationForMetrics }));


      this.props.callback && this.props.callback();}, 


    render: function render() {
      return (
        React.createElement("div", { className: "btn-email invite-button", 
          onClick: this.handleEmailButtonClick }, 
        React.createElement("img", { src: "shared/img/glyph-email-16x16.svg" }), 
        React.createElement("p", null, mozL10n.get("invite_email_link_button"))));} });





  var FacebookShareButton = React.createClass({ displayName: "FacebookShareButton", 
    mixins: [React.addons.PureRenderMixin], 

    propTypes: { 
      callback: React.PropTypes.func, 
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired, 
      locationForMetrics: React.PropTypes.string.isRequired, 
      roomData: React.PropTypes.object.isRequired }, 


    handleFacebookButtonClick: function handleFacebookButtonClick(event) {
      event.preventDefault();

      this.props.dispatcher.dispatch(new sharedActions.FacebookShareRoomUrl({ 
        from: this.props.locationForMetrics, 
        roomUrl: this.props.roomData.roomUrl }));


      this.props.callback && this.props.callback();}, 


    render: function render() {
      return (
        React.createElement("div", { className: "btn-facebook invite-button", 
          onClick: this.handleFacebookButtonClick }, 
        React.createElement("img", { src: "shared/img/glyph-facebook-16x16.svg" }), 
        React.createElement("p", null, mozL10n.get("invite_facebook_button3"))));} });





  var SharePanelView = React.createClass({ displayName: "SharePanelView", 
    mixins: [sharedMixins.DropdownMenuMixin(".room-invitation-overlay")], 

    propTypes: { 
      callback: React.PropTypes.func, 
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired, 
      error: React.PropTypes.object, 
      facebookEnabled: React.PropTypes.bool.isRequired, 
      locationForMetrics: React.PropTypes.string.isRequired, 
      // This data is supplied by the activeRoomStore.
      roomData: React.PropTypes.object.isRequired, 
      show: React.PropTypes.bool.isRequired }, 


    render: function render() {var _this = this;
      if (!this.props.show || !this.props.roomData.roomUrl) {
        return null;}


      var cx = classNames;

      return (
        React.createElement("div", { className: "room-invitation-overlay" }, 
        React.createElement("div", { className: "room-invitation-content" }, 
        React.createElement("div", { className: "room-context-header" }, mozL10n.get("invite_header_text_bold2")), 
        React.createElement("div", null, mozL10n.get("invite_header_text4"))), 

        React.createElement("div", { className: "input-button-group" }, 
        React.createElement("div", { className: "input-button-group-label" }, mozL10n.get("invite_your_link")), 
        React.createElement("div", { className: "input-button-content" }, 
        React.createElement("div", { className: "input-group group-item-top" }, 
        React.createElement("input", { readOnly: true, type: "text", value: this.props.roomData.roomUrl })), 

        React.createElement(CopyLinkButton, { 
          callback: this.props.callback, 
          dispatcher: this.props.dispatcher, 
          locationForMetrics: this.props.locationForMetrics, 
          roomData: this.props.roomData }))), 


        React.createElement("div", { className: cx({ 
            "btn-group": true, 
            "share-action-group": true }) }, 

        React.createElement(EmailLinkButton, { 
          callback: this.props.callback, 
          dispatcher: this.props.dispatcher, 
          locationForMetrics: this.props.locationForMetrics, 
          roomData: this.props.roomData }), 
        function () {
          if (_this.props.facebookEnabled) {
            return React.createElement(FacebookShareButton, { 
              callback: _this.props.callback, 
              dispatcher: _this.props.dispatcher, 
              locationForMetrics: _this.props.locationForMetrics, 
              roomData: _this.props.roomData });}

          return null;}())));} });







  return { 
    CopyLinkButton: CopyLinkButton, 
    EmailLinkButton: EmailLinkButton, 
    FacebookShareButton: FacebookShareButton, 
    SharePanelView: SharePanelView };}(

navigator.mozL10n || document.mozL10n);
