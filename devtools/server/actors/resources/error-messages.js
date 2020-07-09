/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci, Cu } = require("chrome");
const Services = require("Services");
const ChromeUtils = require("ChromeUtils");
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

const {
  TYPES: { ERROR_MESSAGE },
} = require("devtools/server/actors/resources/index");

class ErrorMessageWatcher {
  /**
   * Start watching for all error messages related to a given Target Actor.
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
    // The following code expects the ThreadActor to be instantiated, via:
    // buildPageErrorResource > TabSources.getActorIdForInternalSourceId
    // The Thread Actor is instantiated via Target.attach, but we should
    // probably review this and only instantiate the actor instead of attaching the target.
    targetActor.attach();

    // Create the consoleListener.
    const listener = {
      QueryInterface: ChromeUtils.generateQI(["nsIConsoleListener"]),
      observe(message) {
        if (!shouldHandleMessage(targetActor, message)) {
          return;
        }

        onAvailable([buildPageErrorResource(targetActor, message)]);
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
      if (!shouldHandleMessage(targetActor, message)) {
        continue;
      }

      messages.push(buildPageErrorResource(targetActor, message));
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
module.exports = ErrorMessageWatcher;

/**
 * Returns true if the message is considered an error message, and as a result, should
 * be sent to the client.
 *
 * @param {nsIConsoleMessage|nsIScriptError} message
 */
function shouldHandleMessage(targetActor, message) {
  // The listener we use can be called either with a nsIConsoleMessage or a nsIScriptError.
  // In this file, we only want to handle nsIScriptError.
  if (
    // We only care about nsIScriptError
    !(message instanceof Ci.nsIScriptError) ||
    !isCategoryAllowed(targetActor, message.category) ||
    // Block any error that was triggered by eager evaluation
    message.sourceName === "debugger eager eval code" ||
    // In Process targets don't include messages from private window
    (isProcessTarget(targetActor) && message.isFromPrivateWindow)
  ) {
    return false;
  }

  // Process targets listen for everything
  if (isProcessTarget(targetActor)) {
    return true;
  }

  if (!message.innerWindowID) {
    return false;
  }

  const { window } = targetActor.window;
  const win = window?.WindowGlobalChild?.getByInnerWindowId(
    message.innerWindowID
  );
  return targetActor.browserId === win?.browsingContext?.browserId;
}

const PLATFORM_SPECIFIC_CATEGORIES = [
  "XPConnect JavaScript",
  "component javascript",
  "chrome javascript",
  "chrome registration",
];

/**
 * Check if the given message category is allowed to be tracked or not.
 * We ignore chrome-originating errors as we only care about content.
 *
 * @param string category
 *        The message category you want to check.
 * @return boolean
 *         True if the category is allowed to be logged, false otherwise.
 */
function isCategoryAllowed(targetActor, category) {
  // CSS Parser errors will be handled by the CSSMessageWatcher.
  if (!category || category === "CSS Parser") {
    return false;
  }

  // We listen for everything on Process targets
  if (isProcessTarget(targetActor)) {
    return true;
  }

  // For non-process targets, we filter-out platform-specific errors.
  return !PLATFORM_SPECIFIC_CATEGORIES.includes(category);
}

function isProcessTarget(targetActor) {
  const { typeName } = targetActor;
  return (
    typeName === "parentProcessTarget" || typeName === "contentProcessTarget"
  );
}

/**
 * Prepare an nsIScriptError to be sent to the client.
 *
 * @param nsIScriptError error
 *        The page error we need to send to the client.
 * @return object
 *         The object you can send to the remote client.
 */
function buildPageErrorResource(targetActor, error) {
  const stack = prepareStackForRemote(targetActor, error.stack);
  let lineText = error.sourceLine;
  if (lineText && lineText.length > DevToolsServer.LONG_STRING_INITIAL_LENGTH) {
    lineText = lineText.substr(0, DevToolsServer.LONG_STRING_INITIAL_LENGTH);
  }

  let notesArray = null;
  const notes = error.notes;
  if (notes?.length) {
    notesArray = [];
    for (let i = 0, len = notes.length; i < len; i++) {
      const note = notes.queryElementAt(i, Ci.nsIScriptErrorNote);
      notesArray.push({
        messageBody: createStringGrip(targetActor, note.errorMessage),
        frame: {
          source: note.sourceName,
          sourceId: getActorIdForInternalSourceId(targetActor, note.sourceId),
          line: note.lineNumber,
          column: note.columnNumber,
        },
      });
    }
  }

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
    timeStamp: error.timeStamp,
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

/**
 * Prepare a SavedFrame stack to be sent to the client.
 *
 * @param SavedFrame errorStack
 *        Stack for an error we need to send to the client.
 * @return object
 *         The object you can send to the remote client.
 */
function prepareStackForRemote(targetActor, errorStack) {
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
