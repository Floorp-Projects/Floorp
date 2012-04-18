/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/PlacesUtils.jsm");
Components.utils.import("resource://gre/modules/NetUtil.jsm");

const ABOUT_PERMISSIONS_SPEC = "about:permissions";

const TEST_URI_1 = NetUtil.newURI("http://mozilla.com/");
const TEST_URI_2 = NetUtil.newURI("http://mozilla.org/");

// values from DefaultPermissions object
const PERM_UNKNOWN = 0;
const PERM_ALLOW = 1;
const PERM_DENY = 2;
const PERM_SESION = 8;

// used to set permissions on test sites
const TEST_PERMS = {
  "password": PERM_ALLOW,
  "cookie": PERM_ALLOW,
  "geo": PERM_UNKNOWN,
  "indexedDB": PERM_UNKNOWN,
  "popup": PERM_DENY,
  "plugins" : PERM_ALLOW
};

const NO_GLOBAL_ALLOW = [
  "geo",
  "indexedDB"
];

// number of managed permissions in the interface
const TEST_PERMS_COUNT = 6;

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(cleanUp);

  // add test history visit
  PlacesUtils.history.addVisit(TEST_URI_1, Date.now() * 1000, null,
    Ci.nsINavHistoryService.TRANSITION_LINK, false, 0);

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

  function observer() {
    Services.obs.removeObserver(observer, "browser-permissions-initialized", false);
    runNextTest();
  }
  Services.obs.addObserver(observer, "browser-permissions-initialized", false);

  // open about:permissions
  gBrowser.selectedTab = gBrowser.addTab("about:permissions");
}

function cleanUp() {
  for (let type in TEST_PERMS) {
    if (type != "password") {
      Services.perms.remove(TEST_URI_1.host, type);
      Services.perms.remove(TEST_URI_2.host, type);
    }
  }

  gBrowser.removeTab(gBrowser.selectedTab);
}

function runNextTest() {
  if (gTestIndex == tests.length) {
    waitForClearHistory(finish);
    return;
  }

  let nextTest = tests[gTestIndex++];
  info("[" + nextTest.name + "] running test");
  nextTest();
}

var gSitesList;
var gHeaderDeck;
var gSiteLabel;

