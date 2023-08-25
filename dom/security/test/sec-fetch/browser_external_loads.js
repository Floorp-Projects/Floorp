"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

var gExpectedHeader = {};

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
        ok(false, `${header} should be set`);
      } else {
        ok(true, `${header} should not be set`);
      }
    }
  });
}

add_task(async function external_load() {
  waitForExplicitFinish();
  Services.obs.addObserver(checkSecFetchUser, "http-on-stop-request");

  let headersChecked = new Promise(resolve => {
    let reqStopped = async (subject, topic, data) => {
      Services.obs.removeObserver(reqStopped, "http-on-stop-request");
      resolve();
    };
    Services.obs.addObserver(reqStopped, "http-on-stop-request");
  });

  // System fetch. Shouldn't use Sec- headers for that.
  gExpectedHeader = {
    "sec-fetch-site": null,
    "sec-fetch-mode": null,
    "sec-fetch-dest": null,
    "sec-fetch-user": null,
  };
  await window.fetch(`${TEST_PATH}file_dummy_link.html?sysfetch`);
  await headersChecked;

  // Simulate an external load in the *current* window with
  // Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL and the system principal.
  gExpectedHeader = {
    "sec-fetch-site": "none",
    "sec-fetch-mode": "navigate",
    "sec-fetch-dest": "document",
    "sec-fetch-user": "?1",
  };

  let loaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  window.browserDOMWindow.openURI(
    makeURI(`${TEST_PATH}file_dummy_link.html`),
    null,
    Ci.nsIBrowserDOMWindow.OPEN_CURRENTWINDOW,
    Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL,
    Services.scriptSecurityManager.getSystemPrincipal()
  );
  await loaded;

  // Open a link in a *new* window through the context menu.
  gExpectedHeader = {
    "sec-fetch-site": "same-origin",
    "sec-fetch-mode": "navigate",
    "sec-fetch-dest": "document",
    "sec-fetch-user": "?1",
  };

  loaded = BrowserTestUtils.waitForNewWindow({
    url: `${TEST_PATH}file_dummy_link_location.html`,
  });
  BrowserTestUtils.waitForEvent(document, "popupshown", false, event => {
    document.getElementById("context-openlink").doCommand();
    event.target.hidePopup();
    return true;
  });
  BrowserTestUtils.synthesizeMouseAtCenter(
    "#dummylink",
    { type: "contextmenu", button: 2 },
    gBrowser.selectedBrowser
  );

  let win = await loaded;
  win.close();

  // Simulate an external load in a *new* window with
  // Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL and the system principal.
  gExpectedHeader = {
    "sec-fetch-site": "none",
    "sec-fetch-mode": "navigate",
    "sec-fetch-dest": "document",
    "sec-fetch-user": "?1",
  };

  loaded = BrowserTestUtils.waitForNewWindow({
    url: "https://example.com/newwindow",
  });
  window.browserDOMWindow.openURI(
    makeURI("https://example.com/newwindow"),
    null,
    Ci.nsIBrowserDOMWindow.OPEN_NEWWINDOW,
    Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL,
    Services.scriptSecurityManager.getSystemPrincipal()
  );
  win = await loaded;
  win.close();

  // Open a *new* window through window.open without user activation.
  gExpectedHeader = {
    "sec-fetch-site": "same-origin",
    "sec-fetch-mode": "navigate",
    "sec-fetch-dest": "document",
  };

  loaded = BrowserTestUtils.waitForNewWindow({
    url: "https://example.com/windowopen",
  });
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.window.open(
      "https://example.com/windowopen",
      "_blank",
      "height=500,width=500"
    );
  });
  win = await loaded;
  win.close();

  // Open a *new* window through window.open with user activation.
  gExpectedHeader = {
    "sec-fetch-site": "same-origin",
    "sec-fetch-mode": "navigate",
    "sec-fetch-dest": "document",
    "sec-fetch-user": "?1",
  };

  loaded = BrowserTestUtils.waitForNewWindow({
    url: "https://example.com/windowopen_withactivation",
  });
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.document.notifyUserGestureActivation();
    content.window.open(
      "https://example.com/windowopen_withactivation",
      "_blank",
      "height=500,width=500"
    );
    content.document.clearUserGestureActivation();
  });
  win = await loaded;
  win.close();

  Services.obs.removeObserver(checkSecFetchUser, "http-on-stop-request");
  finish();
});
