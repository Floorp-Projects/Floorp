/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that network requests from chrome don't cause the Web Console to
// throw exceptions. See Bug 597136.

"use strict";

const TEST_URI = "http://example.com/";

add_task(async function() {
  // Start a listener on the console service.
  let good = true;
  const listener = {
    QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
    observe(subject) {
      if (
        subject instanceof Ci.nsIScriptError &&
        subject.category === "XPConnect JavaScript" &&
        subject.sourceName.includes("webconsole")
      ) {
        good = false;
      }
    },
  };
  Services.console.registerListener(listener);

  // trigger a lazy-load of the HUD Service
  BrowserConsoleManager;

  await sendRequestFromChrome();

  ok(
    good,
    "No exception was thrown when sending a network request from a chrome window"
  );

  Services.console.unregisterListener(listener);
});

function sendRequestFromChrome() {
  return new Promise(resolve => {
    const xhr = new XMLHttpRequest();

    xhr.addEventListener(
      "load",
      () => {
        window.setTimeout(resolve, 0);
      },
      { once: true }
    );

    xhr.open("GET", TEST_URI, true);
    xhr.send(null);
  });
}
