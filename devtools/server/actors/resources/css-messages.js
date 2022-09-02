/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const nsIConsoleListenerWatcher = require("devtools/server/actors/resources/utils/nsi-console-listener-watcher");
const { Ci } = require("chrome");
const { DevToolsServer } = require("devtools/server/devtools-server");
const { createStringGrip } = require("devtools/server/actors/object/utils");
const {
  getActorIdForInternalSourceId,
} = require("devtools/server/actors/utils/dbg-source");
const { WebConsoleUtils } = require("devtools/server/actors/webconsole/utils");

const {
  TYPES: { CSS_MESSAGE },
} = require("devtools/server/actors/resources/index");

const { MESSAGE_CATEGORY } = require("devtools/shared/constants");

class CSSMessageWatcher extends nsIConsoleListenerWatcher {
  /**
   * Start watching for all CSS messages related to a given Target Actor.
   * This will notify about existing messages, but also the one created in future.
   *
   * @param TargetActor targetActor
   *        The target actor from which we should observe messages
   * @param Object options
   *        Dictionary object with following attributes:
   *        - onAvailable: mandatory function
   *          This will be called for each resource.
   */
  async watch(targetActor, { onAvailable }) {
    super.watch(targetActor, { onAvailable });

    // Calling ensureCSSErrorReportingEnabled will make the server parse the stylesheets to
    // retrieve the warnings if the docShell wasn't already watching for CSS messages.
    if (targetActor.ensureCSSErrorReportingEnabled) {
      targetActor.ensureCSSErrorReportingEnabled();
    }
  }

  /**
   * Returns true if the message is considered a CSS message, and as a result, should
   * be sent to the client.
   *
   * @param {nsIConsoleMessage|nsIScriptError} message
   */
  shouldHandleMessage(targetActor, message) {
    // The listener we use can be called either with a nsIConsoleMessage or as nsIScriptError.
    // In this file, we want to ignore anything but nsIScriptError.
    if (
      // We only care about CSS Parser nsIScriptError
      !(message instanceof Ci.nsIScriptError) ||
      message.category !== MESSAGE_CATEGORY.CSS_PARSER
    ) {
      return false;
    }

    // Filter specific to CONTENT PROCESS targets
    // Process targets listen for everything but messages from private windows.
    if (this.isProcessTarget(targetActor)) {
      return !message.isFromPrivateWindow;
    }

    if (!message.innerWindowID) {
      return false;
    }

    const ids = targetActor.windows.map(window =>
      WebConsoleUtils.getInnerWindowId(window)
    );
    return ids.includes(message.innerWindowID);
  }

  /**
   * Prepare an nsIScriptError to be sent to the client.
   *
   * @param nsIScriptError error
   *        The page error we need to send to the client.
   * @return object
   *         The object you can send to the remote client.
   */
  buildResource(targetActor, error) {
    const stack = this.prepareStackForRemote(targetActor, error.stack);
    let lineText = error.sourceLine;
    if (
      lineText &&
      lineText.length > DevToolsServer.LONG_STRING_INITIAL_LENGTH
    ) {
      lineText = lineText.substr(0, DevToolsServer.LONG_STRING_INITIAL_LENGTH);
    }

    const notesArray = this.prepareNotesForRemote(targetActor, error.notes);

    // If there is no location information in the error but we have a stack,
    // fill in the location with the first frame on the stack.
    let { sourceName, sourceId, lineNumber, columnNumber } = error;
    if (!sourceName && !sourceId && !lineNumber && !columnNumber && stack) {
      sourceName = stack[0].filename;
      sourceId = stack[0].sourceId;
      lineNumber = stack[0].lineNumber;
      columnNumber = stack[0].columnNumber;
    }

    const pageError = {
      errorMessage: createStringGrip(targetActor, error.errorMessage),
      sourceName,
      sourceId: getActorIdForInternalSourceId(targetActor, sourceId),
      lineText,
      lineNumber,
      columnNumber,
      category: error.category,
      innerWindowID: error.innerWindowID,
      timeStamp: error.microSecondTimeStamp / 1000,
      warning: !!(error.flags & error.warningFlag),
      error: !(error.flags & (error.warningFlag | error.infoFlag)),
      info: !!(error.flags & error.infoFlag),
      private: error.isFromPrivateWindow,
      stacktrace: stack,
      notes: notesArray,
      chromeContext: error.isFromChromeContext,
      isForwardedFromContentProcess: error.isForwardedFromContentProcess,
    };

    return {
      pageError,
      resourceType: CSS_MESSAGE,
      cssSelectors: error.cssSelectors,
    };
  }
}
module.exports = CSSMessageWatcher;
