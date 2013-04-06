/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/PlacesUtils.jsm");
Components.utils.import("resource://gre/modules/NetUtil.jsm");
Components.utils.import("resource://gre/modules/ForgetAboutSite.jsm");

const ABOUT_PERMISSIONS_SPEC = "about:permissions";

const TEST_URI_1 = NetUtil.newURI("http://mozilla.com/");
const TEST_URI_2 = NetUtil.newURI("http://mozilla.org/");
const TEST_URI_3 = NetUtil.newURI("http://wikipedia.org/");

// values from DefaultPermissions object
const PERM_UNKNOWN = 0;
const PERM_ALLOW = 1;
const PERM_DENY = 2;

// used to set permissions on test sites
const TEST_PERMS = {
  "password": PERM_ALLOW,
  "cookie": PERM_ALLOW,
  "geo": PERM_UNKNOWN,
  "indexedDB": PERM_UNKNOWN,
  "popup": PERM_DENY
};

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(cleanUp);
  setup(function() {
    runNextTest();
  });
}

function setup(aCallback) {
  // add test history visit
  addVisits(TEST_URI_1, function() {
    // set permissions ourselves to avoid problems with different defaults
    // from test harness configuration
    for (let type in TEST_PERMS) {
      if (type == "password") {
        Services.logins.setLoginSavingEnabled(TEST_URI_2.prePath, true);
      } else {
        // set permissions on a site without history visits to test enumerateServices
        Services.perms.add(TEST_URI_2, type, TEST_PERMS[type]);
      }
    }

    Services.perms.add(TEST_URI_3, "popup", TEST_PERMS["popup"]);
    aCallback();
  });
}

function cleanUp() {
  for (let type in TEST_PERMS) {
    if (type != "password") {
      Services.perms.remove(TEST_URI_1.host, type);
      Services.perms.remove(TEST_URI_2.host, type);
      Services.perms.remove(TEST_URI_3.host, type);
    }
  }
}

function runNextTest() {
  if (gTestIndex == tests.length) {
    waitForClearHistory(finish);
    return;
  }

  let nextTest = tests[gTestIndex++];
  info(nextTest.desc);

  function preinit_observer() {
    Services.obs.removeObserver(preinit_observer, "browser-permissions-preinit");
    nextTest.preInit();
  }
  Services.obs.addObserver(preinit_observer, "browser-permissions-preinit", false);

  function init_observer() {
    Services.obs.removeObserver(init_observer, "browser-permissions-initialized");
    nextTest.run();
  }
  Services.obs.addObserver(init_observer, "browser-permissions-initialized", false);

  // open about:permissions
  let tab = gBrowser.selectedTab = gBrowser.addTab("about:permissions");
  registerCleanupFunction(function() {
    gBrowser.removeTab(tab);
  });
}

var gSitesList;

var gTestIndex = 0;
var tests = [
  // 'preInit' occurs after opening about:permissions, before sites-list is populated
  // 'run' occurs after sites-list is populated
  {
    desc: "test filtering before sites-list is fully constructed.",
    preInit: function() {
      let sitesFilter = gBrowser.contentDocument.getElementById("sites-filter");
      sitesFilter.value = TEST_URI_2.host;
      sitesFilter.doCommand();
    },
    run: function() {
      let testSite1 = getSiteItem(TEST_URI_1.host);
      ok(testSite1.collapsed, "test site 1 is collapsed after early filtering");
      let testSite2 = getSiteItem(TEST_URI_2.host);
      ok(!testSite2.collapsed, "test site 2 is not collapsed after early filtering");
      let testSite3 = getSiteItem(TEST_URI_3.host);
      ok(testSite3.collapsed, "test site 3 is collapsed after early filtering");

      runNextTest();
    }
  },
  {
    desc: "test removing from sites-list before it is fully constructed.",
    preInit: function() {
      ForgetAboutSite.removeDataFromDomain(TEST_URI_2.host);
    },
    run: function() {
      let testSite1 = getSiteItem(TEST_URI_1.host);
      ok(!testSite2, "test site 1 was not removed from sites list");
      let testSite2 = getSiteItem(TEST_URI_2.host);
      ok(!testSite2, "test site 2 was pre-removed from sites list");
      let testSite3 = getSiteItem(TEST_URI_3.host);
      ok(!testSite2, "test site 3 was not removed from sites list");

      runNextTest();
    }
  }
];

function getSiteItem(aHost) {
  return gBrowser.contentDocument.
                  querySelector(".site[value='" + aHost + "']");
}

// copied from toolkit/components/places/tests/head_common.js
function waitForClearHistory(aCallback) {
  let observer = {
    observe: function(aSubject, aTopic, aData) {
      Services.obs.removeObserver(this, PlacesUtils.TOPIC_EXPIRATION_FINISHED);
      aCallback();
    }
  };
  Services.obs.addObserver(observer, PlacesUtils.TOPIC_EXPIRATION_FINISHED, false);
  PlacesUtils.bhistory.removeAllPages();
}
