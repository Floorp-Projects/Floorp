"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

add_task(async function() {
  let webNav = Services.appShell.createWindowlessBrowser(false);

  let docShell = webNav.docShell;

  let loadContext = docShell.QueryInterface(Ci.nsILoadContext);

  equal(
    loadContext.usePrivateBrowsing,
    false,
    "Should start out in non-private mode"
  );

  loadContext.usePrivateBrowsing = true;
  equal(
    loadContext.usePrivateBrowsing,
    true,
    "Should be able to change to private mode prior to a document load"
  );

  loadContext.usePrivateBrowsing = false;
  equal(
    loadContext.usePrivateBrowsing,
    false,
    "Should be able to change to non-private mode prior to a document load"
  );

  let oa = docShell.getOriginAttributes();

  oa.privateBrowsingId = 1;
  docShell.setOriginAttributes(oa);

  equal(
    loadContext.usePrivateBrowsing,
    true,
    "Should be able to change origin attributes prior to a document load"
  );

  oa.privateBrowsingId = 0;
  docShell.setOriginAttributes(oa);

  equal(
    loadContext.usePrivateBrowsing,
    false,
    "Should be able to change origin attributes prior to a document load"
  );

  let loadURIOptions = {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  };
  webNav.loadURI("data:text/html,", loadURIOptions);

  // Return to the event loop so the load can begin.
  await new Promise(executeSoon);

  // This causes a failed assertion rather than an exception on debug
  // builds.
  if (!AppConstants.DEBUG) {
    Assert.throws(
      () => {
        loadContext.usePrivateBrowsing = true;
      },
      /NS_ERROR_FAILURE/,
      "Should not be able to change private browsing state after initial load has started"
    );

    oa.privateBrowsingId = 1;
    Assert.throws(
      () => {
        docShell.setOriginAttributes(oa);
      },
      /NS_ERROR_FAILURE/,
      "Should not be able to change origin attributes after initial load has started"
    );

    equal(
      loadContext.usePrivateBrowsing,
      false,
      "Should not be able to change private browsing state after initial load has started"
    );

    loadContext.usePrivateBrowsing = false;
    ok(
      true,
      "Should be able to set usePrivateBrowsing to its current value even after initial load"
    );
  }

  webNav.close();
});
