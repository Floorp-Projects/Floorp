/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Private Browsing Tests.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * Test added with bug 460086 to test the behavior of the new API that was added
 * to remove all traces of visiting a site.
 */

////////////////////////////////////////////////////////////////////////////////
//// Constants

let pb = Cc["@mozilla.org/privatebrowsing;1"].
         getService(Ci.nsIPrivateBrowsingService);

const COOKIE_EXPIRY = Math.round(Date.now() / 1000) + 60;
const COOKIE_NAME = "testcookie";
const COOKIE_PATH = "/";

const LOGIN_USERNAME = "username";
const LOGIN_PASSWORD = "password";
const LOGIN_USERNAME_FIELD = "username_field";
const LOGIN_PASSWORD_FIELD = "password_field";

const PERMISSION_TYPE = "test-perm";
const PERMISSION_VALUE = Ci.nsIPermissionManager.ALLOW_ACTION;

const PREFERENCE_NAME = "test-pref";

////////////////////////////////////////////////////////////////////////////////
//// Utility Functions

/**
 * Creates an nsIURI object for the given string representation of a URI.
 *
 * @param aURIString
 *        The spec of the URI to create.
 * @returns an nsIURI representing aURIString.
 */
function uri(aURIString)
{
  return Cc["@mozilla.org/network/io-service;1"].
         getService(Ci.nsIIOService).
         newURI(aURIString, null, null);
}

/**
 * Adds a visit to history.
 *
 * @param aURI
 *        The URI to add.
 */
function add_visit(aURI)
{
  check_visited(aURI, false);
  let bh = Cc["@mozilla.org/browser/global-history;2"].
           getService(Ci.nsIBrowserHistory);
  bh.addPageWithDetails(aURI, aURI.spec, Date.now() * 1000);
  check_visited(aURI, true);
}

/**
 * Checks to ensure a URI string is visited or not.
 *
 * @param aURI
 *        The URI to check.
 * @param aIsVisited
 *        True if the URI should be visited, false otherwise.
 */
function check_visited(aURI, aIsVisited)
{
  let gh = Cc["@mozilla.org/browser/global-history;2"].
           getService(Ci.nsIGlobalHistory2);
  let checker = aIsVisited ? do_check_true : do_check_false;
  checker(gh.isVisited(aURI));
}

/**
 * Add a cookie to the cookie service.
 *
 * @param aDomain
 */
function add_cookie(aDomain)
{
  check_cookie_exists(aDomain, false);
  let cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager2);
  cm.add(aDomain, COOKIE_PATH, COOKIE_NAME, "", false, false, false,
         COOKIE_EXPIRY);
  check_cookie_exists(aDomain, true);
}

/**
 * Checks to ensure that a cookie exists or not for a domain.
 *
 * @param aDomain
 *        The domain to check for the cookie.
 * @param aExists
 *        True if the cookie should exist, false otherwise.
 */
function check_cookie_exists(aDomain, aExists)
{
  let cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager2);
  let cookie = {
    host: aDomain,
    name: COOKIE_NAME,
    path: COOKIE_PATH
  }
  let checker = aExists ? do_check_true : do_check_false;
  checker(cm.cookieExists(cookie));
}

/**
 * Adds a download to download history.
 *
 * @param aURIString
 *        The string of the URI to add.
 * @param aIsActive
 *        If it should be set to an active state in the database.  This does not
 *        make it show up in the list of active downloads however!
 */
function add_download(aURIString, aIsActive)
{
  check_downloaded(aURIString, false);
  let db = Cc["@mozilla.org/download-manager;1"].
           getService(Ci.nsIDownloadManager).
           DBConnection;
  let stmt = db.createStatement(
    "INSERT INTO moz_downloads (source, state) " +
    "VALUES (:source, :state)"
  );
  stmt.params.source = aURIString;
  stmt.params.state = aIsActive ? Ci.nsIDownloadManager.DOWNLOAD_DOWNLOADING :
                                  Ci.nsIDownloadManager.DOWNLOAD_FINISHED;
  try {
    stmt.execute();
  }
  finally {
    stmt.finalize();
  }
  check_downloaded(aURIString, true);
}

/**
 * Checks to ensure a URI string is in download history or not.
 *
 * @param aURIString
 *        The string of the URI to check.
 * @param aIsDownloaded
 *        True if the URI should be downloaded, false otherwise.
 */
function check_downloaded(aURIString, aIsDownloaded)
{
  let db = Cc["@mozilla.org/download-manager;1"].
           getService(Ci.nsIDownloadManager).
           DBConnection;
  let stmt = db.createStatement(
    "SELECT * " +
    "FROM moz_downloads " +
    "WHERE source = :source"
  );
  stmt.params.source = aURIString;

  let checker = aIsDownloaded ? do_check_true : do_check_false;
  try {
    checker(stmt.step());
  }
  finally {
    stmt.finalize();
  }
}

