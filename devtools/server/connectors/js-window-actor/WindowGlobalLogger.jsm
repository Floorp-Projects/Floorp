/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["WindowGlobalLogger"];

const WindowGlobalLogger = {
  /**
   * This logger can run from the content or parent process, and windowGlobal
   * will either be of type `WindowGlobalParent` or `WindowGlobalChild`.
   *
   * The interface for each type can be found in WindowGlobalActors.webidl
   * (https://searchfox.org/mozilla-central/source/dom/chrome-webidl/WindowGlobalActors.webidl)
   *
   * @param {WindowGlobalParent|WindowGlobalChild} windowGlobal
   *        The window global to log. See WindowGlobalActors.webidl for details
   *        about the types.
   * @param {String} message
   *        A custom message that will be displayed at the beginning of the log.
   */
  logWindowGlobal: function(windowGlobal, message) {
    const browsingContext = windowGlobal.browsingContext;
    dump(
      message +
        " | BrowsingContext.browserId: " +
        browsingContext.browserId +
        " id: " +
        browsingContext.id +
        " Inner Window ID: " +
        windowGlobal.innerWindowId +
        " pid:" +
        windowGlobal.osPid +
        " isClosed:" +
        windowGlobal.isClosed +
        " isInProcess:" +
        windowGlobal.isInProcess +
        " isCurrentGlobal:" +
        windowGlobal.isCurrentGlobal +
        " currentRemoteType:" +
        browsingContext.currentRemoteType +
        " hasParent:" +
        (browsingContext.parent ? browsingContext.parent.id : "no") +
        " => " +
        (windowGlobal.documentURI ? windowGlobal.documentURI.spec : "no-uri") +
        "\n"
    );
  },
};
