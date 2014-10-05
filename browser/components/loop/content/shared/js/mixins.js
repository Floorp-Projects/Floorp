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
   * Dropdown menu mixin.
   * @type {Object}
   */
  var DropdownMenuMixin = {
    getInitialState: function() {
      return {showMenu: false};
    },

    _onBodyClick: function() {
      this.setState({showMenu: false});
    },

    componentDidMount: function() {
      rootObject.document.body.addEventListener("click", this._onBodyClick);
    },

    componentWillUnmount: function() {
      rootObject.document.body.removeEventListener("click", this._onBodyClick);
    },

    showDropdownMenu: function() {
      this.setState({showMenu: true});
    },

    hideDropdownMenu: function() {
      this.setState({showMenu: false});
    }
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

  return {
    setRootObject: setRootObject,
    DropdownMenuMixin: DropdownMenuMixin,
    DocumentVisibilityMixin: DocumentVisibilityMixin
  };
})();