/**
 * Adds a disabled host to the login manager.
 *
 * @param aHost
 *        The host to add to the list of disabled hosts.
 */
function add_disabled_host(aHost)
{
  check_disabled_host(aHost, false);
  let lm = Cc["@mozilla.org/login-manager;1"].
           getService(Ci.nsILoginManager);
  lm.setLoginSavingEnabled(aHost, false);
  check_disabled_host(aHost, true);
}

/**
 * Checks to see if a host is disabled for storing logins or not.
 *
 * @param aHost
 *        The host to check if it is disabled.
 * @param aIsDisabled
 *        True if the host should be disabled, false otherwise.
 */
function check_disabled_host(aHost, aIsDisabled)
{
  let lm = Cc["@mozilla.org/login-manager;1"].
           getService(Ci.nsILoginManager);
  let checker = aIsDisabled ? do_check_false : do_check_true;
  checker(lm.getLoginSavingEnabled(aHost));
}

/**
 * Adds a login for the specified host to the login manager.
 *
 * @param aHost
 *        The host to add the login for.
 */
function add_login(aHost)
{
  check_login_exists(aHost, false);
  let login = Cc["@mozilla.org/login-manager/loginInfo;1"].
              createInstance(Ci.nsILoginInfo);
  login.init(aHost, "", null, LOGIN_USERNAME, LOGIN_PASSWORD,
             LOGIN_USERNAME_FIELD, LOGIN_PASSWORD_FIELD);
  let lm = Cc["@mozilla.org/login-manager;1"].
           getService(Ci.nsILoginManager);
  lm.addLogin(login);
  check_login_exists(aHost, true);
}

/**
 * Checks to see if a login exists for a host.
 *
 * @param aHost
 *        The host to check for the test login.
 * @param aExists
 *        True if the login should exist, false otherwise.
 */
function check_login_exists(aHost, aExists)
{
  let lm = Cc["@mozilla.org/login-manager;1"].
           getService(Ci.nsILoginManager);
  let count = { value: 0 };
  lm.findLogins(count, aHost, "", null);
  do_check_eq(count.value, aExists ? 1 : 0);
}

/**
 * Adds a permission for the specified URI to the permission manager.
 *
 * @param aURI
 *        The URI to add the test permission for.
 */
function add_permission(aURI)
{
  check_permission_exists(aURI, false);
  let pm = Cc["@mozilla.org/permissionmanager;1"].
           getService(Ci.nsIPermissionManager);
  pm.add(aURI, PERMISSION_TYPE, PERMISSION_VALUE);
  check_permission_exists(aURI, true);
}

/**
 * Checks to see if a permission exists for the given URI.
 *
 * @param aURI
 *        The URI to check if a permission exists.
 * @param aExists
 *        True if the permission should exist, false otherwise.
 */
function check_permission_exists(aURI, aExists)
{
  let pm = Cc["@mozilla.org/permissionmanager;1"].
           getService(Ci.nsIPermissionManager);
  let perm = pm.testExactPermission(aURI, PERMISSION_TYPE);
  let checker = aExists ? do_check_eq : do_check_neq;
  checker(perm, PERMISSION_VALUE);
}

/**
 * Adds a content preference for the specified URI.
 *
 * @param aURI
 *        The URI to add a preference for.
 */
function add_preference(aURI)
{
  check_preference_exists(aURI, false);
  let cp = Cc["@mozilla.org/content-pref/service;1"].
           getService(Ci.nsIContentPrefService);
  cp.setPref(aURI, PREFERENCE_NAME, "foo");
  check_preference_exists(aURI, true);
}

/**
 * Checks to see if a preference exists for the given URI.
 *
 * @param aURI
 *        The URI to check if a preference exists.
 * @param aExists
 *        True if the permission should exist, false otherwise.
 */
function check_preference_exists(aURI, aExists)
{
  let cp = Cc["@mozilla.org/content-pref/service;1"].
           getService(Ci.nsIContentPrefService);
  let checker = aExists ? do_check_true : do_check_false;
  checker(cp.hasPref(aURI, PREFERENCE_NAME));
}

////////////////////////////////////////////////////////////////////////////////
//// Test Functions

// History
function test_history_cleared_with_direct_match()
{
  const TEST_URI = uri("http://mozilla.org/foo");
  add_visit(TEST_URI);
  pb.removeDataFromDomain("mozilla.org");
  check_visited(TEST_URI, false);
}

