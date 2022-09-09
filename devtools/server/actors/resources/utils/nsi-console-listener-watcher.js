/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci, Cu } = require("chrome");

const { createStringGrip } = require("devtools/server/actors/object/utils");

const {
  getActorIdForInternalSourceId,
} = require("devtools/server/actors/utils/dbg-source");

class nsIConsoleListenerWatcher {
  /**
   * Start watching for all messages related to a given Target Actor.
   * This will notify about existing messages, as well as those created in the future.
   *
   * @param TargetActor targetActor
   *        The target actor from which we should observe messages
   * @param Object options
   *        Dictionary object with following attributes:
   *        - onAvailable: mandatory function
   *          This will be called for each resource.
   */
  async watch(targetActor, { onAvailable }) {
    if (!this.shouldHandleTarget(targetActor)) {
      return;
    }

    // Create the consoleListener.
    const listener = {
      QueryInterface: ChromeUtils.generateQI(["nsIConsoleListener"]),
      observe: message => {
        if (!this.shouldHandleMessage(targetActor, message)) {
          return;
        }

        onAvailable([this.buildResource(targetActor, message)]);
      },
    };

    // Retrieve the cached messages just before registering the listener, so we don't get
    // duplicated messages.
    const cachedMessages = Services.console.getMessageArray() || [];
    Services.console.registerListener(listener);
    this.listener = listener;

    // Remove unwanted cache messages and send an array of resources.
    const messages = [];
    for (const message of cachedMessages) {
      if (!this.shouldHandleMessage(targetActor, message, true)) {
        continue;
      }

      messages.push(this.buildResource(targetActor, message));
    }
    onAvailable(messages);
  }

  /**
   * Return false if the watcher shouldn't be created.
   *
   * @param {TargetActor} targetActor
   * @return {Boolean}
   */
  shouldHandleTarget(targetActor) {
    return true;
  }

  /**
   * Return true if you want the passed message to be handled by the watcher. This should
   * be implemented on the child class.
   *
   * @param {TargetActor} targetActor
   * @param {nsIScriptError|nsIConsoleMessage} message
   * @return {Boolean}
   */
  shouldHandleMessage(targetActor, message) {
    throw new Error(
      "'shouldHandleMessage' should be implemented in the class that extends nsIConsoleListenerWatcher"
    );
  }

  /**
   * Prepare the resource to be sent to the client. This should be implemented on the
   * child class.
   *
   * @param targetActor
   * @param nsIScriptError|nsIConsoleMessage message
   * @return object
   *         The object you can send to the remote client.
   */
  buildResource(targetActor, message) {
    throw new Error(
      "'buildResource' should be implemented in the class that extends nsIConsoleListenerWatcher"
    );
  }

  /**
   * Prepare a SavedFrame stack to be sent to the client.
   *
   * @param {TargetActor} targetActor
   * @param {SavedFrame} errorStack
   *        Stack for an error we need to send to the client.
   * @return object
   *         The object you can send to the remote client.
   */
  prepareStackForRemote(targetActor, errorStack) {
    // Convert stack objects to the JSON attributes expected by client code
    // Bug 1348885: If the global from which this error came from has been
    // nuked, stack is going to be a dead wrapper.
    if (!errorStack || (Cu && Cu.isDeadWrapper(errorStack))) {
      return null;
    }
    const stack = [];
    let s = errorStack;
    while (s) {
      stack.push({
        filename: s.source,
        sourceId: getActorIdForInternalSourceId(targetActor, s.sourceId),
        lineNumber: s.line,
        columnNumber: s.column,
        functionName: s.functionDisplayName,
        asyncCause: s.asyncCause ? s.asyncCause : undefined,
      });
      s = s.parent || s.asyncParent;
    }
    return stack;
  }

  /**
   * Prepare error notes to be sent to the client.
   *
   * @param {TargetActor} targetActor
   * @param {nsIArray<nsIScriptErrorNote>} errorNotes
   * @return object
   *         The object you can send to the remote client.
   */
  prepareNotesForRemote(targetActor, errorNotes) {
    if (!errorNotes?.length) {
      return null;
    }

    const notes = [];
    for (let i = 0, len = errorNotes.length; i < len; i++) {
      const note = errorNotes.queryElementAt(i, Ci.nsIScriptErrorNote);
      notes.push({
        messageBody: createStringGrip(targetActor, note.errorMessage),
        frame: {
          source: note.sourceName,
          sourceId: getActorIdForInternalSourceId(targetActor, note.sourceId),
          line: note.lineNumber,
          column: note.columnNumber,
        },
      });
    }
    return notes;
  }

  isProcessTarget(targetActor) {
    const { typeName } = targetActor;
    return (
      typeName === "parentProcessTarget" || typeName === "contentProcessTarget"
    );
  }

  /**
   * Stop watching for messages.
   */
  destroy() {
    if (this.listener) {
      Services.console.unregisterListener(this.listener);
    }
  }
}
module.exports = nsIConsoleListenerWatcher;
