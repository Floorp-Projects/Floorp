/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const nsIConsoleListenerWatcher = require("resource://devtools/server/actors/resources/utils/nsi-console-listener-watcher.js");
const {
  DevToolsServer,
} = require("resource://devtools/server/devtools-server.js");
const {
  createStringGrip,
} = require("resource://devtools/server/actors/object/utils.js");
const {
  getActorIdForInternalSourceId,
} = require("resource://devtools/server/actors/utils/dbg-source.js");
const {
  WebConsoleUtils,
} = require("resource://devtools/server/actors/webconsole/utils.js");

loader.lazyRequireGetter(
  this,
  ["getStyleSheetText"],
  "resource://devtools/server/actors/utils/stylesheet-utils.js",
  true
);

const {
  TYPES: { CSS_MESSAGE },
} = require("resource://devtools/server/actors/resources/index.js");

const { MESSAGE_CATEGORY } = require("resource://devtools/shared/constants.js");

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
    await this.#ensureCSSErrorReportingEnabled(targetActor);
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

  /**
   * Ensure that CSS error reporting is enabled for the provided target actor.
   *
   * @param {TargetActor} targetActor
   *        The target actor for which CSS Error Reporting should be enabled.
   * @return {Promise} Promise that resolves when cssErrorReportingEnabled was
   *         set in all the docShells owned by the provided target, and existing
   *         stylesheets have been re-parsed if needed.
   */
  async #ensureCSSErrorReportingEnabled(targetActor) {
    const docShells = targetActor.docShells;
    if (!docShells) {
      // If the target actor does not expose a docShells getter (ie is not an
      // instance of WindowGlobalTargetActor), nothing to do here.
      return;
    }

    const promises = docShells.map(async docShell => {
      if (docShell.cssErrorReportingEnabled) {
        // CSS Error Reporting already enabled here, nothing to do.
        return;
      }

      try {
        docShell.cssErrorReportingEnabled = true;
      } catch (e) {
        return;
      }

      // After enabling CSS Error Reporting, reparse existing stylesheets to
      // detect potential CSS errors.

      // Ensure docShell.document is available.
      docShell.QueryInterface(Ci.nsIWebNavigation);
      // We don't really want to reparse UA sheets and such, but want to do
      // Shadow DOM / XBL.
      const sheets = InspectorUtils.getAllStyleSheets(
        docShell.document,
        /* documentOnly = */ true
      );
      for (const sheet of sheets) {
        if (InspectorUtils.hasRulesModifiedByCSSOM(sheet)) {
          continue;
        }

        try {
          // Reparse the sheet so that we see the existing errors.
          const text = await getStyleSheetText(sheet);
          InspectorUtils.parseStyleSheet(sheet, text, /* aUpdate = */ false);
        } catch (e) {
          console.error("Error while parsing stylesheet");
        }
      }
    });

    await Promise.all(promises);
  }
}
module.exports = CSSMessageWatcher;
