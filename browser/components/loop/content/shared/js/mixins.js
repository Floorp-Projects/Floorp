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

      var menu = this.refs.menu;
      if (!menu) {
        return;
      }

      // Correct the position of the menu if necessary.
      var menuNode = menu.getDOMNode();
      var menuNodeRect = menuNode.getBoundingClientRect();
      var bodyRect = {
        height: this.documentBody.offsetHeight,
        width: this.documentBody.offsetWidth
      };

      // First we check the vertical overflow.
      var y = menuNodeRect.top + menuNodeRect.height;
      if (y >= bodyRect.height) {
        menuNode.style.marginTop = bodyRect.height - y + "px";
      }

      // Then we check the horizontal overflow.
      var x = menuNodeRect.left + menuNodeRect.width;
      if (x >= bodyRect.width) {
        menuNode.style.marginLeft = bodyRect.width - x + "px";
      }
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
    _videoDimensionsCache: {
      local: {},
      remote: {}
    },

    componentDidMount: function() {
      rootObject.addEventListener("orientationchange", this.updateVideoContainer);
      rootObject.addEventListener("resize", this.updateVideoContainer);
    },

    componentWillUnmount: function() {
      rootObject.removeEventListener("orientationchange", this.updateVideoContainer);
      rootObject.removeEventListener("resize", this.updateVideoContainer);
    },

    /**
     * Whenever the dimensions change of a video stream, this function is called
     * by `updateVideoDimensions` to store the new values and notifies the callee
     * if the dimensions changed compared to the currently stored values.
     *
     * @param  {String} which         Type of video stream. May be 'local' or 'remote'
     * @param  {Object} newDimensions Object containing 'width' and 'height' properties
     * @return {Boolean}              `true` when the dimensions have changed,
     *                                `false` if not
     */
    _updateDimensionsCache: function(which, newDimensions) {
      var cache = this._videoDimensionsCache[which];
      var cacheKeys = Object.keys(cache);
      var changed = false;
      Object.keys(newDimensions).forEach(function(videoType) {
        if (cacheKeys.indexOf(videoType) === -1) {
          cache[videoType] = newDimensions[videoType];
          cache[videoType].aspectRatio = this.getAspectRatio(cache[videoType]);
          changed = true;
          return;
        }
        if (cache[videoType].width !== newDimensions[videoType].width) {
          cache[videoType].width = newDimensions[videoType].width;
          changed = true;
        }
        if (cache[videoType].height !== newDimensions[videoType].height) {
          cache[videoType].height = newDimensions[videoType].height;
          changed = true;
        }
        if (changed) {
          cache[videoType].aspectRatio = this.getAspectRatio(cache[videoType]);
        }
      }, this);
      return changed;
    },

    /**
     * Whenever the dimensions change of a video stream, this function is called
     * to process these changes and possibly trigger an update to the video
     * container elements.
     *
     * @param  {Object} localVideoDimensions  Object containing 'width' and 'height'
     *                                        properties grouped by stream name
     * @param  {Object} remoteVideoDimensions Object containing 'width' and 'height'
     *                                        properties grouped by stream name
     */
    updateVideoDimensions: function(localVideoDimensions, remoteVideoDimensions) {
      var localChanged = this._updateDimensionsCache("local", localVideoDimensions);
      var remoteChanged = this._updateDimensionsCache("remote", remoteVideoDimensions);
      if (localChanged || remoteChanged) {
        this.updateVideoContainer();
      }
    },

    /**
     * Get the aspect ratio of a width/ height pair, which should be the dimensions
     * of a stream. The returned object is an aspect ratio indexed by 1; the leading
     * size has a value smaller than 1 and the slave size has a value of 1.
     * this is exactly the same as notations like 4:3 and 16:9, which are merely
     * human-readable forms of their fractional counterparts. 4:3 === 1:0.75 and
     * 16:9 === 1:0.5625.
     * So we're using the aspect ratios in their original form, because that's
     * easier to do calculus with.
     *
     * Example:
     * A stream with dimensions `{ width: 640, height: 480 }` yields an indexed
     * aspect ratio of `{ width: 1, height: 0.75 }`. This means that the 'height'
     * will determine the value of 'width' when the stream is stretched or shrunk
     * to fit inside its container element at the maximum size.
     *
     * @param  {Object} dimensions Object containing 'width' and 'height' properties
     * @return {Object}            Contains the indexed aspect ratio for 'width'
     *                             and 'height' assigned to the corresponding
     *                             properties.
     */
    getAspectRatio: function(dimensions) {
      if (dimensions.width === dimensions.height) {
        return {width: 1, height: 1};
      }
      var denominator = Math.max(dimensions.width, dimensions.height);
      return {
        width: dimensions.width / denominator,
        height: dimensions.height / denominator
      };
    },

    /**
     * Retrieve the dimensions of the active remote video stream. This assumes
     * that if screens are being shared, the remote camera stream is hidden.
     * Example output:
     *   {
     *     width: 680,
     *     height: 480,
     *     streamWidth: 640,
     *     streamHeight: 480,
     *     offsetX: 20,
     *     offsetY: 0
     *   }
     *
     * Note: This expects a class on the element that has the name "remote" or the
     *       same name as the possible video types (currently only "screen").
     * Note: Once we support multiple remote video streams, this function will
     *       need to be updated.
     *
     * @param {string} videoType The video type according to the sdk, e.g. "camera" or
     *                           "screen".
     * @return {Object} contains the remote stream dimension properties of its
     *                  container node, the stream itself and offset of the stream
     *                  relative to its container node in pixels.
     */
    getRemoteVideoDimensions: function(videoType) {
      var remoteVideoDimensions;

      if (videoType in this._videoDimensionsCache.remote) {
        var node = this._getElement("." + (videoType === "camera" ? "remote" : videoType));
        var width = node.offsetWidth;
        // If the width > 0 then we record its real size by taking its aspect
        // ratio in account. Due to the 'contain' fit-mode, the stream will be
        // centered inside the video element.
        // We'll need to deal with more than one remote video stream whenever
        // that becomes something we need to support.
        if (width) {
          remoteVideoDimensions = {
            width: width,
            height: node.offsetHeight
          };

          var ratio = this._videoDimensionsCache.remote[videoType].aspectRatio;
          // Leading axis is the side that has the smallest ratio.
          var leadingAxis = Math.min(ratio.width, ratio.height) === ratio.width ?
            "width" : "height";
          var slaveAxis = leadingAxis === "height" ? "width" : "height";

          // We need to work out if the leading axis of the video is full, by
          // calculating the expected length of the leading axis based on the
          // length of the slave axis and aspect ratio.
          var leadingAxisFull = remoteVideoDimensions[slaveAxis] * ratio[leadingAxis] >
            remoteVideoDimensions[leadingAxis];

          if (leadingAxisFull) {
            // If the leading axis is "full" then we need to adjust the slave axis.
            var slaveAxisSize = remoteVideoDimensions[leadingAxis] / ratio[leadingAxis];

            remoteVideoDimensions.streamWidth = leadingAxis === "width" ?
              remoteVideoDimensions.width : slaveAxisSize;
            remoteVideoDimensions.streamHeight = leadingAxis === "height" ?
              remoteVideoDimensions.height: slaveAxisSize;
          } else {
            // If the leading axis is not "full" then we need to adjust it, based
            // on the length of the leading axis.
            var leadingAxisSize = remoteVideoDimensions[slaveAxis] * ratio[leadingAxis];

            remoteVideoDimensions.streamWidth = leadingAxis === "height" ?
              remoteVideoDimensions.width : leadingAxisSize;
            remoteVideoDimensions.streamHeight = leadingAxis === "width" ?
              remoteVideoDimensions.height: leadingAxisSize;
          }
        }
      }

      // Supply some sensible defaults for the remoteVideoDimensions if no remote
      // stream is connected (yet).
      if (!remoteVideoDimensions) {
        var node = this._getElement(".remote");
        var width = node.offsetWidth;
        var height = node.offsetHeight;
        remoteVideoDimensions = {
          width: width,
          height: height,
          streamWidth: width,
          streamHeight: height
        };
      }

      // Calculate the size of each individual letter- or pillarbox for convenience.
      remoteVideoDimensions.offsetX = remoteVideoDimensions.width -
        remoteVideoDimensions.streamWidth;
      if (remoteVideoDimensions.offsetX > 0) {
        remoteVideoDimensions.offsetX /= 2;
      }
      remoteVideoDimensions.offsetY = remoteVideoDimensions.height -
        remoteVideoDimensions.streamHeight;
      if (remoteVideoDimensions.offsetY > 0) {
        remoteVideoDimensions.offsetY /= 2;
      }

      return remoteVideoDimensions;
    },

    /**
     * Used to update the video container whenever the orientation or size of the
     * display area changes.
     *
     * Buffer the calls to this function to make sure we don't overflow the stack
     * with update calls when many 'resize' event are fired, to prevent blocking
     * the event loop.
     */
    updateVideoContainer: function() {
      if (this._bufferedUpdateVideo) {
        rootObject.clearTimeout(this._bufferedUpdateVideo);
        this._bufferedUpdateVideo = null;
      }

      this._bufferedUpdateVideo = rootObject.setTimeout(function() {
        this._bufferedUpdateVideo = null;
        var localStreamParent = this._getElement(".local .OT_publisher");
        var remoteStreamParent = this._getElement(".remote .OT_subscriber");
        var screenShareStreamParent = this._getElement('.screen .OT_subscriber');
        if (localStreamParent) {
          localStreamParent.style.width = "100%";
        }
        if (remoteStreamParent) {
          remoteStreamParent.style.height = "100%";
        }
        if (screenShareStreamParent) {
          screenShareStreamParent.style.height = "100%";
        }

        // Update the position and dimensions of the containers of local and remote
        // video streams, if necessary. The consumer of this mixin should implement
        // the actual updating mechanism.
        Object.keys(this._videoDimensionsCache.local).forEach(function(videoType) {
          var ratio = this._videoDimensionsCache.local[videoType].aspectRatio;
          if (videoType == "camera" && this.updateLocalCameraPosition) {
            this.updateLocalCameraPosition(ratio);
          }
        }, this);
        Object.keys(this._videoDimensionsCache.remote).forEach(function(videoType) {
          var ratio = this._videoDimensionsCache.remote[videoType].aspectRatio;
          if (videoType == "camera" && this.updateRemoteCameraPosition) {
            this.updateRemoteCameraPosition(ratio);
          }
        }, this);
      }.bind(this), 0);
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
        fitMode: "contain",
        width: "100%",
        height: "100%",
        publishVideo: options.publishVideo,
        showControls: false,
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
