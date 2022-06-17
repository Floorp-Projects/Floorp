/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Provide infrastructure for JSProcessActor tests.
 */

const URL = "about:blank";
const TEST_URL = "http://test2.example.org/";
let processActorOptions = {
  jsm: {
    parent: {
      moduleURI: "resource://testing-common/TestProcessActorParent.jsm",
    },
    child: {
      moduleURI: "resource://testing-common/TestProcessActorChild.jsm",
      observers: ["test-js-content-actor-child-observer"],
    },
  },
  "sys.mjs": {
    parent: {
      esModuleURI: "resource://testing-common/TestProcessActorParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource://testing-common/TestProcessActorChild.sys.mjs",
      observers: ["test-js-content-actor-child-observer"],
    },
  },
};

function promiseNotification(aNotification) {
  const { Services } = ChromeUtils.import(
    "resource://gre/modules/Services.jsm"
  );
  let notificationResolve;
  let notificationObserver = function observer() {
    notificationResolve();
    Services.obs.removeObserver(notificationObserver, aNotification);
  };
  return new Promise(resolve => {
    notificationResolve = resolve;
    Services.obs.addObserver(notificationObserver, aNotification);
  });
}

function declTest(name, cfg) {
  declTestWithOptions(name, cfg, "jsm");
  declTestWithOptions(name, cfg, "sys.mjs");
}

function declTestWithOptions(name, cfg, fileExt) {
  let { url = "about:blank", includeParent = false, remoteTypes, test } = cfg;

  // Build the actor options object which will be used to register & unregister
  // our process actor.
  let actorOptions = {
    parent: Object.assign({}, processActorOptions[fileExt].parent),
    child: Object.assign({}, processActorOptions[fileExt].child),
  };
  actorOptions.includeParent = includeParent;
  if (remoteTypes !== undefined) {
    actorOptions.remoteTypes = remoteTypes;
  }

  // Add a new task for the actor test declared here.
  add_task(async function() {
    info("Entering test: " + name);

    // Register our actor, and load a new tab with the provided URL
    ChromeUtils.registerProcessActor("TestProcessActor", actorOptions);
    try {
      await BrowserTestUtils.withNewTab(url, async browser => {
        info("browser ready");
        await Promise.resolve(test(browser, window, fileExt));
      });
    } finally {
      // Unregister the actor after the test is complete.
      ChromeUtils.unregisterProcessActor("TestProcessActor");
      info("Exiting test: " + name);
    }
  });
}
