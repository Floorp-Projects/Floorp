/* eslint-env mozilla/chrome-script */

// Chrome scripts are run with synchronous messages, so make sure we're completely
// decoupled from the content process before doing this work.
Cu.dispatch(function() {
  let chromeWin = Services.ww.activeWindow;
  let contextMenu = chromeWin.document.getElementById("contentAreaContextMenu");
  var suggestion = contextMenu.querySelector(".spell-suggestion");
  suggestion.doCommand();
  contextMenu.hidePopup();
  sendAsyncMessage("spellingCorrected");
});
