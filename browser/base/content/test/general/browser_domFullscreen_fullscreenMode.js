/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

// This test tends to trigger a race in the fullscreen time telemetry,
// where the fullscreen enter and fullscreen exit events (which use the
// same histogram ID) overlap. That causes TelemetryStopwatch to log an
// error.
SimpleTest.ignoreAllUncaughtExceptions(true);

function listenOneEvent(aEvent, aListener) {
  function listener(evt) {
    removeEventListener(aEvent, listener);
    aListener(evt);
  }
  addEventListener(aEvent, listener);
}

function queryFullscreenState(browser) {
  return SpecialPowers.spawn(browser, [], () => {
    return {
      inDOMFullscreen: !!content.document.fullscreenElement,
      inFullscreen: content.fullScreen,
    };
  });
}

function captureUnexpectedFullscreenChange() {
  ok(false, "catched an unexpected fullscreen change");
}

const FS_CHANGE_DOM = 1 << 0;
const FS_CHANGE_SIZE = 1 << 1;
const FS_CHANGE_BOTH = FS_CHANGE_DOM | FS_CHANGE_SIZE;

function waitForDocActivated(aBrowser) {
  return SpecialPowers.spawn(aBrowser, [], () => {
    return ContentTaskUtils.waitForCondition(
      () => content.browsingContext.isActive && content.document.hasFocus()
    );
  });
}

function waitForFullscreenChanges(aBrowser, aFlags) {
  return new Promise(resolve => {
    let fullscreenData = null;
    let sizemodeChanged = false;
    function tryResolve() {
      if (
        (!(aFlags & FS_CHANGE_DOM) || fullscreenData) &&
        (!(aFlags & FS_CHANGE_SIZE) || sizemodeChanged)
      ) {
        // In the platforms that support reporting occlusion state (e.g. Mac),
        // enter/exit fullscreen mode will trigger docshell being set to
        // non-activate and then set to activate back again.
        // For those platform, we should wait until the docshell has been
        // activated again, otherwise, the fullscreen request might be denied.
        waitForDocActivated(aBrowser).then(() => {
          if (!fullscreenData) {
            queryFullscreenState(aBrowser).then(resolve);
          } else {
            resolve(fullscreenData);
          }
        });
      }
    }
    if (aFlags & FS_CHANGE_SIZE) {
      listenOneEvent("sizemodechange", () => {
        sizemodeChanged = true;
        tryResolve();
      });
    }
    if (aFlags & FS_CHANGE_DOM) {
      BrowserTestUtils.waitForContentEvent(aBrowser, "fullscreenchange").then(
        async () => {
          fullscreenData = await queryFullscreenState(aBrowser);
          tryResolve();
        }
      );
    }
  });
}

var gTests = [
  {
    desc: "document method",
    affectsFullscreenMode: false,
    exitFunc: browser => {
      SpecialPowers.spawn(browser, [], () => {
        content.document.exitFullscreen();
      });
    },
  },
  {
    desc: "escape key",
    affectsFullscreenMode: false,
    exitFunc: () => {
      executeSoon(() => EventUtils.synthesizeKey("KEY_Escape"));
    },
  },
  {
    desc: "F11 key",
    affectsFullscreenMode: true,
    exitFunc() {
      executeSoon(() => EventUtils.synthesizeKey("KEY_F11"));
    },
  },
];

function checkState(expectedStates, contentStates) {
  is(
    contentStates.inDOMFullscreen,
    expectedStates.inDOMFullscreen,
    "The DOM fullscreen state of the content should match"
  );
  // TODO window.fullScreen is not updated as soon as the fullscreen
  //      state flips in child process, hence checking it could cause
  //      anonying intermittent failure. As we just want to confirm the
  //      fullscreen state of the browser window, we can just check the
  //      that on the chrome window below.
  // is(contentStates.inFullscreen, expectedStates.inFullscreen,
  //    "The fullscreen state of the content should match");
  is(
    !!document.fullscreenElement,
    expectedStates.inDOMFullscreen,
    "The DOM fullscreen state of the chrome should match"
  );
  is(
    window.fullScreen,
    expectedStates.inFullscreen,
    "The fullscreen state of the chrome should match"
  );
}

