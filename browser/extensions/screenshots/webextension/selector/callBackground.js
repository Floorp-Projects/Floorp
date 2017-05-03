/* globals browser, log */

"use strict";

this.callBackground = function callBackground(funcName, ...args) {
  return browser.runtime.sendMessage({funcName, args}).then((result) => {
    if (result.type === "success") {
      return result.value;
    } else if (result.type === "error") {
      let exc = new Error(result.message);
      exc.name = "BackgroundError";
      if ('errorCode' in result) {
        exc.errorCode = result.errorCode;
      }
      if ('popupMessage' in result) {
        exc.popupMessage = result.popupMessage;
      }
      throw exc;
    } else {
      log.error("Unexpected background result:", result);
      let exc = new Error(`Bad response type from background page: ${result.type || undefined}`);
      exc.resultType = result.type || "undefined";
      throw exc;
    }
  });
}
null;
