/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const nsIConsoleListenerWatcher = require("devtools/server/actors/resources/utils/nsi-console-listener-watcher");
const { Ci } = require("chrome");
const { DevToolsServer } = require("devtools/server/devtools-server");
const ErrorDocs = require("devtools/server/actors/errordocs");
const {
  createStringGrip,
  makeDebuggeeValue,
  createValueGripForTarget,
} = require("devtools/server/actors/object/utils");
const {
  getActorIdForInternalSourceId,
} = require("devtools/server/actors/utils/dbg-source");
const { WebConsoleUtils } = require("devtools/server/actors/webconsole/utils");

const {
  TYPES: { ERROR_MESSAGE },
} = require("devtools/server/actors/resources/index");
const Targets = require("devtools/server/actors/targets/index");

const { MESSAGE_CATEGORY } = require("devtools/shared/constants");

const PLATFORM_SPECIFIC_CATEGORIES = [
  "XPConnect JavaScript",
  "component javascript",
  "chrome javascript",
  "chrome registration",
];

class ErrorMessageWatcher extends nsIConsoleListenerWatcher {
  shouldHandleMessage(targetActor, message, isCachedMessage = false) {
    // The listener we use can be called either with a nsIConsoleMessage or a nsIScriptError.
    // In this file, we only want to handle nsIScriptError.
    if (
      // We only care about nsIScriptError
      !(message instanceof Ci.nsIScriptError) ||
      !this.isCategoryAllowed(targetActor, message.category) ||
      // Block any error that was triggered by eager evaluation
      message.sourceName === "debugger eager eval code"
    ) {
      return false;
    }

    // Filter specific to CONTENT PROCESS targets
    if (this.isProcessTarget(targetActor)) {
      // Don't want to display cached messages from private windows.
      const isCachedFromPrivateWindow =
        isCachedMessage && message.isFromPrivateWindow;
      if (isCachedFromPrivateWindow) {
        return false;
      }

      // `ContentChild` forwards all errors to the parent process (via IPC) all errors up
      // the parent process and sets a `isForwardedFromContentProcess` property on them.
      // Ignore these forwarded messages as the original ones will be logged either in a
      // content process target (if window-less message) or frame target (if related to a window)
      if (message.isForwardedFromContentProcess) {
        return false;
      }

      // Ignore all messages related to a given window for content process targets
      // These messages will be handled by Watchers instantiated for the related frame targets
      if (
        targetActor.targetType == Targets.TYPES.PROCESS &&
        message.innerWindowID
      ) {
        return false;
      }

      return true;
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
   * Check if the given message category is allowed to be tracked or not.
   * We ignore chrome-originating errors as we only care about content.
   *
   * @param string category
   *        The message category you want to check.
   * @return boolean
   *         True if the category is allowed to be logged, false otherwise.
   */
  isCategoryAllowed(targetActor, category) {
    // CSS Parser errors will be handled by the CSSMessageWatcher.
    if (!category || category === MESSAGE_CATEGORY.CSS_PARSER) {
      return false;
    }

    // We listen for everything on Process targets
    if (this.isProcessTarget(targetActor)) {
      return true;
    }

    // Don't restrict any categories in the Browser Toolbox/Browser Console
    if (targetActor.sessionContext.type == "all") {
      return true;
    }

    // For non-process targets in other toolboxes, we filter-out platform-specific errors.
    return !PLATFORM_SPECIFIC_CATEGORIES.includes(category);
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
      errorMessageName: error.errorMessageName,
      exceptionDocURL: ErrorDocs.GetURL(error),
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
      isPromiseRejection: error.isPromiseRejection,
      isForwardedFromContentProcess: error.isForwardedFromContentProcess,
    };

    // If the pageError does have an exception object, we want to return the grip for it,
    // but only if we do manage to get the grip, as we're checking the property on the
    // client to render things differently.
    if (error.hasException) {
      try {
        const obj = makeDebuggeeValue(targetActor, error.exception);
        if (obj?.class !== "DeadObject") {
          pageError.exception = createValueGripForTarget(targetActor, obj);
          pageError.hasException = true;
        }
      } catch (e) {}
    }

    return {
      pageError,
      resourceType: ERROR_MESSAGE,
    };
  }
}
module.exports = ErrorMessageWatcher;
