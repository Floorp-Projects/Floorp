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
    propTypes: {
      onUrlHashChange: React.PropTypes.func.isRequired
    },

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

    _isLoopDesktop: function() {
      return typeof rootObject.navigator.mozLoop === "object";
    },

    /**
     * Starts playing an audio file, stopping any audio that is already in progress.
     *
     * @param {String} filename The filename to play (excluding the extension).
     */
    play: function(filename, options) {
      if (this._isLoopDesktop()) {
        // XXX: We need navigator.mozLoop.playSound(name), see Bug 1089585.
        return;
      }

      options = options || {};
      options.loop = options.loop || false;

      this._ensureAudioStopped();
      this.audio = new Audio('shared/sounds/' + filename + ".ogg");
      this.audio.loop = options.loop;
      this.audio.play();
    },

    /**
     * Ensures audio is stopped playing, and removes the object from memory.
     */
    _ensureAudioStopped: function() {
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
    UrlHashChangeMixin: UrlHashChangeMixin
  };
})();
