#ifdef 0
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#endif

/**
 * This singleton represents the page's toolbar that allows to enable/disable
 * the 'New Tab Page' feature and to reset the whole page.
 */
let gToolbar = {
  /**
   * Initializes the toolbar.
   * @param aSelector The query selector of the toolbar.
   */
  init: function Toolbar_init(aSelector) {
    this._node = document.querySelector(aSelector);
    let buttons = this._node.querySelectorAll("input");

    // Listen for 'click' events on the toolbar buttons.
    ["show", "hide", "reset"].forEach(function (aType, aIndex) {
      let self = this;
      let button = buttons[aIndex];
      let handler = function () self[aType]();

      button.addEventListener("click", handler, false);

#ifdef XP_MACOSX
      // Per default buttons lose focus after being clicked on Mac OS X.
      // So when the URL bar has focus and a toolbar button is clicked the
      // URL bar regains focus and the history pops up. We need to prevent
      // that by explicitly removing its focus.
      button.addEventListener("mousedown", function () {
        window.focus();
      }, false);
#endif
    }, this);
  },

  /**
   * Enables the 'New Tab Page' feature.
   */
  show: function Toolbar_show() {
    this._passButtonFocus("show", "hide");
    gAllPages.enabled = true;
  },

  /**
   * Disables the 'New Tab Page' feature.
   */
  hide: function Toolbar_hide() {
    this._passButtonFocus("hide", "show");
    gAllPages.enabled = false;
  },

  /**
   * Resets the whole page and forces it to re-render its content.
   * @param aCallback The callback to call when the page has been reset.
   */
  reset: function Toolbar_reset(aCallback) {
    this._passButtonFocus("reset", "hide");
    let node = gGrid.node;

    // animate the page reset
    gTransformation.fadeNodeOut(node, function () {
      NewTabUtils.reset();

      gLinks.populateCache(function () {
        gAllPages.update();

        // Without the setTimeout() we have a strange flicker.
        setTimeout(function () gTransformation.fadeNodeIn(node, aCallback));
      }, true);
    });
  },

  /**
   * Passes the focus from the current button to the next.
   * @param aCurrent The button that currently has focus.
   * @param aNext The button that is focused next.
   */
  _passButtonFocus: function Toolbar_passButtonFocus(aCurrent, aNext) {
    if (document.querySelector("#toolbar-button-" + aCurrent + ":-moz-focusring"))
      document.getElementById("toolbar-button-" + aNext).focus();
  }
};

