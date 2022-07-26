/* eslint-env mozilla/chrome-script */

let dialogObserverTopic = "common-dialog-loaded";

function dialogObserver(subj, topic, data) {
  subj.document.querySelector("dialog").acceptDialog();
  sendAsyncMessage("promptAccepted");
}

addMessageListener("init", message => {
  Services.obs.addObserver(dialogObserver, dialogObserverTopic);
  sendAsyncMessage("initDone");
});

addMessageListener("cleanup", message => {
  Services.obs.removeObserver(dialogObserver, dialogObserverTopic);
  sendAsyncMessage("cleanupDone");
});
