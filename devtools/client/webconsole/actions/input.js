/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Utils: WebConsoleUtils } = require("devtools/client/webconsole/utils");
const {
  EVALUATE_EXPRESSION,
  SET_TERMINAL_INPUT,
  SET_TERMINAL_EAGER_RESULT,
  EDITOR_PRETTY_PRINT,
} = require("devtools/client/webconsole/constants");
const { getAllPrefs } = require("devtools/client/webconsole/selectors/prefs");
const ResourceCommand = require("devtools/shared/commands/resource/resource-command");
const l10n = require("devtools/client/webconsole/utils/l10n");

loader.lazyServiceGetter(
  this,
  "clipboardHelper",
  "@mozilla.org/widget/clipboardhelper;1",
  "nsIClipboardHelper"
);
loader.lazyRequireGetter(
  this,
  "messagesActions",
  "devtools/client/webconsole/actions/messages"
);
loader.lazyRequireGetter(
  this,
  "historyActions",
  "devtools/client/webconsole/actions/history"
);
loader.lazyRequireGetter(
  this,
  "ConsoleCommand",
  "devtools/client/webconsole/types",
  true
);
loader.lazyRequireGetter(
  this,
  "netmonitorBlockingActions",
  "devtools/client/netmonitor/src/actions/request-blocking"
);

loader.lazyRequireGetter(
  this,
  ["saveScreenshot", "captureAndSaveScreenshot"],
  "devtools/client/shared/screenshot",
  true
);

const HELP_URL = "https://developer.mozilla.org/docs/Tools/Web_Console/Helpers";

async function getMappedExpression(hud, expression) {
  let mapResult;
  try {
    mapResult = await hud.getMappedExpression(expression);
  } catch (e) {
    console.warn("Error when calling getMappedExpression", e);
  }

  let mapped = null;
  if (mapResult) {
    ({ expression, mapped } = mapResult);
  }
  return { expression, mapped };
}

function evaluateExpression(expression, from = "input") {
  return async ({ dispatch, toolbox, webConsoleUI, hud, client }) => {
    if (!expression) {
      expression = hud.getInputSelection() || hud.getInputValue();
    }
    if (!expression) {
      return null;
    }

    // We use the messages action as it's doing additional transformation on the message.
    const { messages } = dispatch(
      messagesActions.messagesAdd([
        new ConsoleCommand({
          messageText: expression,
          timeStamp: Date.now(),
        }),
      ])
    );
    const [consoleCommandMessage] = messages;

    dispatch({
      type: EVALUATE_EXPRESSION,
      expression,
      from,
    });

    WebConsoleUtils.usageCount++;

    let mapped;
    ({ expression, mapped } = await getMappedExpression(hud, expression));

    // Even if the evaluation fails,
    // we still need to pass the error response to onExpressionEvaluated.
    const onSettled = res => res;

    const response = await client
      .evaluateJSAsync(expression, {
        frameActor: webConsoleUI.getFrameActor(),
        selectedNodeActor: webConsoleUI.getSelectedNodeActorID(),
        selectedTargetFront: toolbox && toolbox.getSelectedTargetFront(),
        mapped,
      })
      .then(onSettled, onSettled);

    const serverConsoleCommandTimestamp = response.startTime;

    // In case of remote debugging, it might happen that the debuggee page does not have
    // the exact same clock time as the client. This could cause some ordering issues
    // where the result message is displayed *before* the expression that lead to it.
    if (
      serverConsoleCommandTimestamp &&
      consoleCommandMessage.timeStamp > serverConsoleCommandTimestamp
    ) {
      // If we're in such case, we remove the original command message, and add it again,
      // with the timestamp coming from the server.
      dispatch(messagesActions.messageRemove(consoleCommandMessage.id));
      dispatch(
        messagesActions.messagesAdd([
          new ConsoleCommand({
            messageText: expression,
            timeStamp: serverConsoleCommandTimestamp,
          }),
        ])
      );
    }

    return dispatch(onExpressionEvaluated(response));
  };
}