const kPage =
  "http://example.org/browser/browser/" +
  "base/content/test/general/dummy_page.html";

add_task(async function() {
  await pushPrefs(
    ["full-screen-api.transition-duration.enter", "0 0"],
    ["full-screen-api.transition-duration.leave", "0 0"]
  );

  registerCleanupFunction(async function() {
    if (window.fullScreen) {
      let fullscreenPromise = waitForFullscreenChanges(
        gBrowser.selectedBrowser,
        FS_CHANGE_SIZE
      );
      executeSoon(() => BrowserFullScreen());
      await fullscreenPromise;
    }
  });

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: kPage,
  });
  let browser = tab.linkedBrowser;

  // As requestFullscreen checks the active state of the docshell,
  // wait for the document to be activated, just to be sure that
  // the fullscreen request won't be denied.
  await waitForDocActivated(browser);

  for (let test of gTests) {
    let contentStates;
    info("Testing exit DOM fullscreen via " + test.desc);

    contentStates = await queryFullscreenState(browser);
    checkState({ inDOMFullscreen: false, inFullscreen: false }, contentStates);

    /* DOM fullscreen without fullscreen mode */

    info("> Enter DOM fullscreen");
    let fullscreenPromise = waitForFullscreenChanges(browser, FS_CHANGE_BOTH);
    await SpecialPowers.spawn(browser, [], () => {
      content.document.body.requestFullscreen();
    });
    contentStates = await fullscreenPromise;
    checkState({ inDOMFullscreen: true, inFullscreen: true }, contentStates);

    info("> Exit DOM fullscreen");
    fullscreenPromise = waitForFullscreenChanges(browser, FS_CHANGE_BOTH);
    test.exitFunc(browser);
    contentStates = await fullscreenPromise;
    checkState({ inDOMFullscreen: false, inFullscreen: false }, contentStates);

    /* DOM fullscreen with fullscreen mode */

    info("> Enter fullscreen mode");
    // Need to be asynchronous because sizemodechange event could be
    // dispatched synchronously, which would cause the event listener
    // miss that event and wait infinitely.
    fullscreenPromise = waitForFullscreenChanges(browser, FS_CHANGE_SIZE);
    executeSoon(() => BrowserFullScreen());
    contentStates = await fullscreenPromise;
    checkState({ inDOMFullscreen: false, inFullscreen: true }, contentStates);

    info("> Enter DOM fullscreen in fullscreen mode");
    fullscreenPromise = waitForFullscreenChanges(browser, FS_CHANGE_DOM);
    await SpecialPowers.spawn(browser, [], () => {
      content.document.body.requestFullscreen();
    });
    contentStates = await fullscreenPromise;
    checkState({ inDOMFullscreen: true, inFullscreen: true }, contentStates);

    info("> Exit DOM fullscreen in fullscreen mode");
    fullscreenPromise = waitForFullscreenChanges(
      browser,
      test.affectsFullscreenMode ? FS_CHANGE_BOTH : FS_CHANGE_DOM
    );
    test.exitFunc(browser);
    contentStates = await fullscreenPromise;
    checkState(
      {
        inDOMFullscreen: false,
        inFullscreen: !test.affectsFullscreenMode,
      },
      contentStates
    );

    /* Cleanup */

    // Exit fullscreen mode if we are still in
    if (window.fullScreen) {
      info("> Cleanup");
      fullscreenPromise = waitForFullscreenChanges(browser, FS_CHANGE_SIZE);
      executeSoon(() => BrowserFullScreen());
      await fullscreenPromise;
    }
  }

  BrowserTestUtils.removeTab(tab);
});
