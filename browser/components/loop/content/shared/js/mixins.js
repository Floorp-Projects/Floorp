/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.shared = loop.shared || {};
loop.shared.mixins = (function() {
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
    console.log("loop.shared.mixins: rootObject set to " + obj);
    rootObject = obj;
  }

  /**
   * window.location mixin. Handles changes in the call url.
   * Forces a reload of the page to ensure proper state of the webapp
   *
   * @type {Object}
   */
  var UrlHashChangeMixin = {
    componentDidMount: function() {
      rootObject.addEventListener("hashchange", this.onUrlHashChange, false);
    },

    componentWillUnmount: function() {
      rootObject.removeEventListener("hashchange", this.onUrlHashChange, false);
    }
  };

  /**
   * Document location mixin.
   *
   * @type {Object}
   */
  var DocumentLocationMixin = {
    locationReload: function() {
      rootObject.location.reload();
    }
  };

  /**
   * Document title mixin.
   *
   * @type {Object}
   */
  var DocumentTitleMixin = {
    setTitle: function(newTitle) {
      rootObject.document.title = newTitle;
    }
  };

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
    closeWindow: function() {
      rootObject.close();
    }
  };

  /**
   * Dropdown menu mixin.
   * @type {Object}
   */
  var DropdownMenuMixin = {
    get documentBody() {
      return rootObject.document.body;
    },

    getInitialState: function() {
      return {showMenu: false};
    },

    _onBodyClick: function() {
      this.setState({showMenu: false});
    },

    componentDidMount: function() {
      this.documentBody.addEventListener("click", this._onBodyClick);
      this.documentBody.addEventListener("blur", this.hideDropdownMenu);
    },

    componentWillUnmount: function() {
      this.documentBody.removeEventListener("click", this._onBodyClick);
      this.documentBody.removeEventListener("blur", this.hideDropdownMenu);
    },

    showDropdownMenu: function() {
      this.setState({showMenu: true});
    },

    hideDropdownMenu: function() {
      this.setState({showMenu: false});
    },

    toggleDropdownMenu: function() {
      this.setState({showMenu: !this.state.showMenu});
    },
  };

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
    _onDocumentVisibilityChanged: function(event) {
      var hidden = event.target.hidden;
      if (hidden && typeof this.onDocumentHidden === "function") {
        this.onDocumentHidden();
      }
      if (!hidden && typeof this.onDocumentVisible === "function") {
        this.onDocumentVisible();
      }
    },

    componentDidMount: function() {
      rootObject.document.addEventListener(
        "visibilitychange", this._onDocumentVisibilityChanged);
    },

    componentWillUnmount: function() {
      rootObject.document.removeEventListener(
        "visibilitychange", this._onDocumentVisibilityChanged);
    }
  };

  /**
   * Media setup mixin. Provides a common location for settings for the media
   * elements and handling updates of the media containers.
   */
  var MediaSetupMixin = {
    componentDidMount: function() {
      rootObject.addEventListener('orientationchange', this.updateVideoContainer);
      rootObject.addEventListener('resize', this.updateVideoContainer);
    },

    componentWillUnmount: function() {
      rootObject.removeEventListener('orientationchange', this.updateVideoContainer);
      rootObject.removeEventListener('resize', this.updateVideoContainer);
    },

    /**
     * Used to update the video container whenever the orientation or size of the
     * display area changes.
     */
    updateVideoContainer: function() {
      var localStreamParent = this._getElement('.local .OT_publisher');
      var remoteStreamParent = this._getElement('.remote .OT_subscriber');
      if (localStreamParent) {
        localStreamParent.style.width = "100%";
      }
      if (remoteStreamParent) {
        remoteStreamParent.style.height = "100%";
      }
    },

    /**
     * Returns the default configuration for publishing media on the sdk.
     *
     * @param {Object} options An options object containing:
     * - publishVideo A boolean set to true to publish video when the stream is initiated.
     */
    getDefaultPublisherConfig: function(options) {
      options = options || {};
      if (!"publishVideo" in options) {
        throw new Error("missing option publishVideo");
      }

      // height set to 100%" to fix video layout on Google Chrome
      // @see https://bugzilla.mozilla.org/show_bug.cgi?id=1020445
      return {
        insertMode: "append",
        width: "100%",
        height: "100%",
        publishVideo: options.publishVideo,
        style: {
          audioLevelDisplayMode: "off",
          buttonDisplayMode: "off",
          nameDisplayMode: "off",
          videoDisabledDisplayMode: "off"
        }
      };
    },

    /**
     * Returns either the required DOMNode
     *
     * @param {String} className The name of the class to get the element for.
     */
    _getElement: function(className) {
      return this.getDOMNode().querySelector(className);
    }
  };

  /**
   * Audio mixin. Allows playing a single audio file and ensuring it
   * is stopped when the component is unmounted.
   */
  var AudioMixin = {
    audio: null,
    _audioRequest: null,

    _isLoopDesktop: function() {
      return rootObject.navigator &&
             typeof rootObject.navigator.mozLoop === "object";
    },

    /**
     * Starts playing an audio file, stopping any audio that is already in progress.
     *
     * @param {String} name The filename to play (excluding the extension).
     */
    play: function(name, options) {
      if (this._isLoopDesktop() && rootObject.navigator.mozLoop.doNotDisturb) {
        return;
      }

      options = options || {};
      options.loop = options.loop || false;

      this._ensureAudioStopped();
      this._getAudioBlob(name, function(error, blob) {
        if (error) {
          console.error(error);
          return;
        }

        var url = URL.createObjectURL(blob);
        this.audio = new Audio(url);
        this.audio.loop = options.loop;
        this.audio.play();
      }.bind(this));
    },

    _getAudioBlob: function(name, callback) {
      if (this._isLoopDesktop()) {
        rootObject.navigator.mozLoop.getAudioBlob(name, callback);
        return;
      }

      var url = "shared/sounds/" + name + ".ogg";
      this._audioRequest = new XMLHttpRequest();
      this._audioRequest.open("GET", url, true);
      this._audioRequest.responseType = "arraybuffer";
      this._audioRequest.onload = function() {
        var request = this._audioRequest;
        var error;
        if (request.status < 200 || request.status >= 300) {
          error = new Error(request.status + " " + request.statusText);
          callback(error);
          return;
        }

        var type = request.getResponseHeader("Content-Type");
        var blob = new Blob([request.response], {type: type});
        callback(null, blob);
      }.bind(this);

      this._audioRequest.send(null);
    },

    /**
     * Ensures audio is stopped playing, and removes the object from memory.
     */
    _ensureAudioStopped: function() {
      if (this._audioRequest) {
        this._audioRequest.abort();
        delete this._audioRequest;
      }

      if (this.audio) {
        this.audio.pause();
        this.audio.removeAttribute("src");
        delete this.audio;
      }
    },

    /**
     * Ensures audio is stopped when the component is unmounted.
     */
    componentWillUnmount: function() {
      this._ensureAudioStopped();
    }
  };

  /**
   * A mixin especially for rooms. This plays the right sound according to
   * the state changes. Requires AudioMixin to also be used.
   */
  var RoomsAudioMixin = {
    mixins: [AudioMixin],

    componentWillUpdate: function(nextProps, nextState) {
      var ROOM_STATES = loop.store.ROOM_STATES;

      function isConnectedToRoom(state) {
        return state === ROOM_STATES.HAS_PARTICIPANTS ||
               state === ROOM_STATES.SESSION_CONNECTED;
      }

      function notConnectedToRoom(state) {
        // Failed and full are states that the user is not
        // really connected to the room, but we don't want to
        // catch those here, as they get their own sounds.
        return state === ROOM_STATES.INIT ||
               state === ROOM_STATES.GATHER ||
               state === ROOM_STATES.READY ||
               state === ROOM_STATES.JOINED ||
               state === ROOM_STATES.ENDED;
      }

      // Joining the room.
      if (notConnectedToRoom(this.state.roomState) &&
          isConnectedToRoom(nextState.roomState)) {
        this.play("room-joined");
      }

      // Other people coming and leaving.
      if (this.state.roomState === ROOM_STATES.SESSION_CONNECTED &&
          nextState.roomState === ROOM_STATES.HAS_PARTICIPANTS) {
        this.play("room-joined-in");
      }

      if (this.state.roomState === ROOM_STATES.HAS_PARTICIPANTS &&
          nextState.roomState === ROOM_STATES.SESSION_CONNECTED) {
        this.play("room-left");
      }

      // Leaving the room - same sound as if a participant leaves
      if (isConnectedToRoom(this.state.roomState) &&
          notConnectedToRoom(nextState.roomState)) {
        this.play("room-left");
      }

      // Room failures
      if (nextState.roomState === ROOM_STATES.FAILED ||
          nextState.roomState === ROOM_STATES.FULL) {
        this.play("failure");
      }
    }
  };

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
    WindowCloseMixin: WindowCloseMixin
  };
})();
