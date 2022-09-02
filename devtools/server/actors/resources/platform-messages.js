/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const nsIConsoleListenerWatcher = require("devtools/server/actors/resources/utils/nsi-console-listener-watcher");
const { Ci } = require("chrome");

const {
  TYPES: { PLATFORM_MESSAGE },
} = require("devtools/server/actors/resources/index");

const { createStringGrip } = require("devtools/server/actors/object/utils");

class PlatformMessageWatcher extends nsIConsoleListenerWatcher {
  shouldHandleTarget(targetActor) {
    return this.isProcessTarget(targetActor);
  }

  /**
   * Returns true if the message is considered a platform message, and as a result, should
   * be sent to the client.
   *
   * @param {TargetActor} targetActor
   * @param {nsIConsoleMessage} message
   */
  shouldHandleMessage(targetActor, message) {
    // The listener we use can be called either with a nsIConsoleMessage or as nsIScriptError.
    // In this file, we want to ignore nsIScriptError, which are handled by the
    // error-messages resource handler (See Bug 1644186).
    if (message instanceof Ci.nsIScriptError) {
      return false;
    }

    // Ignore message that were forwarded from the content process to the parent process,
    // since we're getting those directly from the content process.
    if (message.isForwardedFromContentProcess) {
      return false;
    }

    return true;
  }

  /**
   * Returns an object from the nsIConsoleMessage.
   *
   * @param {Actor} targetActor
   * @param {nsIConsoleMessage} message
   */
  buildResource(targetActor, message) {
    return {
      message: createStringGrip(targetActor, message.message),
      timeStamp: message.microSecondTimeStamp / 1000,
      resourceType: PLATFORM_MESSAGE,
    };
  }
}
module.exports = PlatformMessageWatcher;
