/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* jshint esnext:true */
/* global loop:true */

var loop = loop || {};
loop.desktopRouter = (function() {
  "use strict";

  /**
   * On the desktop app, the use of about: uris prevents us from changing the
   * url of the location. As a result, we change the navigate function to simply
   * activate the new routes, and not try changing the url.
   *
   * XXX It is conceivable we might be able to remove this in future, if we
   * can either swap to resource uris or remove the limitation on the about uris.
   */
  var extendedRouter = {
    navigate: function(to) {
      this[this.routes[to]]();
    }
  };

  var DesktopRouter = loop.shared.router.BaseRouter.extend(extendedRouter);

  var DesktopConversationRouter =
    loop.shared.router.BaseConversationRouter.extend(extendedRouter);

  return {
    DesktopRouter: DesktopRouter,
    DesktopConversationRouter: DesktopConversationRouter
  };
})();