function test_history_cleared_with_subdomain()
{
  const TEST_URI = uri("http://www.mozilla.org/foo");
  add_visit(TEST_URI);
  pb.removeDataFromDomain("mozilla.org");
  check_visited(TEST_URI, false);
}

function test_history_not_cleared_with_uri_contains_domain()
{
  const TEST_URI = uri("http://ilovemozilla.org/foo");
  add_visit(TEST_URI);
  pb.removeDataFromDomain("mozilla.org");
  check_visited(TEST_URI, true);

  // Clear history since we left something there from this test.
  let bh = Cc["@mozilla.org/browser/global-history;2"].
           getService(Ci.nsIBrowserHistory);
  bh.removeAllPages();
}

// Cookie Service
function test_cookie_cleared_with_direct_match()
{
  const TEST_DOMAIN = "mozilla.org";
  add_cookie(TEST_DOMAIN);
  pb.removeDataFromDomain("mozilla.org");
  check_cookie_exists(TEST_DOMAIN, false);
}

function test_cookie_cleared_with_subdomain()
{
  const TEST_DOMAIN = "www.mozilla.org";
  add_cookie(TEST_DOMAIN);
  pb.removeDataFromDomain("mozilla.org");
  check_cookie_exists(TEST_DOMAIN, false);
}

function test_cookie_not_cleared_with_uri_contains_domain()
{
  const TEST_DOMAIN = "ilovemozilla.org";
  add_cookie(TEST_DOMAIN);
  pb.removeDataFromDomain("mozilla.org");
  check_cookie_exists(TEST_DOMAIN, true);
}

// Download Manager
function test_download_history_cleared_with_direct_match()
{
  const TEST_URI = "http://mozilla.org/foo";
  add_download(TEST_URI, false);
  pb.removeDataFromDomain("mozilla.org");
  check_downloaded(TEST_URI, false);
}

function test_download_history_cleared_with_subdomain()
{
  const TEST_URI = "http://www.mozilla.org/foo";
  add_download(TEST_URI, false);
  pb.removeDataFromDomain("mozilla.org");
  check_downloaded(TEST_URI, false);
}

function test_download_history_not_cleared_with_active_direct_match()
{
  // Tests that downloads marked as active in the db are not deleted from the db
  const TEST_URI = "http://mozilla.org/foo";
  add_download(TEST_URI, true);
  pb.removeDataFromDomain("mozilla.org");
  check_downloaded(TEST_URI, true);

  // Reset state
  let db = Cc["@mozilla.org/download-manager;1"].
           getService(Ci.nsIDownloadManager).
           DBConnection;
  db.executeSimpleSQL("DELETE FROM moz_downloads");
  check_downloaded(TEST_URI, false);
}

// Login Manager
function test_login_manager_disabled_hosts_cleared_with_direct_match()
{
  const TEST_HOST = "http://mozilla.org";
  add_disabled_host(TEST_HOST);
  pb.removeDataFromDomain("mozilla.org");
  check_disabled_host(TEST_HOST, false);
}

function test_login_manager_disabled_hosts_cleared_with_subdomain()
{
  const TEST_HOST = "http://www.mozilla.org";
  add_disabled_host(TEST_HOST);
  pb.removeDataFromDomain("mozilla.org");
  check_disabled_host(TEST_HOST, false);
}

function test_login_manager_disabled_hosts_not_cleared_with_uri_contains_domain()
{
  const TEST_HOST = "http://ilovemozilla.org";
  add_disabled_host(TEST_HOST);
  pb.removeDataFromDomain("mozilla.org");
  check_disabled_host(TEST_HOST, true);

  // Reset state
  let lm = Cc["@mozilla.org/login-manager;1"].
           getService(Ci.nsILoginManager);
  lm.setLoginSavingEnabled(TEST_HOST, true);
  check_disabled_host(TEST_HOST, false);
}

function test_login_manager_logins_cleared_with_direct_match()
{
  const TEST_HOST = "http://mozilla.org";
  add_login(TEST_HOST);
  pb.removeDataFromDomain("mozilla.org");
  check_login_exists(TEST_HOST, false);
}

function test_login_manager_logins_cleared_with_subdomain()
{
  const TEST_HOST = "http://www.mozilla.org";
  add_login(TEST_HOST);
  pb.removeDataFromDomain("mozilla.org");
  check_login_exists(TEST_HOST, false);
}

function tets_login_manager_logins_not_cleared_with_uri_contains_domain()
{
  const TEST_HOST = "http://ilovemozilla.org";
  add_login(TEST_HOST);
  pb.removeDataFromDomain("mozilla.org");
  check_login_exists(TEST_HOST, true);

  let lm = Cc["@mozilla.org/login-manager;1"].
           getService(Ci.nsILoginManager);
  lm.removeAllLogins();
  check_login_exists(TEST_HOST, false);
}