/**
 * The JavaScript evaluation response handler.
 *
 * @private
 * @param {Object} response
 *        The message received from the server.
 */
function onExpressionEvaluated(response) {
  return async ({ dispatch, webConsoleUI }) => {
    if (response.error) {
      console.error(`Evaluation error`, response.error, ": ", response.message);
      return;
    }

    // If the evaluation was a top-level await expression that was rejected, there will
    // be an uncaught exception reported, so we don't need to do anything.
    if (response.topLevelAwaitRejected === true) {
      return;
    }

    if (!response.helperResult) {
      webConsoleUI.wrapper.dispatchMessageAdd(response);
      return;
    }

    await dispatch(handleHelperResult(response));
  };
}

function handleHelperResult(response) {
  // eslint-disable-next-line complexity
  return async ({ dispatch, hud, toolbox, webConsoleUI }) => {
    const { result, helperResult } = response;
    const helperHasRawOutput = !!helperResult?.rawOutput;
    const hasNetworkResourceCommandSupport = hud.resourceCommand.hasResourceCommandSupport(
      hud.resourceCommand.TYPES.NETWORK_EVENT
    );
    let networkFront = null;
    // @backward-compat { version 86 } default network events watcher support
    if (hasNetworkResourceCommandSupport) {
      networkFront = await hud.resourceCommand.watcherFront.getNetworkParentActor();
    }

    if (helperResult?.type) {
      switch (helperResult.type) {
        case "clearOutput":
          dispatch(messagesActions.messagesClear());
          break;
        case "clearHistory":
          dispatch(historyActions.clearHistory());
          break;
        case "inspectObject": {
          const objectActor = helperResult.object;
          if (hud.toolbox && !helperResult.forceExpandInConsole) {
            hud.toolbox.inspectObjectActor(objectActor);
          } else {
            webConsoleUI.inspectObjectActor(objectActor);
          }
          break;
        }
        case "help":
          hud.openLink(HELP_URL);
          break;
        case "copyValueToClipboard":
          clipboardHelper.copyString(helperResult.value);
          break;
        case "screenshotOutput":
          const { args, value } = helperResult;
          const targetFront =
            toolbox?.getSelectedTargetFront() || hud.currentTarget;
          let screenshotMessages;

          // @backward-compat { version 87 } The screenshot-content actor isn't available
          // in older server.
          // With an old server, the console actor captures the screenshot when handling
          // the command, and send it to the client which only needs to save it to a file.
          // With a new server, the server simply acknowledges the command,
          // and the client will drive the whole screenshot process (capture and save).
          if (targetFront.hasActor("screenshotContent")) {
            screenshotMessages = await captureAndSaveScreenshot(
              targetFront,
              webConsoleUI.getPanelWindow(),
              args
            );
          } else {
            screenshotMessages = await saveScreenshot(
              webConsoleUI.getPanelWindow(),
              args,
              value
            );
          }

          if (screenshotMessages && screenshotMessages.length > 0) {
            dispatch(
              messagesActions.messagesAdd(
                screenshotMessages.map(message => ({
                  message: {
                    level: message.level || "log",
                    arguments: [message.text],
                  },
                  resourceType: ResourceCommand.TYPES.CONSOLE_MESSAGE,
                }))
              )
            );
          }
          break;
        case "blockURL":
          const blockURL = helperResult.args.url;
          // The console actor isn't able to block the request as the console actor runs in the content
          // process, while the request has to be blocked from the parent process.
          // Then, calling the Netmonitor action will only update the visual state of the Netmonitor,
          // but we also have to block the request via the NetworkParentActor.
          // @backward-compat { version 86 } default network events watcher support
          if (hasNetworkResourceCommandSupport && networkFront) {
            await networkFront.blockRequest({ url: blockURL });
          }
          toolbox
            .getPanel("netmonitor")
            ?.panelWin.store.dispatch(
              netmonitorBlockingActions.addBlockedUrl(blockURL)
            );

          dispatch(
            messagesActions.messagesAdd([
              {
                resourceType: ResourceCommand.TYPES.PLATFORM_MESSAGE,
                message: l10n.getFormatStr(
                  "webconsole.message.commands.blockedURL",
                  [blockURL]
                ),
              },
            ])
          );
          break;
        case "unblockURL":
          const unblockURL = helperResult.args.url;
          // @backward-compat { version 86 } see related comments in block url above
          if (hasNetworkResourceCommandSupport && networkFront) {
            await networkFront.unblockRequest({ url: unblockURL });
          }
          toolbox
            .getPanel("netmonitor")
            ?.panelWin.store.dispatch(
              netmonitorBlockingActions.removeBlockedUrl(unblockURL)
            );

          dispatch(
            messagesActions.messagesAdd([
              {
                resourceType: ResourceCommand.TYPES.PLATFORM_MESSAGE,
                message: l10n.getFormatStr(
                  "webconsole.message.commands.unblockedURL",
                  [unblockURL]
                ),
              },
            ])
          );
          // early return as we already dispatched necessary messages.
          return;
      }
    }

    const hasErrorMessage =
      response.exceptionMessage ||
      (helperResult && helperResult.type === "error");

    // Hide undefined results coming from helper functions.
    const hasUndefinedResult =
      result && typeof result == "object" && result.type == "undefined";

    if (hasErrorMessage || helperHasRawOutput || !hasUndefinedResult) {
      dispatch(messagesActions.messagesAdd([response]));
    }
  };
}

