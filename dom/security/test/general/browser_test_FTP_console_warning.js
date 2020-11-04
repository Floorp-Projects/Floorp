// Description of the test:
//   Ensure that FTP subresource loads trigger a warning in the webconsole.
"use strict";

function scopedCuImport(path) {
  const scope = {};
  ChromeUtils.import(path, scope);
  return scope;
}

// These files don't actually exist, we are just looking for messages
// that indicate that loading those files would have been blocked.
var seen_files = ["a.html", "b.html", "c.html", "d.png"];

function on_new_message(msgObj) {
  let text = msgObj.message;
  if (
    text.includes("Loading FTP subresource within http(s) page not allowed")
  ) {
    // Remove the file in the message from the list.
    seen_files = seen_files.filter(file => {
      return !text.includes(file);
    });
  }
}

const kTestPath = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://mochi.test:8888"
);
const kTestURI = kTestPath + "file_FTP_console_warning.html";

add_task(async function() {
  // A longer timeout is necessary for this test than the plain mochitests
  // due to opening a new tab with the web console.
  requestLongerTimeout(4);

  Services.console.registerListener(on_new_message);

  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, kTestURI);
  await BrowserTestUtils.waitForCondition(() => seen_files.length === 0);
  is(seen_files.length, 0, "All FTP subresources should be blocked");

  Services.console.unregisterListener(on_new_message);
});
