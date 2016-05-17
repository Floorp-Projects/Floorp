"use strict"; /* This Source Code Form is subject to the terms of the Mozilla Public
               * License, v. 2.0. If a copy of the MPL was not distributed with this file,
               * You can obtain one at http://mozilla.org/MPL/2.0/. */
// This mixin should be deprecated and a new solution implemented for
// processing menus and taking care of menu positioning globally. This
// new implementation should ensure all menus are positioned using the
// same method of positioning

var loop = loop || {};
loop.shared = loop.shared || {};
loop.shared.mixins = function () {
  "use strict";

  /**
   * Root object, by default set to window.
   * @type {DOMWindow|Object}
   */
  var rootObject = window;

  /**
   * Sets a new root object.  This is useful for testing native DOM events so we
   * can fake them. In beforeEach(), loop.shared.mixins.setRootObject is used to
   * substitute a fake window, and in afterEach(), the real window object is
   * replaced.
   *
   * @param {Object}
   */
  function setRootObject(obj) {
    rootObject = obj;}


  /**
   * window.location mixin. Handles changes in the call url.
   * Forces a reload of the page to ensure proper state of the webapp
   *
   * @type {Object}
   */
  var UrlHashChangeMixin = { 
    componentDidMount: function componentDidMount() {
      rootObject.addEventListener("hashchange", this.onUrlHashChange, false);}, 


    componentWillUnmount: function componentWillUnmount() {
      rootObject.removeEventListener("hashchange", this.onUrlHashChange, false);} };



  /**
   * Document location mixin.
   *
   * @type {Object}
   */
  var DocumentLocationMixin = { 
    locationReload: function locationReload() {
      rootObject.location.reload();} };



  /**
   * Document title mixin.
   *
   * @type {Object}
   */
  var DocumentTitleMixin = { 
    setTitle: function setTitle(newTitle) {
      rootObject.document.title = newTitle;} };



  /**
   * Window close mixin, for more testable closing of windows.  Instead of
   * calling window.close() directly, use this mixin and call
   * this.closeWindow from your component.
   *
   * @type {Object}
   *
   * @see setRootObject for info on how to unit test code that uses this mixin
   */
  var WindowCloseMixin = { 
    closeWindow: function closeWindow() {
      rootObject.close();} };



  /**
   * Dropdown menu mixin.
   *
   * @param {Sring} [boundingBoxSelector] Selector that points to an element that
   *                                      defines the constraints this dropdown
   *                                      is shown within. If not provided,
   *                                      `document.body` is assumed to be the
   *                                      constraining element.
   * @type {Object}
   */
  var DropdownMenuMixin = function DropdownMenuMixin(boundingBoxSelector) {
    return { 
      get documentBody() {
        return rootObject.document.body;}, 


      getInitialState: function getInitialState() {
        return { 
          showMenu: false };}, 



      /*
       * Event listener callback in charge of closing panels when the users
       * clicks on something that is not a dropdown trigger button or menu item.
       */
      _onBodyClick: function _onBodyClick(event) {
        var menuButton = this.refs["menu-button"];

        if (this.refs.anchor) {
          menuButton = this.refs.anchor;}


        /*
         * XXX Because the mixin is inherited by multiple components there are
         * multiple such listeners at one time. This means that this.refs is not
         * relevant when you click inside component A but the listener that is
         * running is in component B and does not recognise event.target. This
         * should be refactored to only be attached once to the document and use
         * classList instead of refs.
         */
        if (event.target.classList.contains("dropdown-menu-item") || 
        event.target.classList.contains("dropdown-menu-button")) {
          return;}


        if (event.target !== menuButton) {
          this.setState({ showMenu: false });}}, 



      _correctMenuPosition: function _correctMenuPosition() {
        var menu = this.refs.menu;
        if (!menu) {
          return;}

        if (menu.style.maxWidth) {
          menu.style.maxWidth = "none";}

        if (menu.style.maxHeight) {
          menu.style.maxHeight = "none";}


        // Correct the position of the menu only if necessary.
        var x, y, boundingBox, boundingRect;
        // Amount of pixels that the dropdown needs to stay away from the edges of
        // the page body.
        var boundOffset = 4;
        var menuNodeRect = menu.getBoundingClientRect();
        // If the menu dimensions are constrained to a bounding element, instead of
        // the document body, find that element.
        if (boundingBoxSelector) {
          boundingBox = this.documentBody.querySelector(boundingBoxSelector);
          if (boundingBox) {
            boundingRect = boundingBox.getBoundingClientRect();}}


        if (!boundingRect) {
          boundingRect = { 
            height: this.documentBody.offsetHeight, 
            left: 0, 
            top: 0, 
            width: this.documentBody.offsetWidth };}


        // Make sure the menu position will be a certain fixed amount of pixels away
        // from the border of the bounding box.
        boundingRect.width -= boundOffset;
        boundingRect.height -= boundOffset;

        x = menuNodeRect.left;
        y = menuNodeRect.top;

        // If there's an anchor present, position it relative to it first.
        var anchor = this.refs.anchor && ReactDOM.findDOMNode(this.refs.anchor);
        if (anchor) {
          // XXXmikedeboer: at the moment we only support positioning centered above
          //                anchor node. Please add more modes as necessary.
          var anchorNodeRect = anchor.getBoundingClientRect();
          // Because we're _correcting_ the position of the dropdown, we assume that
          // the node is positioned absolute at 0,0 coordinates (top left).
          x = Math.floor(anchorNodeRect.left - menuNodeRect.width / 2 + anchorNodeRect.width / 2);
          y = Math.floor(anchorNodeRect.top - menuNodeRect.height - anchorNodeRect.height);}


        var overflowX = false;
        var overflowY = false;
        // Check the horizontal overflow.
        if (x + menuNodeRect.width > boundingRect.width) {
          // Anchor positioning is already relative, so don't subtract it again.
          x = Math.floor(boundingRect.width - ((anchor ? 0 : x) + menuNodeRect.width));
          overflowX = true;}

        // Check the vertical overflow.
        if (y + menuNodeRect.height > boundingRect.height) {
          // Anchor positioning is already relative, so don't subtract it again.
          y = Math.floor(boundingRect.height - ((anchor ? 0 : y) + menuNodeRect.height));
          overflowY = true;}


        if (anchor || overflowX) {
          // Set the maximum dimensions that the menu DOMNode may grow to. The
          // content overflow style should be defined in CSS.
          // Since we don't care much about horizontal overflow currently, this
          // doesn't really do much for now.
          if (menuNodeRect.width > boundingRect.width) {
            menu.classList.add("overflow");
            menu.style.maxWidth = boundingRect.width + "px";}

          menu.style.marginLeft = x + "px";} else 
        if (!menu.style.marginLeft) {
          menu.style.marginLeft = "auto";}


        if (anchor || overflowY) {
          if (menuNodeRect.height > boundingRect.height + y) {
            menu.classList.add("overflow");
            // Set the maximum dimensions that the menu DOMNode may grow to. The
            // content overflow style should be defined in CSS.
            menu.style.maxHeight = boundingRect.height + y + "px";
            // Since we just adjusted the max-height of the menu - thus its actual
            // height as well - we need to adjust its vertical offset with the same
            // amount.
            y += menuNodeRect.height - (boundingRect.height + y);}

          menu.style.marginTop = y + "px";} else 
        if (!menu.style.marginLeft) {
          menu.style.marginTop = "auto";}


        // Added call to _repositionMenu() if it exists, to allow a component to
        // add specific repositioning to a menu.
        if (this._repositionMenu) {
          this._repositionMenu();}

        menu.style.visibility = "visible";}, 


      componentDidMount: function componentDidMount() {
        this.documentBody.addEventListener("click", this._onBodyClick);
        rootObject.addEventListener("blur", this.hideDropdownMenu);}, 


      componentWillUnmount: function componentWillUnmount() {
        this.documentBody.removeEventListener("click", this._onBodyClick);
        rootObject.removeEventListener("blur", this.hideDropdownMenu);}, 


      showDropdownMenu: function showDropdownMenu() {
        this.setState({ showMenu: true }, this._correctMenuPosition);}, 


      hideDropdownMenu: function hideDropdownMenu() {
        this.setState({ showMenu: false }, function () {
          var menu = this.refs.menu && ReactDOM.findDOMNode(this.refs.menu);
          if (menu) {
            menu.style.visibility = "hidden";}});}, 




      toggleDropdownMenu: function toggleDropdownMenu() {
        this[this.state.showMenu ? "hideDropdownMenu" : "showDropdownMenu"]();} };};




  /**
   * Document visibility mixin. Allows defining the following hooks for when the
   * document visibility status changes:
   *
   * - {Function} onDocumentVisible For when the document becomes visible.
   * - {Function} onDocumentHidden  For when the document becomes hidden.
   *
   * @type {Object}
   */
  var DocumentVisibilityMixin = { 
    _onDocumentVisibilityChanged: function _onDocumentVisibilityChanged(event) {
      if (!this.isMounted()) {
        return;}


      var hidden = event.target.hidden;
      if (hidden && typeof this.onDocumentHidden === "function") {
        this.onDocumentHidden();}

      if (!hidden && typeof this.onDocumentVisible === "function") {
        this.onDocumentVisible();}}, 



    componentDidMount: function componentDidMount() {
      rootObject.document.addEventListener(
      "visibilitychange", this._onDocumentVisibilityChanged);
      // Assume that the consumer components is only mounted when the document
      // has become visible.
      this._onDocumentVisibilityChanged({ target: rootObject.document });}, 


    componentWillUnmount: function componentWillUnmount() {
      rootObject.document.removeEventListener(
      "visibilitychange", this._onDocumentVisibilityChanged);} };



  /**
   * Media setup mixin. Provides a common location for settings for the media
   * elements.
   */
  var MediaSetupMixin = { 
    /**
     * Returns the default configuration for publishing media on the sdk.
     *
     * @param {Object} options An options object containing:
     * - publishVideo A boolean set to true to publish video when the stream is initiated.
     */
    getDefaultPublisherConfig: function getDefaultPublisherConfig(options) {
      options = options || {};
      if (!("publishVideo" in options)) {
        throw new Error("missing option publishVideo");}


      // height set to 100%" to fix video layout on Google Chrome
      // @see https://bugzilla.mozilla.org/show_bug.cgi?id=1020445
      return { 
        insertMode: "append", 
        fitMode: "contain", 
        width: "100%", 
        height: "100%", 
        publishVideo: options.publishVideo, 
        showControls: false };} };




  /**
   * Audio mixin. Allows playing a single audio file and ensuring it
   * is stopped when the component is unmounted.
   */
  var AudioMixin = { 
    audio: null, 
    _audioRequest: null, 

    _isLoopDesktop: function _isLoopDesktop() {
      var isUIShowcase = !!(rootObject.document.querySelector && 
      rootObject.document.querySelector("#main > .showcase"));
      return loop.shared.utils.isDesktop() || isUIShowcase;}, 


    /**
     * Internal function that determines if we can play an audio fragment at this
     * time. If the user disabled all notifications (e.g. muted), the returned
     * Promise will resolve with FALSE.
     *
     * @return {Promise}
     */
    _canPlay: function _canPlay() {
      return new Promise(function (resolve) {
        if (!this._isLoopDesktop()) {
          resolve(true);
          return;}


        loop.request("GetDoNotDisturb").then(function (mayNotDisturb) {
          resolve(!mayNotDisturb);});}.

      bind(this));}, 


    /**
     * Starts playing an audio file, stopping any audio that is already in progress.
     *
     * @param {String} name    The filename to play (excluding the extension).
     * @param {Object} options A list of options for the sound:
     *                         - {Boolean} loop Whether or not to loop the sound.
     */
    play: function play(name, options) {
      this._canPlay().then(function (canPlay) {
        if (!canPlay) {
          return;}


        options = options || {};
        options.loop = options.loop || false;

        this._ensureAudioStopped();
        this._getAudioBlob(name, function (error, blob) {
          if (error) {
            console.error(error);
            return;}

          if (!blob) {
            return;}


          var url = URL.createObjectURL(blob);
          this.audio = new Audio(url);
          this.audio.loop = options.loop;
          this.audio.play();}.
        bind(this));}.
      bind(this));}, 


    _getAudioBlob: function _getAudioBlob(name, callback) {
      this._canPlay().then(function (canPlay) {
        if (!canPlay) {
          callback();
          return;}


        if (this._isLoopDesktop()) {
          loop.request("GetAudioBlob", name).then(function (result) {
            if (result && result.isError) {
              callback(result);
              return;}

            callback(null, result);});

          return;}


        var url = "shared/sounds/" + name + ".ogg";
        this._audioRequest = new XMLHttpRequest();
        this._audioRequest.open("GET", url, true);
        this._audioRequest.responseType = "arraybuffer";
        this._audioRequest.onload = function () {
          var request = this._audioRequest;
          var error;
          if (request.status < 200 || request.status >= 300) {
            error = new Error(request.status + " " + request.statusText);
            callback(error);
            return;}


          var type = request.getResponseHeader("Content-Type");
          var blob = new Blob([request.response], { type: type });
          callback(null, blob);}.
        bind(this);

        this._audioRequest.send(null);}.
      bind(this));}, 


    /**
     * Ensures audio is stopped playing, and removes the object from memory.
     */
    _ensureAudioStopped: function _ensureAudioStopped() {
      if (this._audioRequest) {
        this._audioRequest.abort();
        delete this._audioRequest;}


      if (this.audio) {
        this.audio.pause();
        this.audio.removeAttribute("src");
        delete this.audio;}}, 



    /**
     * Ensures audio is stopped when the component is unmounted.
     */
    componentWillUnmount: function componentWillUnmount() {
      this._ensureAudioStopped();} };



  /**
   * A mixin especially for rooms. This plays the right sound according to
   * the state changes. Requires AudioMixin to also be used.
   */
  var RoomsAudioMixin = { 
    mixins: [AudioMixin], 

    componentWillUpdate: function componentWillUpdate(nextProps, nextState) {
      var ROOM_STATES = loop.store.ROOM_STATES;

      function isConnectedToRoom(state) {
        return state === ROOM_STATES.HAS_PARTICIPANTS || 
        state === ROOM_STATES.SESSION_CONNECTED;}


      function notConnectedToRoom(state) {
        // Failed and full are states that the user is not
        // really connected to the room, but we don't want to
        // catch those here, as they get their own sounds.
        return state === ROOM_STATES.INIT || 
        state === ROOM_STATES.GATHER || 
        state === ROOM_STATES.READY || 
        state === ROOM_STATES.JOINED || 
        state === ROOM_STATES.ENDED;}


      // Joining the room.
      if (notConnectedToRoom(this.state.roomState) && 
      isConnectedToRoom(nextState.roomState)) {
        this.play("room-joined");}


      // Other people coming and leaving.
      if (this.state.roomState === ROOM_STATES.SESSION_CONNECTED && 
      nextState.roomState === ROOM_STATES.HAS_PARTICIPANTS) {
        this.play("room-joined-in");}


      if (this.state.roomState === ROOM_STATES.HAS_PARTICIPANTS && 
      nextState.roomState === ROOM_STATES.SESSION_CONNECTED) {
        this.play("room-left");}


      // Leaving the room - same sound as if a participant leaves
      if (isConnectedToRoom(this.state.roomState) && 
      notConnectedToRoom(nextState.roomState)) {
        this.play("room-left");}


      // Room failures
      if (nextState.roomState === ROOM_STATES.FAILED || 
      nextState.roomState === ROOM_STATES.FULL) {
        this.play("failure");}} };




  return { 
    AudioMixin: AudioMixin, 
    RoomsAudioMixin: RoomsAudioMixin, 
    setRootObject: setRootObject, 
    DropdownMenuMixin: DropdownMenuMixin, 
    DocumentVisibilityMixin: DocumentVisibilityMixin, 
    DocumentLocationMixin: DocumentLocationMixin, 
    DocumentTitleMixin: DocumentTitleMixin, 
    MediaSetupMixin: MediaSetupMixin, 
    UrlHashChangeMixin: UrlHashChangeMixin, 
    WindowCloseMixin: WindowCloseMixin };}();