function focusInput() {
  return ({ hud }) => {
    return hud.focusInput();
  };
}

function setInputValue(value) {
  return ({ hud }) => {
    return hud.setInputValue(value);
  };
}

/**
 * Request an eager evaluation from the server.
 *
 * @param {String} expression: The expression to evaluate.
 * @param {Boolean} force: When true, will request an eager evaluation again, even if
 *                         the expression is the same one than the one that was used in
 *                         the previous evaluation.
 */
function terminalInputChanged(expression, force = false) {
  return async ({ dispatch, webConsoleUI, hud, toolbox, client, getState }) => {
    const prefs = getAllPrefs(getState());
    if (!prefs.eagerEvaluation) {
      return null;
    }

    const { terminalInput = "" } = getState().history;

    // Only re-evaluate if the expression did change.
    if (
      (!terminalInput && !expression) ||
      (typeof terminalInput === "string" &&
        typeof expression === "string" &&
        expression.trim() === terminalInput.trim() &&
        !force)
    ) {
      return null;
    }

    dispatch({
      type: SET_TERMINAL_INPUT,
      expression: expression.trim(),
    });

    // There's no need to evaluate an empty string.
    if (!expression || !expression.trim()) {
      return dispatch({
        type: SET_TERMINAL_EAGER_RESULT,
        expression,
        result: null,
      });
    }

    let mapped;
    ({ expression, mapped } = await getMappedExpression(hud, expression));

    const response = await client.evaluateJSAsync(expression, {
      frameActor: await webConsoleUI.getFrameActor(),
      selectedNodeActor: webConsoleUI.getSelectedNodeActorID(),
      selectedTargetFront: toolbox && toolbox.getSelectedTargetFront(),
      mapped,
      eager: true,
    });

    return dispatch({
      type: SET_TERMINAL_EAGER_RESULT,
      result: getEagerEvaluationResult(response),
    });
  };
}

/**
 * Refresh the current eager evaluation by requesting a new eager evaluation.
 */
function updateInstantEvaluationResultForCurrentExpression() {
  return ({ getState, dispatch }) =>
    dispatch(terminalInputChanged(getState().history.terminalInput, true));
}

function getEagerEvaluationResult(response) {
  const result = response.exception || response.result;
  // Don't show syntax errors results to the user.
  if (result?.isSyntaxError || (result && result.type == "undefined")) {
    return null;
  }

  return result;
}

function prettyPrintEditor() {
  return {
    type: EDITOR_PRETTY_PRINT,
  };
}

module.exports = {
  evaluateExpression,
  focusInput,
  setInputValue,
  terminalInputChanged,
  updateInstantEvaluationResultForCurrentExpression,
  prettyPrintEditor,
};
