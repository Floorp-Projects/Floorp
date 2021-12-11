"use strict";

let gExpectedHeader = {};

function checkSecFetchUser(subject, topic, data) {
  let channel = subject.QueryInterface(Ci.nsIHttpChannel);
  if (!channel.URI.spec.startsWith("https://example.com")) {
    return;
  }

  info(`testing headers for load of ${channel.URI.spec}`);

  const secFetchHeaders = [
    "sec-fetch-mode",
    "sec-fetch-dest",
    "sec-fetch-user",
    "sec-fetch-site",
  ];

  secFetchHeaders.forEach(header => {
    const expectedValue = gExpectedHeader[header];
    try {
      is(
        channel.getRequestHeader(header),
        expectedValue,
        `${header} is set to ${expectedValue}`
      );
    } catch (e) {
      if (expectedValue) {
        ok(false, "required headers are set");
      } else {
        ok(true, `${header} should not be set`);
      }
    }
  });
}

add_task(async function external_load() {
  waitForExplicitFinish();

  gExpectedHeader = {
    "sec-fetch-site": "none",
    "sec-fetch-mode": "navigate",
    "sec-fetch-dest": "document",
    "sec-fetch-user": "?1",
  };

  Services.obs.addObserver(checkSecFetchUser, "http-on-stop-request");

  let loaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  // Simulate an external load with Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL and
  // the system principal.
  window.browserDOMWindow.openURI(
    makeURI("https://example.com"),
    null,
    Ci.nsIBrowserDOMWindow.OPEN_CURRENTWINDOW,
    Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL,
    Services.scriptSecurityManager.getSystemPrincipal()
  );
  await loaded;

  Services.obs.removeObserver(checkSecFetchUser, "http-on-stop-request");
  finish();
});
