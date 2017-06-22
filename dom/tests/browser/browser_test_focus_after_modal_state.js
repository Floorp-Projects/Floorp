const TEST_URL =
  "https://example.com/browser/dom/tests/browser/focus_after_prompt.html";

function awaitAndClosePrompt() {
  return new Promise(resolve => {
    function onDialogShown(node) {
      Services.obs.removeObserver(onDialogShown, "tabmodal-dialog-loaded");
      let button = node.ui.button0;
      button.click();
      resolve();
    }
    Services.obs.addObserver(onDialogShown, "tabmodal-dialog-loaded");
  });
}

let lastMessageReceived = "";
function waitForMessage(message) {
  return new Promise((resolve, reject) => {
    messageManager.addMessageListener(message, function() {
      ok(true, "Received message: " + message);
      lastMessageReceived = message;
      resolve();
    });
  });
}

add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  let browser = tab.linkedBrowser;

  // Focus on editable iframe.
  await BrowserTestUtils.synthesizeMouseAtCenter("#edit", {}, browser);
  await ContentTask.spawn(browser, null, async function() {
    is(content.document.activeElement, content.document.getElementById("edit"),
       "Focus should be on iframe element");
  });

  await ContentTask.spawn(browser, null, async function() {
    content.document.getElementById("edit").contentDocument.addEventListener(
      "focus", function(event) {
        sendAsyncMessage("Test:FocusReceived");
      });

    content.document.getElementById("edit").contentDocument.addEventListener(
      "blur", function(event) {
        sendAsyncMessage("Test:BlurReceived");
      });
  });

  // Click on div that triggers a prompt, and then check that focus is back on
  // the editable iframe.
  let dialogShown = awaitAndClosePrompt();
  let waitForBlur = waitForMessage("Test:BlurReceived");
  let waitForFocus = waitForMessage("Test:FocusReceived");
  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    let div = content.document.getElementById("clickMeDiv");
    div.click();
  });
  await dialogShown;
  await Promise.all([waitForBlur, waitForFocus]);
  await ContentTask.spawn(browser, null, async function() {
    is(content.document.activeElement, content.document.getElementById("edit"),
       "Focus should be back on iframe element");
  });
  is(lastMessageReceived, "Test:FocusReceived",
     "Should receive blur and then focus event");

  await BrowserTestUtils.removeTab(tab);
});
