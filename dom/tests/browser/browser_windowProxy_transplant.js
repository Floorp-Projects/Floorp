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

    info("Chrome script has loaded initial URI.");
    await ContentTask.spawn(
      browser,
      { URL1, URL2, URL3 },
      async ({ URL1, URL2, URL3 }) => {
        let iframe = content.document.createElement("iframe");
        content.document.body.appendChild(iframe);

        info("Chrome script created iframe");

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
        let chromeWin1 = iframe.contentWindow;
        let chromeWin1x = Cu.waiveXrays(iframe.contentWindow);
        content.win1x = Cu.waiveXrays(iframe.contentWindow);

        ok(chromeWin1 != chromeWin1x, "waiving xrays creates a new thing?");

        content.bc1 = iframe.browsingContext;

        is(
          content.bc0,
          content.bc1,
          "same to same-origin BrowsingContext match"
        );
        is(content.win0, content.win1, "same to same-origin WindowProxy match");

        ok(
          !Cu.isDeadWrapper(content.win1),
          "win1 shouldn't be a dead wrapper before navigation"
        );
        ok(
          !Cu.isDeadWrapper(chromeWin1),
          "chromeWin1 shouldn't be a dead wrapper before navigation"
        );

        askLoad(URL2);
        await waitLoad();

        content.win2 = iframe.contentWindow;
        content.bc2 = iframe.browsingContext;

        // When chrome accesses a remote window proxy in content, the result
        // should be a remote outer window proxy in the chrome compartment, not an
        // Xray wrapper around the content remote window proxy. The former will
        // throw a security error, because @@toPrimitive can't be called cross
        // process, while the latter will result in an opaque wrapper, because
        // XPConnect doesn't know what to do when trying to create an Xray wrapper
        // around a remote outer window proxy. See bug 1556845.
        Assert.throws(
          () => {
            dump("content.win1 " + content.win1 + "\n");
          },
          /SecurityError: Permission denied to access property Symbol.toPrimitive on cross-origin object/,
          "Should get a remote outer window proxy when accessing old window proxy"
        );
        Assert.throws(
          () => {
            dump("content.win2 " + content.win2 + "\n");
          },
          /SecurityError: Permission denied to access property Symbol.toPrimitive on cross-origin object/,
          "Should get a remote outer window proxy when accessing new window proxy"
        );

        // If we fail to transplant existing non-remote outer window proxies, then
        // after we navigate the iframe existing chrome references to the window will
        // become dead wrappers. Also check content.win1 for thoroughness, though
        // we don't nuke content-content references.
        ok(
          !Cu.isDeadWrapper(content.win1),
          "win1 shouldn't be a dead wrapper after navigation"
        );
        ok(
          !Cu.isDeadWrapper(chromeWin1),
          "chromeWin1 shouldn't be a dead wrapper after navigation"
        );
        ok(
          Cu.isDeadWrapper(chromeWin1x),
          "chromeWin1x should be a dead wrapper after navigation"
        );
        ok(
          Cu.isDeadWrapper(content.win1x),
          "content.win1x should be a dead wrapper after navigation"
        );

        is(
          content.bc1,
          content.bc2,
          "same to cross-origin navigation BrowsingContext match"
        );
        is(
          content.win1,
          content.win2,
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
        is(
          content.win2,
          content.win3,
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
        is(
          content.win3,
          content.win4,
          "cross to same-origin navigation WindowProxy match"
        );
      }
    );
  } finally {
    await BrowserTestUtils.closeWindow(win);
  }
});
