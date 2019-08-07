/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* import-globals-from ../../../../../toolkit/components/extensions/test/mochitest/head_webrequest.js */
loadTestSubscript("head_webrequest.js");

ChromeUtils.import("resource://gre/modules/HiddenFrame.jsm", this);
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

SimpleTest.requestCompleteLog();

function createHiddenBrowser(url) {
  let frame = new HiddenFrame();
  return new Promise(resolve =>
    frame.get().then(subframe => {
      let doc = subframe.document;
      let browser = doc.createElementNS(XUL_NS, "browser");
      browser.setAttribute("type", "content");
      if (Services.appinfo.browserTabsRemoteAutostart) {
        browser.setAttribute("remote", "true");
      }
      browser.setAttribute("disableglobalhistory", "true");
      browser.setAttribute("src", url);

      doc.documentElement.appendChild(browser);
      resolve({ frame: frame, browser: browser });
    })
  );
}

let extension;
let dummy =
  "http://mochi.test:8888/browser/browser/components/extensions/test/browser/file_dummy.html";
let headers = {
  request: {
    add: {
      "X-WebRequest-request": "text",
      "X-WebRequest-request-binary": "binary",
    },
    modify: {
      "user-agent": "WebRequest",
    },
    remove: ["accept-encoding"],
  },
  response: {
    add: {
      "X-WebRequest-response": "text",
      "X-WebRequest-response-binary": "binary",
    },
    modify: {
      server: "WebRequest",
      "content-type": "text/html; charset=utf-8",
    },
    remove: ["connection"],
  },
};

let urls = ["http://mochi.test/browser/*"];
let events = {
  onBeforeRequest: [{ urls }, ["blocking"]],
  onBeforeSendHeaders: [{ urls }, ["blocking", "requestHeaders"]],
  onSendHeaders: [{ urls }, ["requestHeaders"]],
  onHeadersReceived: [{ urls }, ["blocking", "responseHeaders"]],
  onCompleted: [{ urls }, ["responseHeaders"]],
};

add_task(async function setup() {
  extension = makeExtension(events);
  await extension.startup();
});

add_task(async function test_newWindow() {
  let expect = {
    "file_dummy.html": {
      type: "main_frame",
      headers,
    },
  };
  // NOTE: When running solo, favicon will be loaded at some point during
  // the tests in this file, so all tests ignore it.  When running with
  // other tests in this directory, favicon gets loaded at some point before
  // we run, and we never see the request, thus it cannot be handled as part
  // of expect above.
  extension.sendMessage("set-expected", { expect, ignore: ["favicon.ico"] });
  await extension.awaitMessage("continue");

  let openedWindow = await BrowserTestUtils.openNewBrowserWindow();
  await BrowserTestUtils.openNewForegroundTab(
    openedWindow.gBrowser,
    `${dummy}?newWindow=${Math.random()}`
  );

  await extension.awaitMessage("done");
  await BrowserTestUtils.closeWindow(openedWindow);
});

add_task(async function test_newTab() {
  // again, in this window
  let expect = {
    "file_dummy.html": {
      type: "main_frame",
      headers,
    },
  };
  extension.sendMessage("set-expected", { expect, ignore: ["favicon.ico"] });
  await extension.awaitMessage("continue");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    `${dummy}?newTab=${Math.random()}`
  );

  await extension.awaitMessage("done");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_subframe() {
  let expect = {
    "file_dummy.html": {
      type: "main_frame",
      headers,
    },
  };
  // test a content subframe attached to hidden window
  extension.sendMessage("set-expected", { expect, ignore: ["favicon.ico"] });
  info("*** waiting to continue");
  await extension.awaitMessage("continue");
  info("*** creating hidden browser");
  let frameInfo = await createHiddenBrowser(
    `${dummy}?subframe=${Math.random()}`
  );
  info("*** waiting for finish");
  await extension.awaitMessage("done");
  info("*** destroying hidden browser");
  // cleanup
  frameInfo.browser.remove();
  frameInfo.frame.destroy();
});

add_task(async function teardown() {
  await extension.unload();
});
