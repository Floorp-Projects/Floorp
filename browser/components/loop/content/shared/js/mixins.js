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
   * Sets a new root object. This is useful for testing native DOM events so we
   * can fake them.
   *
   * @param {Object}
   */
  function setRootObject(obj) {
    console.info("loop.shared.mixins: rootObject set to " + obj);
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

  return {
    AudioMixin: AudioMixin,
    setRootObject: setRootObject,
    DropdownMenuMixin: DropdownMenuMixin,
    DocumentVisibilityMixin: DocumentVisibilityMixin,
    DocumentLocationMixin: DocumentLocationMixin,
    DocumentTitleMixin: DocumentTitleMixin,
    UrlHashChangeMixin: UrlHashChangeMixin
  };
})();
