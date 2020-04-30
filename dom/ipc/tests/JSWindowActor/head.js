/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Provide infrastructure for JSWindowActor tests.
 */

const URL = "about:blank";
const TEST_URL = "http://test2.example.org/";
let windowActorOptions = {
  parent: {
    moduleURI: "resource://testing-common/TestWindowParent.jsm",
  },
  child: {
    moduleURI: "resource://testing-common/TestWindowChild.jsm",

    events: {
      mozshowdropdown: {},
    },

    observers: ["test-js-window-actor-child-observer", "audio-playback"],
  },
};

function declTest(name, cfg) {
  let {
    url = "about:blank",
    allFrames = false,
    includeChrome = false,
    matches,
    remoteTypes,
    messageManagerGroups,
    fission,
    test,
  } = cfg;

  // Build the actor options object which will be used to register & unregister our window actor.
  let actorOptions = {
    parent: Object.assign({}, windowActorOptions.parent),
    child: Object.assign({}, windowActorOptions.child),
  };
  actorOptions.allFrames = allFrames;
  actorOptions.includeChrome = includeChrome;
  if (matches !== undefined) {
    actorOptions.matches = matches;
  }
  if (remoteTypes !== undefined) {
    actorOptions.remoteTypes = remoteTypes;
  }
  if (messageManagerGroups !== undefined) {
    actorOptions.messageManagerGroups = messageManagerGroups;
  }

  // Add a new task for the actor test declared here.
  add_task(async function() {
    info("Entering test: " + name);

    // Create a fresh window with the correct settings, and register our actor.
    let win = await BrowserTestUtils.openNewBrowserWindow({
      remote: true,
      fission,
    });
    ChromeUtils.registerWindowActor("TestWindow", actorOptions);

    // Wait for the provided URL to load in our browser
    let browser = win.gBrowser.selectedBrowser;
    BrowserTestUtils.loadURI(browser, url);
    await BrowserTestUtils.browserLoaded(browser, false, url);

    // Run the provided test
    info("browser ready");
    try {
      await Promise.resolve(test(browser, win));
    } finally {
      // Clean up after we're done.
      ChromeUtils.unregisterWindowActor("TestWindow");
      await BrowserTestUtils.closeWindow(win);
      info("Exiting test: " + name);
    }
  });
}
