/* eslint-env mozilla/chrome-script */

let dialogObserverTopic = "common-dialog-loaded";

function dialogObserver(subj) {
  subj.document.querySelector("dialog").acceptDialog();
  sendAsyncMessage("promptAccepted");
}

addMessageListener("init", () => {
  Services.obs.addObserver(dialogObserver, dialogObserverTopic);
  sendAsyncMessage("initDone");
});

addMessageListener("cleanup", () => {
  Services.obs.removeObserver(dialogObserver, dialogObserverTopic);
  sendAsyncMessage("cleanupDone");
});
