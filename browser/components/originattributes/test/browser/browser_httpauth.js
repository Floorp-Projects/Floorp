let Cu = Components.utils;
let {HttpServer} = Cu.import("resource://testing-common/httpd.js", {});

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
    response.setHeader("WWW-Authenticate", "Basic realm=\"User Visible Realm\"");
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
  // Submit random account and password
  let dialog = subject.Dialog;
  dialog.ui.loginTextbox.setAttribute("value", Math.random());
  dialog.ui.password1Textbox.setAttribute("value", Math.random());
  dialog.ui.button0.click();
}

Services.obs.addObserver(onCommonDialogLoaded, "common-dialog-loaded", false);

registerCleanupFunction(() => {
  Services.obs.removeObserver(onCommonDialogLoaded, "common-dialog-loaded");
  server.stop(() => {
    server = null;
  });
});

function getResult() {
  // If two targets are isolated, they should get different credentials.
  // Otherwise, the credentials will be cached and therefore the same.
  return credentialQueue.shift();
}

IsolationTestTools.runTests(FILE_URI, getResult);

