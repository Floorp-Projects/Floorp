/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["WindowGlobalLogger"];

function getWindowGlobalUri(windowGlobal) {
  let windowGlobalUri = "";

  if (windowGlobal.documentURI) {
    // If windowGlobal is a WindowGlobalParent documentURI should be available.
    windowGlobalUri = windowGlobal.documentURI.spec;
  } else if (windowGlobal.browsingContext?.window) {
    // If windowGlobal is a WindowGlobalChild, this code runs in the same
    // process as the document and we can directly access the window.location
    // object.
    windowGlobalUri = windowGlobal.browsingContext.window.location.href;
  }

  return windowGlobalUri;
}

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
    const { browsingContext } = windowGlobal;
    const { parent } = browsingContext;
    const windowGlobalUri = getWindowGlobalUri(windowGlobal);

    const details = [];
    details.push(
      "BrowsingContext.browserId: " + browsingContext.browserId,
      "BrowsingContext.id: " + browsingContext.id,
      "innerWindowId: " + windowGlobal.innerWindowId,
      "pid: " + windowGlobal.osPid,
      "isClosed: " + windowGlobal.isClosed,
      "isInProcess: " + windowGlobal.isInProcess,
      "isCurrentGlobal: " + windowGlobal.isCurrentGlobal,
      "currentRemoteType: " + browsingContext.currentRemoteType,
      "hasParent: " + (parent ? parent.id : "no"),
      "uri: " + (windowGlobalUri ? windowGlobalUri : "no uri")
    );

    const header = "[WindowGlobalLogger] " + message;

    // Use a padding for multiline display.
    const padding = "    ";
    const formattedDetails = details.map(s => padding + s);
    const detailsString = formattedDetails.join("\n");

    dump(header + "\n" + detailsString + "\n");
  },
};
