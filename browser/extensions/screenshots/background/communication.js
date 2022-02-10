/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals catcher, log */

"use strict";

this.communication = (function() {
  const exports = {};

  const registeredFunctions = {};

  exports.onMessage = catcher.watchFunction((req, sender, sendResponse) => {
    if (!(req.funcName in registeredFunctions)) {
      log.error(`Received unknown internal message type ${req.funcName}`);
      sendResponse({ type: "error", name: "Unknown message type" });
      return;
    }
    if (!Array.isArray(req.args)) {
      log.error("Received message with no .args list");
      sendResponse({ type: "error", name: "No .args" });
      return;
    }
    const func = registeredFunctions[req.funcName];
    let result;
    try {
      req.args.unshift(sender);
      result = func.apply(null, req.args);
    } catch (e) {
      log.error(`Error in ${req.funcName}:`, e, e.stack);
      // FIXME: should consider using makeError from catcher here:
      sendResponse({
        type: "error",
        message: e + "",
        errorCode: e.errorCode,
        popupMessage: e.popupMessage,
      });
      return;
    }
    if (result && result.then) {
      result
        .then(concreteResult => {
          sendResponse({ type: "success", value: concreteResult });
        })
        .catch(errorResult => {
          log.error(
            `Promise error in ${req.funcName}:`,
            errorResult,
            errorResult && errorResult.stack
          );
          sendResponse({
            type: "error",
            message: errorResult + "",
            errorCode: errorResult.errorCode,
            popupMessage: errorResult.popupMessage,
          });
        });
      return;
    }
    sendResponse({ type: "success", value: result });
  });

  exports.register = function(name, func) {
    registeredFunctions[name] = func;
  };

  return exports;
})();