var gTestIndex = 0;
var tests = [
  function test_page_load() {
    is(gBrowser.currentURI.spec, ABOUT_PERMISSIONS_SPEC, "about:permissions loaded");

    gSitesList = gBrowser.contentDocument.getElementById("sites-list");
    ok(gSitesList, "got sites list");

    gHeaderDeck = gBrowser.contentDocument.getElementById("header-deck");
    ok(gHeaderDeck, "got header deck");

    gSiteLabel = gBrowser.contentDocument.getElementById("site-label");
    ok(gSiteLabel, "got site label");

    runNextTest();
  },

  function test_sites_list() {
    is(gSitesList.firstChild.id, "all-sites-item",
       "all sites is the first item in the sites list");

    ok(getSiteItem(TEST_URI_1.host), "site item from places db exists");
    ok(getSiteItem(TEST_URI_2.host), "site item from enumerating services exists");

    runNextTest();
  },

  function test_filter_sites_list() {
    // set filter to test host
    let sitesFilter = gBrowser.contentDocument.getElementById("sites-filter");
    sitesFilter.value = TEST_URI_1.host;
    sitesFilter.doCommand();

    // make sure correct sites are collapsed/showing
    let testSite1 = getSiteItem(TEST_URI_1.host);
    ok(!testSite1.collapsed, "test site 1 is not collapsed");
    let testSite2 = getSiteItem(TEST_URI_2.host);
    ok(testSite2.collapsed, "test site 2 is collapsed");

    // clear filter
    sitesFilter.value = "";
    sitesFilter.doCommand();

    runNextTest();
  },

  function test_all_sites() {
    // "All Sites" item should be selected when the page is first loaded
    is(gSitesList.selectedItem, gBrowser.contentDocument.getElementById("all-sites-item"),
       "all sites item is selected");

    let defaultsHeader = gBrowser.contentDocument.getElementById("defaults-header");
    is(defaultsHeader, gHeaderDeck.selectedPanel,
       "correct header shown for all sites");

    ok(gBrowser.contentDocument.getElementById("passwords-count").hidden,
       "passwords count is hidden");
    ok(gBrowser.contentDocument.getElementById("cookies-count").hidden,
       "cookies count is hidden");

    // Test to make sure "Allow" items hidden for certain permission types
    NO_GLOBAL_ALLOW.forEach(function(aType) {
      let menuitem = gBrowser.contentDocument.getElementById(aType + "-" + PERM_ALLOW);
      ok(menuitem.hidden, aType + " allow menuitem hidden for all sites");
    });

    runNextTest();
  },

  function test_all_sites_permission() {
    // there should be no user-set pref for cookie behavior
    is(Services.prefs.getIntPref("network.cookie.cookieBehavior"), PERM_UNKNOWN,
       "network.cookie.cookieBehavior is expected default");

    // the default behavior is to allow cookies
    let cookieMenulist = getPermissionMenulist("cookie");
    is(cookieMenulist.value, PERM_ALLOW,
       "menulist correctly shows that cookies are allowed");

    // set the pref to block cookies
    Services.prefs.setIntPref("network.cookie.cookieBehavior", PERM_DENY);
    // check to make sure this change is reflected in the UI
    is(cookieMenulist.value, PERM_DENY, "menulist correctly shows that cookies are blocked");

    // clear the pref
    Services.prefs.clearUserPref("network.cookie.cookieBehavior");

    runNextTest();
  },

  function test_manage_all_passwords() {
    // make sure "Manage All Passwords..." button opens the correct dialog
    addWindowListener("chrome://passwordmgr/content/passwordManager.xul", runNextTest);
    gBrowser.contentDocument.getElementById("passwords-manage-all-button").doCommand();
    
  },

  function test_manage_all_cookies() {
    // make sure "Manage All Cookies..." button opens the correct dialog
    addWindowListener("chrome://browser/content/preferences/cookies.xul", runNextTest);    
    gBrowser.contentDocument.getElementById("cookies-manage-all-button").doCommand();
  },

  function test_select_site() {
    // select the site that has the permissions we set at the beginning of the test
    let testSiteItem = getSiteItem(TEST_URI_2.host);
    gSitesList.selectedItem = testSiteItem;

    let siteHeader = gBrowser.contentDocument.getElementById("site-header");
    is(siteHeader, gHeaderDeck.selectedPanel,
       "correct header shown for a specific site");
    is(gSiteLabel.value, TEST_URI_2.host, "header updated for selected site");

    ok(!gBrowser.contentDocument.getElementById("passwords-count").hidden,
       "passwords count is not hidden");
    ok(!gBrowser.contentDocument.getElementById("cookies-count").hidden,
       "cookies count is not hidden");

    // Test to make sure "Allow" items are *not* hidden for certain permission types
    NO_GLOBAL_ALLOW.forEach(function(aType) {
      let menuitem = gBrowser.contentDocument.getElementById(aType + "-" + PERM_ALLOW);
      ok(!menuitem.hidden, aType  + " allow menuitem not hidden for single site");
    });

    runNextTest();
  },

  function test_permissions() {
    let menulists = gBrowser.contentDocument.getElementsByClassName("pref-menulist");
    is(menulists.length, TEST_PERMS_COUNT, "got expected number of managed permissions");

    for (let i = 0; i < menulists.length; i++) {
      let permissionMenulist = menulists.item(i);
      let permissionType = permissionMenulist.getAttribute("type");

      // permissions should reflect what we set at the beginning of the test
      is(permissionMenulist.value, TEST_PERMS[permissionType],
        "got expected value for " + permissionType + " permission");
    }

    runNextTest();
  },

  function test_permission_change() {
    let geoMenulist = getPermissionMenulist("geo");
    is(geoMenulist.value, PERM_UNKNOWN, "menulist correctly shows that geolocation permission is unspecified");

    // change a permission programatically
    Services.perms.add(TEST_URI_2, "geo", PERM_DENY);
    // check to make sure this change is reflected in the UI
    is(geoMenulist.value, PERM_DENY, "menulist shows that geolocation is blocked");

    // change a permisssion in the UI
    let geoAllowItem = gBrowser.contentDocument.getElementById("geo-" + PERM_ALLOW);
    geoMenulist.selectedItem = geoAllowItem;
    geoMenulist.doCommand();
    // check to make sure this change is reflected in the permission manager
    is(Services.perms.testPermission(TEST_URI_2, "geo"), PERM_ALLOW,
       "permission manager shows that geolocation is allowed");

    runNextTest();
  },

  function test_forget_site() {
    // click "Forget About This Site" button
    gBrowser.contentDocument.getElementById("forget-site-button").doCommand();

    is(gSiteLabel.value, "", "site label cleared");

    let allSitesItem = gBrowser.contentDocument.getElementById("all-sites-item");
    is(gSitesList.selectedItem, allSitesItem,
       "all sites item selected after forgetting selected site");

    // check to make sure site is gone from sites list
    let testSiteItem = getSiteItem(TEST_URI_2.host);
    ok(!testSiteItem, "site removed from sites list");

    // check to make sure we forgot all permissions corresponding to site
    for (let type in TEST_PERMS) {
      if (type == "password") {
        ok(Services.logins.getLoginSavingEnabled(TEST_URI_2.prePath),
           "password saving should be enabled by default");
      } else {
        is(Services.perms.testPermission(TEST_URI_2, type), PERM_UNKNOWN,
           type + " permission should not be set for test site 2");
      }
    }

    runNextTest();
  }
];

function getPermissionMenulist(aType) {
  return gBrowser.contentDocument.getElementById(aType + "-menulist");
}

function getSiteItem(aHost) {
  return gBrowser.contentDocument.
                  querySelector(".site[value='" + aHost + "']");
}

function addWindowListener(aURL, aCallback) {
  Services.wm.addListener({
    onOpenWindow: function(aXULWindow) {
      info("window opened, waiting for focus");
      Services.wm.removeListener(this);

      var domwindow = aXULWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIDOMWindow);
      waitForFocus(function() {
        is(domwindow.document.location.href, aURL, "should have seen the right window open");
        domwindow.close();
        aCallback();
      }, domwindow);
    },
    onCloseWindow: function(aXULWindow) { },
    onWindowTitleChange: function(aXULWindow, aNewTitle) { }
  });
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
