/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals log, browser */

"use strict";

this.callBackground = function callBackground(funcName, ...args) {
  return browser.runtime.sendMessage({ funcName, args }).then(result => {
    if (result && result.type === "success") {
      return result.value;
    } else if (result && result.type === "error") {
      const exc = new Error(result.message || "Unknown error");
      exc.name = "BackgroundError";
      if ("errorCode" in result) {
        exc.errorCode = result.errorCode;
      }
      if ("popupMessage" in result) {
        exc.popupMessage = result.popupMessage;
      }
      throw exc;
    } else {
      log.error("Unexpected background result:", result);
      const exc = new Error(
        `Bad response type from background page: ${(result && result.type) ||
          undefined}`
      );
      exc.resultType = result ? result.type || "undefined" : "undefined result";
      throw exc;
    }
  });
};
null;
