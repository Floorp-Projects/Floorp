/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TOPIC_BROWSERGLUE_TEST = "browser-glue-test";
const TOPICDATA_BROWSERGLUE_TEST = "force-ui-migration";
const UI_VERSION = 138;

const gBrowserGlue = Cc["@mozilla.org/browser/browserglue;1"].getService(
  Ci.nsIObserver
);

function checkConstraint(state, origin, type) {
  Assert.equal(
    state,
    Services.perms.testExactPermissionFromPrincipal(
      Services.scriptSecurityManager.createContentPrincipalFromOrigin(origin),
      type
    ),
    `${origin} of type ${type} was set to: ${state}`
  );
}

// Test to check if migration resets default permissions properly.
add_task(async function test_resettingDefaults() {
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.migration.version");
    Services.perms.removeAll();
  });

  Services.perms.removeAll();
  Services.prefs.setIntPref("browser.migration.version", UI_VERSION);

  let pm = Services.perms;

  // Origin infos for default permissions in the format [origin, type].
  let originInfos = [
    ["https://www.mozilla.org", "uitour"],
    ["https://support.mozilla.org", "uitour"],
    ["about:home", "uitour"],
    ["about:newtab", "uitour"],
    ["https://addons.mozilla.org", "install"],
    ["https://support.mozilla.org", "remote-troubleshooting"],
    ["about:welcome", "autoplay-media"],
  ];

  // Override all default permissions.
  for (let originInfo of originInfos) {
    pm.addFromPrincipal(
      Services.scriptSecurityManager.createContentPrincipalFromOrigin(
        originInfo[0]
      ),
      originInfo[1],
      pm.UNKNOWN_ACTION
    );
  }

  // Check if the default permissions were set to UNKNOWN_ACTION.
  for (let originInfo of originInfos) {
    checkConstraint(pm.UNKNOWN_ACTION, originInfo[0], originInfo[1]);
  }

  // Simulate a migration.
  gBrowserGlue.observe(
    null,
    TOPIC_BROWSERGLUE_TEST,
    TOPICDATA_BROWSERGLUE_TEST
  );

  // Check if the default permissions were reset.
  for (let originInfo of originInfos) {
    checkConstraint(pm.ALLOW_ACTION, originInfo[0], originInfo[1]);
  }
});

// Test to check if user set permissions don't get
// reset during migration.
add_task(async function test_resettingDenyAction() {
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.migration.version");
    Services.perms.removeAll();
  });

  Services.perms.removeAll();
  Services.prefs.setIntPref("browser.migration.version", UI_VERSION);

  let pm = Services.perms;
  // Reset one default perm to DENY_ACTION.
  const origin = "https://www.mozilla.org";
  const type = "uitour";

  pm.addFromPrincipal(
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(origin),
    type,
    pm.DENY_ACTION
  );

  // Check if permission was set correctly.
  checkConstraint(pm.DENY_ACTION, origin, type);

  // Simulate a migration.
  gBrowserGlue.observe(
    null,
    TOPIC_BROWSERGLUE_TEST,
    TOPICDATA_BROWSERGLUE_TEST
  );

  // We expect the permission to remain unchanged.
  checkConstraint(pm.DENY_ACTION, origin, type);
});
