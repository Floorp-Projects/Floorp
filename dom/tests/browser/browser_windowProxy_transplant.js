"use strict";

const DIRPATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  ""
);
const PATH = DIRPATH + "file_postMessage_parent.html";

const URL1 = `http://mochi.test:8888/${PATH}`;
const URL2 = `http://example.com/${PATH}`;
const URL3 = `http://example.org/${PATH}`;

// A bunch of boilerplate which needs to be dealt with.
add_task(async function() {
  // Turn on BC preservation and frameloader rebuilding to ensure that the
  // BrowsingContext is preserved.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["fission.preserve_browsing_contexts", true],
      ["fission.rebuild_frameloaders_on_remoteness_change", true],
      ["browser.tabs.remote.useHTTPResponseProcessSelection", true],
    ],
  });

  // Open a window with fission force-enabled in it.
  let win = await BrowserTestUtils.openNewBrowserWindow({
    fission: true,
    remote: true,
  });
  try {
    // Get the tab & browser to perform the test in.
    let tab = win.gBrowser.selectedTab;
    let browser = tab.linkedBrowser;

    // Start loading the original URI, then wait until it is loaded.
    BrowserTestUtils.loadURI(browser, URL1);
    await BrowserTestUtils.browserLoaded(browser, false, URL1);

    info("Browser has loaded initial URI.");
    await ContentTask.spawn(
      browser,
      { URL1, URL2, URL3 },
      async ({ URL1, URL2, URL3 }) => {
        let iframe = content.document.createElement("iframe");
        content.document.body.appendChild(iframe);

        info("iframe created");

        // Here and below, we have to store references to things in the
        // iframes on the content window, because all chrome references
        // to content will be turned into dead wrappers when the iframes
        // are closed.
        content.win0 = iframe.contentWindow;
        content.bc0 = iframe.browsingContext;

        ok(
          !Cu.isDeadWrapper(content.win0),
          "win0 shouldn't be a dead wrapper before navigation"
        );

        // Helper for waiting for a load.
        function waitLoad() {
          return new Promise(resolve => {
            iframe.addEventListener(
              "load",
              event => {
                info("Got an iframe load event!");
                resolve();
              },
              { once: true }
            );
          });
        }

        function askLoad(url) {
          info("Chrome script asking for load of " + url);
          iframe.contentWindow.postMessage(
            {
              action: "navigate",
              location: url,
            },
            "*"
          );
          info("Chrome script done calling PostMessage");
        }

        // Check that BC and WindowProxy are preserved across navigations.
        iframe.contentWindow.location = URL1;
        await waitLoad();

        content.win1 = iframe.contentWindow;
        content.bc1 = iframe.browsingContext;

        is(
          content.bc0,
          content.bc1,
          "same to same-origin BrowsingContext match"
        );
        ok(
          content.win0 == content.win1,
          "same to same-origin WindowProxy match"
        );

        ok(
          !Cu.isDeadWrapper(content.win1),
          "win1 shouldn't be a dead wrapper before navigation"
        );

        askLoad(URL2);
        await waitLoad();

        content.win2 = iframe.contentWindow;
        content.bc2 = iframe.browsingContext;

        is(
          content.bc1,
          content.bc2,
          "same to cross-origin navigation BrowsingContext match"
        );
        todo(
          content.win1 == content.win2,
          "same to cross-origin navigation WindowProxy match"
        );

        ok(
          !Cu.isDeadWrapper(content.win1),
          "win1 shouldn't be a dead wrapper after navigation"
        );

        askLoad(URL3);
        await waitLoad();

        content.win3 = iframe.contentWindow;
        content.bc3 = iframe.browsingContext;

        is(
          content.bc2,
          content.bc3,
          "cross to cross-origin navigation BrowsingContext match"
        );
        ok(
          content.win2 == content.win3,
          "cross to cross-origin navigation WindowProxy match"
        );

        askLoad(URL1);
        await waitLoad();

        content.win4 = iframe.contentWindow;
        content.bc4 = iframe.browsingContext;

        is(
          content.bc3,
          content.bc4,
          "cross to same-origin navigation BrowsingContext match"
        );
        todo(
          content.win3 == content.win4,
          "cross to same-origin navigation WindowProxty match"
        );
      }
    );
  } finally {
    await BrowserTestUtils.closeWindow(win);
  }
});
