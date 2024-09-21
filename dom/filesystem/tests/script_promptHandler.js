/* eslint-env mozilla/chrome-script */

let dialogObserverTopic = "common-dialog-loaded";

function waitForButtonEnabledState(button) {
  return new Promise(resolve => {
    // Check if the button is already enabled (not disabled)
    if (!button.disabled) {
      resolve();
      return;
    }

    // Create a MutationObserver instance
    let win = button.ownerGlobal;
    let { MutationObserver } = win;
    const observer = new MutationObserver(mutationsList => {
      for (const mutation of mutationsList) {
        if (
          mutation.type === "attributes" &&
          mutation.attributeName === "disabled"
        ) {
          if (!button.disabled) {
            // Resolve the promise when the button is enabled
            observer.disconnect(); // Stop observing
            resolve();
          }
        }
      }
    });

    // Start observing the button for changes to the 'disabled' attribute
    observer.observe(button, {
      attributes: true,
      attributeFilter: ["disabled"],
    });
  });
}

async function dialogObserver(subj) {
  let dialog = subj.document.querySelector("dialog");
  let acceptButton = dialog.getButton("accept");
  await waitForButtonEnabledState(acceptButton);

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
