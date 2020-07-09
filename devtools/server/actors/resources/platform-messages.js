/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const Services = require("Services");
const ChromeUtils = require("ChromeUtils");

const {
  TYPES: { PLATFORM_MESSAGE },
} = require("devtools/server/actors/resources/index");

const { stringIsLong } = require("devtools/server/actors/object/utils");
const { LongStringActor } = require("devtools/server/actors/string");

class PlatformMessageWatcher {
  /**
   * Start watching for all console messages related to a given Target Actor.
   * This will notify about existing console messages, but also the one created in future.
   *
   * @param TargetActor targetActor
   *        The target actor from which we should observe console messages
   * @param Object options
   *        Dictionary object with following attributes:
   *        - onAvailable: mandatory function
   *          This will be called for each resource.
   */
  constructor(targetActor, { onAvailable }) {
    // Platform messages can only be listened on process actors
    if (!targetActor.isRootActor) {
      return;
    }

    // Create the consoleListener.
    const listener = {
      QueryInterface: ChromeUtils.generateQI([Ci.nsIConsoleListener]),
      observe(message) {
        if (!shouldHandleMessage(message)) {
          return;
        }

        onAvailable([buildResourceFromMessage(targetActor, message)]);
      },
    };

    // Retrieve the cached messages just before registering the listener, so we don't get
    // duplicated messages.
    const cachedMessages = Services.console.getMessageArray();
    Services.console.registerListener(listener);
    this.listener = listener;

    // Remove unwanted cache messages and send an array of resources.
    const messages = [];
    for (const message of cachedMessages) {
      if (!shouldHandleMessage(message)) {
        continue;
      }

      messages.push(buildResourceFromMessage(targetActor, message));
    }
    onAvailable(messages);
  }

  /**
   * Stop watching for console messages.
   */
  destroy() {
    if (this.listener) {
      Services.console.unregisterListener(this.listener);
    }
  }
}
module.exports = PlatformMessageWatcher;

/**
 * Returns true if the message is considered a platform message, and as a result, should
 * be sent to the client.
 *
 * @param {nsIConsoleMessage} message
 */
function shouldHandleMessage(message) {
  // The listener we use can be called either with a nsIConsoleMessage or as nsIScriptError.
  // In this file, we want to ignore nsIScriptError, which are handled by the
  // error-messages resource handler (See Bug 1644186).
  if (message instanceof Ci.nsIScriptError) {
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
function buildResourceFromMessage(targetActor, message) {
  return {
    message: createStringGrip(targetActor, message.message),
    timeStamp: message.timeStamp,
    resourceType: PLATFORM_MESSAGE,
  };
}

/**
 * Create a grip for the given string.
 *
 * @param TargetActor targetActor
 *        The Target Actor from which this object originates.
 */
function createStringGrip(targetActor, string) {
  if (string && stringIsLong(string)) {
    const actor = new LongStringActor(targetActor.conn, string);
    targetActor.manage(actor);
    return actor.form();
  }
  return string;
}
