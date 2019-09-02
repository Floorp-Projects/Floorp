/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Utils: WebConsoleUtils } = require("devtools/client/webconsole/utils");
const { EVALUATE_EXPRESSION } = require("devtools/client/webconsole/constants");

loader.lazyServiceGetter(
  this,
  "clipboardHelper",
  "@mozilla.org/widget/clipboardhelper;1",
  "nsIClipboardHelper"
);
loader.lazyRequireGetter(
  this,
  "saveScreenshot",
  "devtools/shared/screenshot/save"
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
const HELP_URL = "https://developer.mozilla.org/docs/Tools/Web_Console/Helpers";

function evaluateExpression(expression) {
  return async ({ dispatch, services }) => {
    if (!expression) {
      expression = services.getInputSelection() || services.getInputValue();
    }
    if (!expression) {
      return null;
    }

    // We use the messages action as it's doing additional transformation on the message.
    dispatch(
      messagesActions.messagesAdd([
        new ConsoleCommand({
          messageText: expression,
          timeStamp: Date.now(),
        }),
      ])
    );
    dispatch({
      type: EVALUATE_EXPRESSION,
      expression,
    });

    WebConsoleUtils.usageCount++;

    let mappedExpressionRes;
    try {
      mappedExpressionRes = await services.getMappedExpression(expression);
    } catch (e) {
      console.warn("Error when calling getMappedExpression", e);
    }

    expression = mappedExpressionRes
      ? mappedExpressionRes.expression
      : expression;

    const { frameActor, client } = services.getFrameActor();

    // Even if requestEvaluation rejects (because of webConsoleClient.evaluateJSAsync),
    // we still need to pass the error response to onExpressionEvaluated.
    const onSettled = res => res;

    const response = await client
      .evaluateJSAsync(expression, {
        frameActor,
        selectedNodeActor: services.getSelectedNodeActor(),
        mapped: mappedExpressionRes ? mappedExpressionRes.mapped : null,
      })
      .then(onSettled, onSettled);

    return onExpressionEvaluated(response, {
      dispatch,
      services,
    });
  };
}

/**
 * The JavaScript evaluation response handler.
 *
 * @private
 * @param {Object} response
 *        The message received from the server.
 */
async function onExpressionEvaluated(response, { dispatch, services } = {}) {
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
    dispatch(messagesActions.messagesAdd([response]));
    return;
  }

  await handleHelperResult(response, { dispatch, services });
}

async function handleHelperResult(response, { dispatch, services }) {
  const result = response.result;
  const helperResult = response.helperResult;
  const helperHasRawOutput = !!(helperResult || {}).rawOutput;

  if (helperResult && helperResult.type) {
    switch (helperResult.type) {
      case "clearOutput":
        dispatch(messagesActions.messagesClear());
        break;
      case "clearHistory":
        dispatch(historyActions.clearHistory());
        break;
      case "inspectObject":
        services.inspectObjectActor(helperResult.object);
        break;
      case "help":
        services.openLink(HELP_URL);
        break;
      case "copyValueToClipboard":
        clipboardHelper.copyString(helperResult.value);
        break;
      case "screenshotOutput":
        const { args, value } = helperResult;
        const screenshotMessages = await saveScreenshot(
          services.getPanelWindow(),
          args,
          value
        );
        dispatch(
          messagesActions.messagesAdd(
            screenshotMessages.map(message => ({
              message,
              type: "logMessage",
            }))
          )
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
}

module.exports = {
  evaluateExpression,
};
