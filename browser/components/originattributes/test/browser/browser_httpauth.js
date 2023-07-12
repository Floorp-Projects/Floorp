let { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

let authPromptModalType = Services.prefs.getIntPref(
  "prompts.modalType.httpAuth"
);

let commonDialogEnabled =
  authPromptModalType === Services.prompt.MODAL_TYPE_WINDOW ||
  (authPromptModalType === Services.prompt.MODAL_TYPE_TAB &&
    Services.prefs.getBoolPref("prompts.tabChromePromptSubDialog"));

let server = new HttpServer();
server.registerPathHandler("/file.html", fileHandler);
server.start(-1);

let BASE_URI = "http://localhost:" + server.identity.primaryPort;
let FILE_URI = BASE_URI + "/file.html";

let credentialQueue = [];

// Ask the user agent for authorization.
function fileHandler(metadata, response) {
  if (!metadata.hasHeader("Authorization")) {
    response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
    response.setHeader("WWW-Authenticate", 'Basic realm="User Visible Realm"');
    return;
  }

  // This will be "account:password" encoded in base64.
  credentialQueue.push(metadata.getHeader("Authorization"));

  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  let body = "<html><body></body></html>";
  response.bodyOutputStream.write(body, body.length);
}

function onCommonDialogLoaded(subject) {
  let dialog;
  if (commonDialogEnabled) {
    dialog = subject.Dialog;
  } else {
    let promptBox =
      subject.ownerGlobal.gBrowser.selectedBrowser.tabModalPromptBox;
    dialog = promptBox.getPrompt(subject).Dialog;
  }
  // Submit random account and password
  dialog.ui.loginTextbox.setAttribute("value", Math.random());
  dialog.ui.password1Textbox.setAttribute("value", Math.random());
  dialog.ui.button0.click();
}

let authPromptTopic = commonDialogEnabled
  ? "common-dialog-loaded"
  : "tabmodal-dialog-loaded";
Services.obs.addObserver(onCommonDialogLoaded, authPromptTopic);

registerCleanupFunction(() => {
  Services.obs.removeObserver(onCommonDialogLoaded, authPromptTopic);
  server.stop(() => {
    server = null;
  });
});

function getResult() {
  // If two targets are isolated, they should get different credentials.
  // Otherwise, the credentials will be cached and therefore the same.
  return credentialQueue.shift();
}

async function doInit(aMode) {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.partition.network_state", false]],
  });
}

IsolationTestTools.runTests(FILE_URI, getResult, null, doInit);
