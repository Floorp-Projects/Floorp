/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.shared = loop.shared || {};
loop.shared.router = (function() {
  "use strict";

  /**
   * Base Router. Allows defining a main active view and ease toggling it when
   * the active route changes.
   *
   * @link http://mikeygee.com/blog/backbone.html
   */
  var BaseRouter = Backbone.Router.extend({
    activeView: undefined,

    /**
     * Loads and render current active view.
     *
     * @param {loop.shared.views.BaseView} view View.
     */
    loadView : function(view) {
      if (this.activeView) {
        this.activeView.hide();
      }
      this.activeView = view.render().show();
    }
  });

  return {
    BaseRouter: BaseRouter
  };
})();
