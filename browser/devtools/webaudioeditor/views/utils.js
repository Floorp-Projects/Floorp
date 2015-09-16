/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Takes an element in an SVG graph and iterates over
 * ancestors until it finds the graph node container. If not found,
 * returns null.
 */

function findGraphNodeParent (el) {
  // Some targets may not contain `classList` property
  if (!el.classList)
    return null;

  while (!el.classList.contains("nodes")) {
    if (el.classList.contains("audionode"))
      return el;
    else
      el = el.parentNode;
  }
  return null;
}

/**
 * Object for use with `mix` into a view.
 * Must have the following properties defined on the view:
 * - `el`
 * - `button`
 * - `_collapseString`
 * - `_expandString`
 * - `_toggleEvent`
 *
 * Optional properties on the view can be defined to specify default
 * visibility options.
 * - `_animated`
 * - `_delayed`
 */
var ToggleMixin = {

  bindToggle: function () {
    this._onToggle = this._onToggle.bind(this);
    this.button.addEventListener("mousedown", this._onToggle, false);
  },

  unbindToggle: function () {
    this.button.removeEventListener("mousedown", this._onToggle);
  },

  show: function () {
    this._viewController({ visible: true });
  },

  hide: function () {
    this._viewController({ visible: false });
  },

  hideImmediately: function () {
    this._viewController({ visible: false, delayed: false, animated: false });
  },

  /**
   * Returns a boolean indicating whether or not the view.
   * is currently being shown.
   */
  isVisible: function () {
    return !this.el.hasAttribute("pane-collapsed");
  },

  /**
   * Toggles the visibility of the view.
   *
   * @param object visible
   *        - visible: boolean indicating whether the panel should be shown or not
   *        - animated: boolean indiciating whether the pane should be animated
   *        - delayed: boolean indicating whether the pane's opening should wait
   *                   a few cycles or not
   */
  _viewController: function ({ visible, animated, delayed }) {
    let flags = {
      visible: visible,
      animated: animated != null ? animated : !!this._animated,
      delayed: delayed != null ? delayed : !!this._delayed,
      callback: () => window.emit(this._toggleEvent, visible)
    };

    ViewHelpers.togglePane(flags, this.el);

    if (flags.visible) {
      this.button.removeAttribute("pane-collapsed");
      this.button.setAttribute("tooltiptext", this._collapseString);
    }
    else {
      this.button.setAttribute("pane-collapsed", "");
      this.button.setAttribute("tooltiptext", this._expandString);
    }
  },

  _onToggle: function () {
    this._viewController({ visible: !this.isVisible() });
  }
}
