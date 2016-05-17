"use strict"; /* This Source Code Form is subject to the terms of the Mozilla Public
               * License, v. 2.0. If a copy of the MPL was not distributed with this file,
               * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.shared = loop.shared || {};
loop.shared.views = function (_, mozL10n) {
  "use strict";

  var sharedActions = loop.shared.actions;

  /**
   * Hang-up control button.
   *
   * Required props:
   * - {Function} action  Function to be executed on click.
   * - {String}   title   Tooltip functionality.
   */
  var HangUpControlButton = React.createClass({ displayName: "HangUpControlButton", 
    mixins: [
    React.addons.PureRenderMixin], 


    propTypes: { 
      action: React.PropTypes.func.isRequired, 
      title: React.PropTypes.string }, 


    handleClick: function handleClick() {
      this.props.action();}, 


    render: function render() {
      return (
        React.createElement("button", { className: "btn btn-hangup", 
          onClick: this.handleClick, 
          title: this.props.title }));} });




  /**
   * Media control button.
   *
   * Required props:
   * - {String}   scope   Media scope, can be "local" or "remote".
   * - {String}   type    Media type, can be "audio" or "video".
   * - {Function} action  Function to be executed on click.
   * - {Bool} muted Stream activation status (default: false).
   */
  var MediaControlButton = React.createClass({ displayName: "MediaControlButton", 
    propTypes: { 
      action: React.PropTypes.func.isRequired, 
      disabled: React.PropTypes.bool, 
      muted: React.PropTypes.bool.isRequired, 
      scope: React.PropTypes.string.isRequired, 
      title: React.PropTypes.string, 
      type: React.PropTypes.string.isRequired, 
      visible: React.PropTypes.bool.isRequired }, 


    getDefaultProps: function getDefaultProps() {
      return { 
        disabled: false, 
        muted: false, 
        visible: true };}, 



    handleClick: function handleClick() {
      this.props.action();}, 


    _getClasses: function _getClasses() {
      var cx = classNames;
      // classes
      var classesObj = { 
        "btn": true, 
        "media-control": true, 
        "transparent-button": true, 
        "local-media": this.props.scope === "local", 
        "muted": this.props.muted || this.props.disabled, 
        "hide": !this.props.visible, 
        "disabled": this.props.disabled };

      classesObj["btn-mute-" + this.props.type] = true;
      return cx(classesObj);}, 


    _getTitle: function _getTitle() {
      if (this.props.title) {
        return this.props.title;}


      var prefix = this.props.muted || this.props.disabled ? "unmute" : "mute";
      var suffix = this.props.type === "video" ? "button_title2" : "button_title";
      var msgId = [prefix, this.props.scope, this.props.type, suffix].join("_");
      return mozL10n.get(msgId);}, 


    render: function render() {
      return (
        React.createElement("button", { className: this._getClasses(), 
          onClick: this.handleClick, 
          title: this._getTitle() }));} });




  /**
   * Conversation controls.
   */
  var ConversationToolbar = React.createClass({ displayName: "ConversationToolbar", 
    getDefaultProps: function getDefaultProps() {
      return { 
        video: { enabled: true, visible: true }, 
        audio: { enabled: true, visible: true }, 
        showHangup: true };}, 



    getInitialState: function getInitialState() {
      return { 
        idle: false };}, 



    propTypes: { 
      audio: React.PropTypes.object.isRequired, 
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired, 
      hangup: React.PropTypes.func.isRequired, 
      showHangup: React.PropTypes.bool, 
      video: React.PropTypes.object.isRequired }, 


    handleClickHangup: function handleClickHangup() {
      this.props.hangup();}, 


    componentDidMount: function componentDidMount() {
      this.userActivity = false;
      this.startIdleCountDown();
      document.body.addEventListener("mousemove", this._onBodyMouseMove);}, 


    componentWillUnmount: function componentWillUnmount() {
      clearTimeout(this.inactivityTimeout);
      clearInterval(this.inactivityPollInterval);
      document.body.removeEventListener("mousemove", this._onBodyMouseMove);}, 


    /**
     * If the conversation toolbar is idle, update its state and initialize the countdown
     * to return of the idle state. If the toolbar is active, only it's updated the userActivity flag.
     */
    _onBodyMouseMove: function _onBodyMouseMove() {
      if (this.state.idle) {
        this.setState({ idle: false });
        this.startIdleCountDown();} else 
      {
        this.userActivity = true;}}, 



    /**
     * Instead of resetting the timeout for every mousemove (this event is called to many times,
     * when the mouse is moving, we check the flat userActivity every 4 seconds. If the flag is activated,
     * the user is still active, and we can restart the countdown for the idle state
     */
    checkUserActivity: function checkUserActivity() {
      this.inactivityPollInterval = setInterval(function () {
        if (this.userActivity) {
          this.userActivity = false;
          this.restartIdleCountDown();}}.

      bind(this), 4000);}, 


    /**
     * Stop the execution of the current inactivity countdown and it starts a new one.
     */
    restartIdleCountDown: function restartIdleCountDown() {
      clearTimeout(this.inactivityTimeout);
      this.startIdleCountDown();}, 


    /**
     * Launchs the process to check the user activity and the inactivity countdown to change
     * the toolbar to idle.
     * When the toolbar changes to idle, we remove the procces to check the user activity,
     * because the toolbar is going to be updated directly when the user moves the mouse.
     */
    startIdleCountDown: function startIdleCountDown() {
      this.checkUserActivity();
      this.inactivityTimeout = setTimeout(function () {
        this.setState({ idle: true });
        clearInterval(this.inactivityPollInterval);}.
      bind(this), 6000);}, 


    render: function render() {
      var cx = classNames;
      var conversationToolbarCssClasses = cx({ 
        "conversation-toolbar": true, 
        "idle": this.state.idle });

      var showButtons = this.props.video.visible || this.props.audio.visible;
      var mediaButtonGroupCssClasses = cx({ 
        "conversation-toolbar-media-btn-group-box": true, 
        "hide": !showButtons });

      return (
        React.createElement("ul", { className: conversationToolbarCssClasses }, 

        this.props.showHangup && showButtons ? 
        React.createElement("li", { className: "conversation-toolbar-btn-box btn-hangup-entry" }, 
        React.createElement(HangUpControlButton, { action: this.handleClickHangup, 
          title: mozL10n.get("rooms_leave_button_label") })) : 
        null, 


        React.createElement("li", { className: "conversation-toolbar-btn-box" }, 
        React.createElement("div", { className: mediaButtonGroupCssClasses }, 
        React.createElement(VideoMuteButton, { dispatcher: this.props.dispatcher, 
          muted: !this.props.video.enabled }), 
        React.createElement(AudioMuteButton, { dispatcher: this.props.dispatcher, 
          muted: !this.props.audio.enabled })))));} });







  var AudioMuteButton = React.createClass({ displayName: "AudioMuteButton", 
    propTypes: { 
      disabled: React.PropTypes.bool, 
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher), 
      muted: React.PropTypes.bool.isRequired }, 


    getDefaultProps: function getDefaultProps() {
      return { 
        disabled: false };}, 



    toggleAudio: function toggleAudio() {
      if (this.props.disabled) {
        return;}


      this.props.dispatcher.dispatch(
      new sharedActions.SetMute({ type: "audio", enabled: this.props.muted }));}, 



    render: function render() {
      return (
        React.createElement(MediaControlButton, { action: this.toggleAudio, 
          disabled: this.props.disabled, 
          muted: this.props.muted, 
          scope: "local", 
          type: "audio" }));} });




  var VideoMuteButton = React.createClass({ displayName: "VideoMuteButton", 
    propTypes: { 
      disabled: React.PropTypes.bool, 
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher), 
      muted: React.PropTypes.bool.isRequired }, 


    getDefaultProps: function getDefaultProps() {
      return { 
        disabled: false };}, 



    toggleVideo: function toggleVideo() {
      if (this.props.disabled) {
        return;}


      this.props.dispatcher.dispatch(
      new sharedActions.SetMute({ type: "video", enabled: this.props.muted }));}, 



    render: function render() {
      return (
        React.createElement(MediaControlButton, { action: this.toggleVideo, 
          disabled: this.props.disabled, 
          muted: this.props.muted || this.props.disabled, 
          scope: "local", 
          type: "video" }));} });




  var Button = React.createClass({ displayName: "Button", 
    propTypes: { 
      additionalClass: React.PropTypes.string, 
      caption: React.PropTypes.string.isRequired, 
      children: React.PropTypes.element, 
      disabled: React.PropTypes.bool, 
      htmlId: React.PropTypes.string, 
      onClick: React.PropTypes.func.isRequired }, 


    getDefaultProps: function getDefaultProps() {
      return { 
        disabled: false, 
        additionalClass: "", 
        htmlId: "" };}, 



    render: function render() {
      var cx = classNames;
      var classObject = { button: true, disabled: this.props.disabled };
      if (this.props.additionalClass) {
        classObject[this.props.additionalClass] = true;}

      return (
        React.createElement("button", { className: cx(classObject), 
          disabled: this.props.disabled, 
          id: this.props.htmlId, 
          onClick: this.props.onClick }, 
        React.createElement("span", { className: "button-caption" }, this.props.caption), 
        this.props.children));} });





  var ButtonGroup = React.createClass({ displayName: "ButtonGroup", 
    propTypes: { 
      additionalClass: React.PropTypes.string, 
      children: React.PropTypes.oneOfType([
      React.PropTypes.element, 
      React.PropTypes.arrayOf(React.PropTypes.element)]) }, 



    getDefaultProps: function getDefaultProps() {
      return { 
        additionalClass: "" };}, 



    render: function render() {
      var cx = classNames;
      var classObject = { "button-group": true };
      if (this.props.additionalClass) {
        classObject[this.props.additionalClass] = true;}

      return (
        React.createElement("div", { className: cx(classObject) }, 
        this.props.children));} });





  var Checkbox = React.createClass({ displayName: "Checkbox", 
    propTypes: { 
      additionalClass: React.PropTypes.string, 
      checked: React.PropTypes.bool, 
      disabled: React.PropTypes.bool, 
      label: React.PropTypes.string, 
      onChange: React.PropTypes.func.isRequired, 
      // If true, this will cause the label to be cut off at the end of the
      // first line with an ellipsis, and a tooltip supplied.
      useEllipsis: React.PropTypes.bool, 
      // If `value` is not supplied, the consumer should rely on the boolean
      // `checked` state changes.
      value: React.PropTypes.string }, 


    getDefaultProps: function getDefaultProps() {
      return { 
        additionalClass: "", 
        checked: false, 
        disabled: false, 
        label: null, 
        useEllipsis: false, 
        value: "" };}, 



    componentWillReceiveProps: function componentWillReceiveProps(nextProps) {
      // Only change the state if the prop has changed, and if it is also
      // different from the state.
      if (this.props.checked !== nextProps.checked && 
      this.state.checked !== nextProps.checked) {
        this.setState({ checked: nextProps.checked });}}, 



    getInitialState: function getInitialState() {
      return { 
        checked: this.props.checked, 
        value: this.props.checked ? this.props.value : "" };}, 



    _handleClick: function _handleClick(event) {
      event.preventDefault();

      var newState = { 
        checked: !this.state.checked, 
        value: this.state.checked ? "" : this.props.value };

      this.setState(newState);
      this.props.onChange(newState);}, 


    render: function render() {
      var cx = classNames;
      var wrapperClasses = { 
        "checkbox-wrapper": true, 
        disabled: this.props.disabled };

      var checkClasses = { 
        checkbox: true, 
        checked: this.state.checked, 
        disabled: this.props.disabled };

      var labelClasses = { 
        "checkbox-label": true, 
        "ellipsis": this.props.useEllipsis };


      if (this.props.additionalClass) {
        wrapperClasses[this.props.additionalClass] = true;}

      return (
        React.createElement("div", { className: cx(wrapperClasses), 
          disabled: this.props.disabled, 
          onClick: this._handleClick }, 
        React.createElement("div", { className: cx(checkClasses) }), 

        this.props.label ? 
        React.createElement("div", { className: cx(labelClasses), 
          title: this.props.useEllipsis ? this.props.label : "" }, 
        this.props.label) : 
        null));} });






  /**
   * Renders an avatar element for display when video is muted.
   */
  var AvatarView = React.createClass({ displayName: "AvatarView", 
    mixins: [React.addons.PureRenderMixin], 

    render: function render() {
      return React.createElement("div", { className: "avatar" });} });



  /**
   * Renders a loading spinner for when video content is not yet available.
   */
  var LoadingView = React.createClass({ displayName: "LoadingView", 
    mixins: [React.addons.PureRenderMixin], 

    render: function render() {
      return (
        React.createElement("div", { className: "loading-background" }, 
        React.createElement("div", { className: "loading-stream" })));} });





  /**
   * Renders the a href link for context.
   *
   * @property {Boolean} allowClick         Set to true to allow the url to be clicked.
   * @property {String}  description        The description for the context url.
   * @property {Function}  handleClick      Function for handling click operation when
   *                                        context link is clicked
   * @property {String}  thumbnail          The thumbnail url (expected to be a data url) to
   *                                        display. If not specified, a fallback url will be
   *                                        shown.
   * @property {String}  url                The url to be displayed. If not present or invalid,
   *                                        the context link will not be clickable
   */
  var ContextUrlLink = React.createClass({ displayName: "ContextUrlLink", 
    mixins: [React.addons.PureRenderMixin], 

    propTypes: { 
      allowClick: React.PropTypes.bool.isRequired, 
      children: React.PropTypes.node, 
      description: React.PropTypes.string, 
      handleClick: React.PropTypes.func, 
      url: React.PropTypes.string }, 


    render: function render() {
      var sanitizedURL = loop.shared.utils.formatSanitizedContextURL(this.props.url);

      var opts = {};
      opts.classNames = classNames({ 
        "context-wrapper": true, 
        "clicks-allowed": this.props.allowClick });

      if (this.props.allowClick && sanitizedURL) {
        opts.href = sanitizedURL.location;}

      if (this.props.handleClick) {
        opts.onClick = this.props.handleClick;}

      if (this.props.description) {
        opts.title = this.props.description;}


      return (
        React.createElement("a", { className: opts.classNames, 
          href: opts.href, 
          onClick: opts.onClick, 
          rel: "noreferrer", 
          target: "_blank", 
          title: opts.title }, 
        this.props.children));} });





  /**
   * Renders a url that's part of context on the display.
   *
   * @property {Boolean} allowClick         Set to true to allow the url to be clicked. If this
   *                                        is specified, then 'dispatcher' is also required.
   * @property {String}  description        The description for the context url.
   * @property {loop.Dispatcher} dispatcher
   * @property {String}  thumbnail          The thumbnail url (expected to be a data url) to
   *                                        display. If not specified, a fallback url will be
   *                                        shown.
   * @property {String}  url                The url to be displayed. If not present or invalid,
   *                                        then this view won't be displayed.
   */
  var ContextUrlView = React.createClass({ displayName: "ContextUrlView", 
    mixins: [React.addons.PureRenderMixin], 

    propTypes: { 
      allowClick: React.PropTypes.bool.isRequired, 
      description: React.PropTypes.string, 
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher), 
      thumbnail: React.PropTypes.string, 
      url: React.PropTypes.string }, 


    /**
     * Dispatches an action to record when the link is clicked.
     */
    handleLinkClick: function handleLinkClick() {
      if (!this.props.allowClick) {
        return;}


      this.props.dispatcher.dispatch(new sharedActions.RecordClick({ 
        linkInfo: "Shared URL" }));}, 



    render: function render() {
      var description = this.props.description || null;
      var thumbnail = this.props.thumbnail;
      var url = this.props.url || null;
      var sanitizedURL = loop.shared.utils.formatSanitizedContextURL(url) || {};
      var hostname = sanitizedURL.hostname || null;

      if (!thumbnail) {
        thumbnail = "shared/img/icons-16x16.svg#globe";}


      return (
        React.createElement("div", { className: "context-content" }, 
        React.createElement(ContextUrlLink, { allowClick: this.props.allowClick, 
          description: description, 
          handleClick: this.handleLinkClick, 
          url: url }, 
        React.createElement("img", { className: "context-preview", src: thumbnail }), 
        React.createElement("span", { className: "context-info" }, 
        description, 
        React.createElement("span", { className: "context-url" }, 
        hostname)))));} });








  /**
   * Renders a media element for display. This also handles displaying an avatar
   * instead of the video, and attaching a video stream to the video element.
   */
  var MediaView = React.createClass({ displayName: "MediaView", 
    // srcMediaElement should be ok for a shallow comparison, so we are safe
    // to use the pure render mixin here.
    mixins: [React.addons.PureRenderMixin], 

    propTypes: { 
      cursorStore: React.PropTypes.instanceOf(loop.store.RemoteCursorStore), 
      dispatcher: React.PropTypes.object, 
      displayAvatar: React.PropTypes.bool.isRequired, 
      isLoading: React.PropTypes.bool.isRequired, 
      mediaType: React.PropTypes.string.isRequired, 
      posterUrl: React.PropTypes.string, 
      screenSharingPaused: React.PropTypes.bool, 
      shareCursor: React.PropTypes.bool, 
      // Expecting "local" or "remote".
      srcMediaElement: React.PropTypes.object }, 


    getInitialState: function getInitialState() {
      return { 
        videoElementSize: null };}, 



    componentDidMount: function componentDidMount() {
      if (!this.props.displayAvatar) {
        this.attachVideo(this.props.srcMediaElement);}


      if (this.props.shareCursor) {
        this.handleVideoDimensions();
        window.addEventListener("resize", this.handleVideoDimensions);}}, 



    componentWillUnmount: function componentWillUnmount() {
      var videoElement = ReactDOM.findDOMNode(this).querySelector("video");
      if (!this.props.shareCursor || !videoElement) {
        return;}


      window.removeEventListener("resize", this.handleVideoDimensions);
      videoElement.removeEventListener("loadeddata", this.handleVideoDimensions);
      videoElement.removeEventListener("mousemove", this.handleMousemove);
      videoElement.removeEventListener("click", this.handleMouseClick);}, 


    componentDidUpdate: function componentDidUpdate() {
      if (!this.props.displayAvatar) {
        this.attachVideo(this.props.srcMediaElement);}}, 



    handleVideoDimensions: function handleVideoDimensions() {
      var videoElement = ReactDOM.findDOMNode(this).querySelector("video");
      if (!videoElement) {
        return;}


      this.setState({ 
        videoElementSize: { 
          clientWidth: videoElement.clientWidth, 
          clientHeight: videoElement.clientHeight } });}, 




    MIN_CURSOR_DELTA: 3, 
    MIN_CURSOR_INTERVAL: 100, 
    lastCursorTime: 0, 
    lastCursorX: -1, 
    lastCursorY: -1, 

    handleMouseMove: function handleMouseMove(event) {
      // Only update every so often.
      var now = Date.now();
      if (now - this.lastCursorTime < this.MIN_CURSOR_INTERVAL) {
        return;}

      this.lastCursorTime = now;

      var storeState = this.props.cursorStore.getStoreState();

      // video is not at the top, so we need to calculate the offset
      var video = ReactDOM.findDOMNode(this).querySelector("video");
      var offset = video.getBoundingClientRect();

      var deltaX = event.clientX - 
      storeState.videoLetterboxing.left - 
      offset.left;
      var deltaY = event.clientY - 
      storeState.videoLetterboxing.top - 
      offset.top;

      // Skip the update if cursor is out of bounds
      if (deltaX < 0 || deltaX > storeState.streamVideoWidth || 
      deltaY < 0 || deltaY > storeState.streamVideoHeight || 
      // or the cursor didn't move the minimum.
      Math.abs(deltaX - this.lastCursorX) < this.MIN_CURSOR_DELTA && 
      Math.abs(deltaY - this.lastCursorY) < this.MIN_CURSOR_DELTA) {
        return;}


      this.lastCursorX = deltaX;
      this.lastCursorY = deltaY;

      this.props.dispatcher.dispatch(new sharedActions.SendCursorData({ 
        ratioX: deltaX / storeState.streamVideoWidth, 
        ratioY: deltaY / storeState.streamVideoHeight, 
        type: loop.shared.utils.CURSOR_MESSAGE_TYPES.POSITION }));}, 



    handleMouseClick: function handleMouseClick() {
      this.props.dispatcher.dispatch(new sharedActions.SendCursorData({ 
        type: loop.shared.utils.CURSOR_MESSAGE_TYPES.CLICK }));}, 



    /**
     * Attaches a video stream from a donor video element to this component's
     * video element if the component is displaying one.
     *
     * @param {Object} srcMediaElement The src video object to clone the stream
     *                                from.
     *
     * XXX need to have a corresponding detachVideo or change this to syncVideo
     * to protect from leaks (bug 1171978)
     */
    attachVideo: function attachVideo(srcMediaElement) {
      if (!srcMediaElement) {
        // Not got anything to display.
        return;}


      var videoElement = ReactDOM.findDOMNode(this).querySelector("video");
      if (!videoElement || videoElement.tagName.toLowerCase() !== "video") {
        // Must be displaying the avatar view, so don't try and attach video.
        return;}


      if (this.props.shareCursor && !this.props.screenSharingPaused) {
        videoElement.addEventListener("loadeddata", this.handleVideoDimensions);
        videoElement.addEventListener("mousemove", this.handleMouseMove);
        videoElement.addEventListener("click", this.handleMouseClick);}


      // Set the src of our video element
      var attrName = "";
      if ("srcObject" in videoElement) {
        // srcObject is according to the standard.
        attrName = "srcObject";} else 
      if ("mozSrcObject" in videoElement) {
        // mozSrcObject is for Firefox
        attrName = "mozSrcObject";} else 
      if ("src" in videoElement) {
        // src is for Chrome.
        attrName = "src";} else 
      {
        console.error("Error attaching stream to element - no supported" + 
        "attribute found");
        return;}


      // If the object hasn't changed it, then don't reattach it.
      if (videoElement[attrName] !== srcMediaElement[attrName]) {
        videoElement[attrName] = srcMediaElement[attrName];}


      videoElement.play();}, 


    render: function render() {
      if (this.props.isLoading) {
        return React.createElement(LoadingView, null);}


      if (this.props.displayAvatar) {
        return React.createElement(AvatarView, null);}


      if (!this.props.srcMediaElement && !this.props.posterUrl) {
        return React.createElement("div", { className: "no-video" });}


      // For now, always mute media. For local media, we should be muted anyway,
      // as we don't want to hear ourselves speaking.
      //
      // For remote media, we would ideally have this live video element in
      // control of the audio, but due to the current method of not rendering
      // the element at all when video is muted we have to rely on the hidden
      // dom element in the sdk driver to play the audio.
      // We might want to consider changing this if we add UI controls relating
      // to the remote audio at some stage in the future.
      return (
        React.createElement("div", { className: "remote-video-box" }, 
        this.state.videoElementSize && this.props.shareCursor ? 
        React.createElement(RemoteCursorView, { 
          videoElementSize: this.state.videoElementSize }) : 
        null, 
        React.createElement("video", { className: this.props.mediaType + "-video", 
          muted: true, 
          poster: this.props.posterUrl })));} });





  var MediaLayoutView = React.createClass({ displayName: "MediaLayoutView", 
    propTypes: { 
      children: React.PropTypes.node, 
      cursorStore: React.PropTypes.instanceOf(loop.store.RemoteCursorStore).isRequired, 
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired, 
      displayScreenShare: React.PropTypes.bool.isRequired, 
      isLocalLoading: React.PropTypes.bool.isRequired, 
      isRemoteLoading: React.PropTypes.bool.isRequired, 
      isScreenShareLoading: React.PropTypes.bool.isRequired, 
      // The poster URLs are for UI-showcase testing and development.
      localPosterUrl: React.PropTypes.string, 
      localSrcMediaElement: React.PropTypes.object, 
      localVideoMuted: React.PropTypes.bool.isRequired, 
      // Passing in matchMedia, allows it to be overriden for ui-showcase's
      // benefit. We expect either the override or window.matchMedia.
      matchMedia: React.PropTypes.func.isRequired, 
      remotePosterUrl: React.PropTypes.string, 
      remoteSrcMediaElement: React.PropTypes.object, 
      renderRemoteVideo: React.PropTypes.bool.isRequired, 
      screenShareMediaElement: React.PropTypes.object, 
      screenSharePosterUrl: React.PropTypes.string, 
      screenSharingPaused: React.PropTypes.bool, 
      showInitialContext: React.PropTypes.bool.isRequired, 
      showMediaWait: React.PropTypes.bool.isRequired, 
      showTile: React.PropTypes.bool.isRequired }, 


    isLocalMediaAbsolutelyPositioned: function isLocalMediaAbsolutelyPositioned(matchMedia) {
      if (!matchMedia) {
        matchMedia = this.props.matchMedia;}

      return matchMedia && (
      // The screen width is less than 640px and we are not screen sharing.
      matchMedia("screen and (max-width:640px)").matches && 
      !this.props.displayScreenShare || 
      // or the screen width is less than 300px.
      matchMedia("screen and (max-width:300px)").matches);}, 


    getInitialState: function getInitialState() {
      return { 
        localMediaAboslutelyPositioned: this.isLocalMediaAbsolutelyPositioned() };}, 



    componentWillReceiveProps: function componentWillReceiveProps(nextProps) {
      // This is all for the ui-showcase's benefit.
      if (this.props.matchMedia !== nextProps.matchMedia) {
        this.updateLocalMediaState(null, nextProps.matchMedia);}}, 



    componentDidMount: function componentDidMount() {
      window.addEventListener("resize", this.updateLocalMediaState);}, 


    componentWillUnmount: function componentWillUnmount() {
      window.removeEventListener("resize", this.updateLocalMediaState);}, 


    updateLocalMediaState: function updateLocalMediaState(event, matchMedia) {
      var newState = this.isLocalMediaAbsolutelyPositioned(matchMedia);
      if (this.state.localMediaAboslutelyPositioned !== newState) {
        this.setState({ 
          localMediaAboslutelyPositioned: newState });}}, 




    renderLocalVideo: function renderLocalVideo() {
      return (
        React.createElement("div", { className: "local" }, 
        React.createElement(MediaView, { 
          displayAvatar: this.props.localVideoMuted, 
          isLoading: this.props.isLocalLoading, 
          mediaType: "local", 
          posterUrl: this.props.localPosterUrl, 
          srcMediaElement: this.props.localSrcMediaElement })));}, 




    renderMediaWait: function renderMediaWait() {
      var msg = mozL10n.get("call_progress_getting_media_description", 
      { clientShortname: mozL10n.get("clientShortname2") });
      var utils = loop.shared.utils;
      var isChrome = utils.isChrome(navigator.userAgent);
      var isFirefox = utils.isFirefox(navigator.userAgent);
      var isOpera = utils.isOpera(navigator.userAgent);
      var promptMediaMessageClasses = classNames({ 
        "prompt-media-message": true, 
        "chrome": isChrome, 
        "firefox": isFirefox, 
        "opera": isOpera, 
        "other": !isChrome && !isFirefox && !isOpera });

      return (
        React.createElement("div", { className: "prompt-media-message-wrapper" }, 
        React.createElement("p", { className: promptMediaMessageClasses }, 
        msg)));}, 





    render: function render() {
      var remoteStreamClasses = classNames({ 
        "remote": true, 
        "focus-stream": !this.props.displayScreenShare });


      var screenShareStreamClasses = classNames({ 
        "screen": true, 
        "focus-stream": this.props.displayScreenShare, 
        "screen-sharing-paused": this.props.screenSharingPaused });


      var mediaWrapperClasses = classNames({ 
        "media-wrapper": true, 
        "receiving-screen-share": this.props.displayScreenShare, 
        "showing-local-streams": this.props.localSrcMediaElement || 
        this.props.localPosterUrl, 
        "showing-media-wait": this.props.showMediaWait, 
        "showing-remote-streams": this.props.remoteSrcMediaElement || 
        this.props.remotePosterUrl || this.props.isRemoteLoading });


      return (
        React.createElement("div", { className: "media-layout" }, 
        React.createElement("div", { className: mediaWrapperClasses }, 
        React.createElement("span", { className: "self-view-hidden-message" }, 
        mozL10n.get("self_view_hidden_message")), 

        React.createElement("div", { className: remoteStreamClasses }, 
        React.createElement(MediaView, { 
          displayAvatar: !this.props.renderRemoteVideo, 
          isLoading: this.props.isRemoteLoading, 
          mediaType: "remote", 
          posterUrl: this.props.remotePosterUrl, 
          srcMediaElement: this.props.remoteSrcMediaElement }), 
        this.state.localMediaAboslutelyPositioned ? 
        this.renderLocalVideo() : null, 
        this.props.displayScreenShare ? null : this.props.children), 

        React.createElement("div", { className: screenShareStreamClasses }, 
        React.createElement(MediaView, { 
          cursorStore: this.props.cursorStore, 
          dispatcher: this.props.dispatcher, 
          displayAvatar: false, 
          isLoading: this.props.isScreenShareLoading, 
          mediaType: "screen-share", 
          posterUrl: this.props.screenSharePosterUrl, 
          screenSharingPaused: this.props.screenSharingPaused, 
          shareCursor: true, 
          srcMediaElement: this.props.screenShareMediaElement }), 
        this.props.displayScreenShare ? this.props.children : null), 

        React.createElement(loop.shared.views.chat.TextChatView, { 
          dispatcher: this.props.dispatcher, 
          showInitialContext: this.props.showInitialContext, 
          showTile: this.props.showTile }), 
        this.state.localMediaAboslutelyPositioned ? 
        null : this.renderLocalVideo(), 
        this.props.showMediaWait ? 
        this.renderMediaWait() : null)));} });






  var RemoteCursorView = React.createClass({ displayName: "RemoteCursorView", 
    statics: { 
      TRIGGERED_RESET_DELAY: 1000 }, 


    mixins: [
    React.addons.PureRenderMixin, 
    loop.store.StoreMixin("remoteCursorStore")], 


    propTypes: { 
      videoElementSize: React.PropTypes.object }, 


    getInitialState: function getInitialState() {
      return this.getStoreState();}, 


    componentWillMount: function componentWillMount() {
      if (!this.state.realVideoSize) {
        return;}


      this._calculateVideoLetterboxing();}, 


    componentWillReceiveProps: function componentWillReceiveProps(nextProps) {
      if (!this.state.realVideoSize) {
        return;}


      // In this case link generator or link clicker have resized their windows
      // so we need to recalculate the video letterboxing.
      this._calculateVideoLetterboxing(this.state.realVideoSize, 
      nextProps.videoElementSize);}, 


    componentWillUpdate: function componentWillUpdate(nextProps, nextState) {
      if (!this.state.realVideoSize || !nextState.realVideoSize) {
        return;}


      if (!this.state.videoLetterboxing) {
        // If this is the first time we receive the event, we must calculate the
        // video letterboxing.
        this._calculateVideoLetterboxing(nextState.realVideoSize);
        return;}


      if (nextState.realVideoSize.width !== this.state.realVideoSize.width || 
      nextState.realVideoSize.height !== this.state.realVideoSize.height) {
        // In this case link generator has resized his window so we need to
        // recalculate the video letterboxing.
        this._calculateVideoLetterboxing(nextState.realVideoSize);}}, 



    _calculateVideoLetterboxing: function _calculateVideoLetterboxing(realVideoSize, videoElementSize) {
      realVideoSize = realVideoSize || this.state.realVideoSize;
      videoElementSize = videoElementSize || this.props.videoElementSize;

      var clientWidth = videoElementSize.clientWidth;
      var clientHeight = videoElementSize.clientHeight;
      var clientRatio = clientWidth / clientHeight;
      var realVideoWidth = realVideoSize.width;
      var realVideoHeight = realVideoSize.height;
      var realVideoRatio = realVideoWidth / realVideoHeight;

      // If the video element ratio is "wider" than the video content, then the
      // full client height will be used and letterbox will be on the sides.
      // E.g., video element is 3:2 and stream is 1:1, so we end up with 2:2.
      var isWider = clientRatio > realVideoRatio;
      var streamVideoHeight = isWider ? clientHeight : clientWidth / realVideoRatio;
      var streamVideoWidth = isWider ? clientHeight * realVideoRatio : clientWidth;

      this.getStore().setStoreState({ 
        videoLetterboxing: { 
          left: (clientWidth - streamVideoWidth) / 2, 
          top: (clientHeight - streamVideoHeight) / 2 }, 

        streamVideoHeight: streamVideoHeight, 
        streamVideoWidth: streamVideoWidth });}, 



    calculateCursorPosition: function calculateCursorPosition() {
      // We need to calculate the cursor postion based on the current video
      // stream dimensions.
      var remoteCursorPosition = this.state.remoteCursorPosition;
      var ratioX = remoteCursorPosition.ratioX;
      var ratioY = remoteCursorPosition.ratioY;

      var cursorPositionX = this.state.streamVideoWidth * ratioX;
      var cursorPositionY = this.state.streamVideoHeight * ratioY;

      return { 
        left: cursorPositionX + this.state.videoLetterboxing.left, 
        top: cursorPositionY + this.state.videoLetterboxing.top };}, 



    resetClickState: function resetClickState() {
      this.getStore().setStoreState({ 
        remoteCursorClick: false });}, 



    render: function render() {
      if (!this.state.remoteCursorPosition || !this.state.videoLetterboxing) {
        return null;}


      var cx = classNames;
      var cursorClasses = cx({ 
        "remote-cursor-container": true, 
        "remote-cursor-clicked": this.state.remoteCursorClick });


      if (this.state.remoteCursorClick) {
        setTimeout(this.resetClickState, this.constructor.TRIGGERED_RESET_DELAY);}


      return (
        React.createElement("div", { className: cursorClasses, style: this.calculateCursorPosition() }, 
        React.createElement("div", { className: "remote-cursor" })));} });





  var AdsTileView = React.createClass({ displayName: "AdsTileView", 
    propTypes: { 
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired, 
      showTile: React.PropTypes.bool.isRequired }, 


    componentDidMount: function componentDidMount() {
      // Watch for messages from the waiting-tile iframe
      window.addEventListener("message", this.recordTileClick);}, 


    componentWillUnmount: function componentWillUnmount() {
      window.removeEventListener("message", this.recordTileClick);}, 


    recordTileClick: function recordTileClick(event) {
      if (event.data === "tile-click") {
        this.props.dispatcher.dispatch(new sharedActions.RecordClick({ 
          linkInfo: "Tiles iframe click" }));}}, 




    recordTilesSupport: function recordTilesSupport() {
      this.props.dispatcher.dispatch(new sharedActions.RecordClick({ 
        linkInfo: "Tiles support link click" }));}, 



    render: function render() {
      if (!this.props.showTile) {
        window.removeEventListener("message", this.recordTileClick);
        return null;}


      return (
        React.createElement("div", { className: "ads-tile" }, 
        React.createElement("div", { className: "ads-wrapper" }, 
        React.createElement("p", null, mozL10n.get("rooms_read_while_wait_offer2")), 
        React.createElement("a", { href: loop.config.tilesSupportUrl, 
          onClick: this.recordTilesSupport, 
          rel: "noreferrer", 
          target: "_blank" }, 
        React.createElement("i", { className: "room-waiting-help" })), 

        React.createElement("iframe", { className: "room-waiting-tile", src: loop.config.tilesIframeUrl }))));} });






  return { 
    AdsTileView: AdsTileView, 
    AudioMuteButton: AudioMuteButton, 
    AvatarView: AvatarView, 
    Button: Button, 
    ButtonGroup: ButtonGroup, 
    Checkbox: Checkbox, 
    ContextUrlLink: ContextUrlLink, 
    ContextUrlView: ContextUrlView, 
    ConversationToolbar: ConversationToolbar, 
    HangUpControlButton: HangUpControlButton, 
    MediaControlButton: MediaControlButton, 
    MediaLayoutView: MediaLayoutView, 
    MediaView: MediaView, 
    LoadingView: LoadingView, 
    RemoteCursorView: RemoteCursorView, 
    VideoMuteButton: VideoMuteButton };}(

_, navigator.mozL10n || document.mozL10n);