// Permission Manager
function test_permission_manager_cleared_with_direct_match()
{
  const TEST_URI = uri("http://mozilla.org");
  add_permission(TEST_URI);
  pb.removeDataFromDomain("mozilla.org");
  check_permission_exists(TEST_URI, false);
}

function test_permission_manager_cleared_with_subdomain()
{
  const TEST_URI = uri("http://www.mozilla.org");
  add_permission(TEST_URI);
  pb.removeDataFromDomain("mozilla.org");
  check_permission_exists(TEST_URI, false);
}

function test_permission_manager_not_cleared_with_uri_contains_domain()
{
  const TEST_URI = uri("http://ilovemozilla.org");
  add_permission(TEST_URI);
  pb.removeDataFromDomain("mozilla.org");
  check_permission_exists(TEST_URI, true);

  // Reset state
  let pm = Cc["@mozilla.org/permissionmanager;1"].
           getService(Ci.nsIPermissionManager);
  pm.removeAll();
  check_permission_exists(TEST_URI, false);
}

// Content Preferences
function test_content_preferences_cleared_with_direct_match()
{
  const TEST_URI = uri("http://mozilla.org");
  add_preference(TEST_URI);
  pb.removeDataFromDomain("mozilla.org");
  check_preference_exists(TEST_URI, false);
}

function test_content_preferences_cleared_with_subdomain()
{
  const TEST_URI = uri("http://www.mozilla.org");
  add_preference(TEST_URI);
  pb.removeDataFromDomain("mozilla.org");
  check_preference_exists(TEST_URI, false);
}

function test_content_preferecnes_not_cleared_with_uri_contains_domain()
{
  const TEST_URI = uri("http://ilovemozilla.org");
  add_preference(TEST_URI);
  pb.removeDataFromDomain("mozilla.org");
  check_preference_exists(TEST_URI, true);

  // Reset state
  let cp = Cc["@mozilla.org/content-pref/service;1"].
           getService(Ci.nsIContentPrefService);
  cp.removePref(TEST_URI, PREFERENCE_NAME);
  check_preference_exists(TEST_URI, false);
}

// Cache
function test_cache_cleared()
{
  // Because this test is asynchronous, it should be the last test
  do_check_eq(tests[tests.length - 1], arguments.callee);

  // NOTE: We could be more extensive with this test and actually add an entry
  //       to the cache, and then make sure it is gone.  However, we trust that
  //       the API is well tested, and that when we get the observer
  //       notification, we have actually cleared the cache.
  // This seems to happen asynchronously...
  let os = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);
  let observer = {
    observe: function(aSubject, aTopic, aData)
    {
      os.removeObserver(observer, "cacheservice:empty-cache");
      do_test_finished();
    }
  };
  os.addObserver(observer, "cacheservice:empty-cache", false);
  pb.removeDataFromDomain("mozilla.org");
  do_test_pending();
}

let tests = [
  // History
  test_history_cleared_with_direct_match,
  test_history_cleared_with_subdomain,
  test_history_not_cleared_with_uri_contains_domain,

  // Cookie Service
  test_cookie_cleared_with_direct_match,
  test_cookie_cleared_with_subdomain,
  test_cookie_not_cleared_with_uri_contains_domain,

  // Download Manager
  // Note: active downloads tested in test_removeDataFromDomain_activeDownloads.js
  test_download_history_cleared_with_direct_match,
  test_download_history_cleared_with_subdomain,
  test_download_history_not_cleared_with_active_direct_match,

  // Login Manager
  test_login_manager_disabled_hosts_cleared_with_direct_match,
  test_login_manager_disabled_hosts_cleared_with_subdomain,
  test_login_manager_disabled_hosts_not_cleared_with_uri_contains_domain,
  test_login_manager_logins_cleared_with_direct_match,
  test_login_manager_logins_cleared_with_subdomain,
  tets_login_manager_logins_not_cleared_with_uri_contains_domain,

  // Permission Manager
  test_permission_manager_cleared_with_direct_match,
  test_permission_manager_cleared_with_subdomain,
  test_permission_manager_not_cleared_with_uri_contains_domain,

  // Content Preferences
  test_content_preferences_cleared_with_direct_match,
  test_content_preferences_cleared_with_subdomain,
  test_content_preferecnes_not_cleared_with_uri_contains_domain,

  // Cache
  test_cache_cleared,
];

function run_test()
{
  for (let i = 0; i < tests.length; i++)
    tests[i]();
}
