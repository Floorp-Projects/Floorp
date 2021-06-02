/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = function({ resource, targetFront }) {
  // @backward-compat { version 91 } `hasNativeConsoleAPI` is new on DOCUMENT_EVENT's will-navigate.
  //                                 Before, it was pulled from the WebConsole actor.
  if (resource.name == "dom-complete" && !("hasNativeConsoleAPI" in resource)) {
    // We assume here that console front has already be fetched by some other code
    // and that startListeners has been called.
    // We do these assumptions because ResourceTransformers have to be synchronous.
    const consoleFront = targetFront.getCachedFront("console");
    resource.hasNativeConsoleAPI = consoleFront.hasNativeConsoleAPI;
  }
  return resource;
};
