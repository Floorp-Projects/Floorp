/* eslint-env mozilla/chrome-script */

let consoleListener;

function ConsoleListener() {
  Services.console.registerListener(this);
}

ConsoleListener.prototype = {
  callbacks: [],

  observe: aMsg => {
    if (!(aMsg instanceof Ci.nsIScriptError)) {
      return;
    }

    let msg = {
      cssSelectors: aMsg.cssSelectors,
      errorMessage: aMsg.errorMessage,
      sourceName: aMsg.sourceName,
      sourceLine: aMsg.sourceLine,
      lineNumber: aMsg.lineNumber,
      columnNumber: aMsg.columnNumber,
      category: aMsg.category,
      windowID: aMsg.outerWindowID,
      innerWindowID: aMsg.innerWindowID,
      isScriptError: true,
      isWarning: (aMsg.flags & Ci.nsIScriptError.warningFlag) === 1,
    };

    sendAsyncMessage("monitor", msg);
  },
};

addMessageListener("load", function(e) {
  consoleListener = new ConsoleListener();
  sendAsyncMessage("ready", {});
});

addMessageListener("unload", function(e) {
  Services.console.unregisterListener(consoleListener);
  consoleListener = null;
  sendAsyncMessage("unloaded", {});
});
