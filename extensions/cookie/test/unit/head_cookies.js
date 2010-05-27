/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components.utils.import("resource://gre/modules/Services.jsm");

const _Cc = Components.classes;
const _Ci = Components.interfaces;

// Ensure 'db' is closed; if not, spin until it is.
function do_wait_for_db_close(db) {
  const ProfD = do_get_profile().QueryInterface(_Ci.nsILocalFile);

  let dbfile = ProfD.clone();
  dbfile.append(db);

  // If there is no database then it cannot be locked.
  if (!dbfile.exists())
    return;

  let thr = _Cc["@mozilla.org/thread-manager;1"].
            getService(_Ci.nsIThreadManager).
            mainThread;

  // Wait until we can open a connection to the database
  let db = null;
  while (!db) {
    // Poll for database
    try {
      db = Services.storage.openUnsharedDatabase(dbfile);
    }
    catch (e) {
      if (thr.hasPendingEvents())
        thr.processNextEvent(false);
    }
  }

  // Wait until we can write to the database
  while (db) {
    // Poll for write access
    try {
      db.executeSimpleSQL("CREATE TABLE moz_test (id INTEGER PRIMARY KEY)");
      db.executeSimpleSQL("DROP TABLE moz_test");
      db.close();
      db = null;
    }
    catch (e) {
      if (thr.hasPendingEvents())
        thr.processNextEvent(false);
    }
  }
}

// Reload the profile by calling the 'observe' method on the given service.
function do_reload_profile(observer, cleanse) {
  observer.observe(null, "profile-before-change", cleanse ? cleanse : "");
  do_wait_for_db_close("cookies.sqlite");
  observer.observe(null, "profile-do-change", "");
}

// Set four cookies; with & without channel, http and non-http; and test
// the cookie count against 'expected' after each set.
function do_set_cookies(uri, channel, session, expected) {
  const cs = _Cc["@mozilla.org/cookieService;1"].getService(_Ci.nsICookieService);

  var suffix = session ? "" : "; max-age=1000";

  // without channel
  cs.setCookieString(uri, null, "oh=hai" + suffix, null);
  do_check_eq(cs.countCookiesFromHost(uri.host), expected[0]);
  // with channel
  cs.setCookieString(uri, null, "can=has" + suffix, channel);
  do_check_eq(cs.countCookiesFromHost(uri.host), expected[1]);
  // without channel, from http
  cs.setCookieStringFromHttp(uri, null, null, "cheez=burger" + suffix, null, null);
  do_check_eq(cs.countCookiesFromHost(uri.host), expected[2]);
  // with channel, from http
  cs.setCookieStringFromHttp(uri, null, null, "hot=dog" + suffix, null, channel);
  do_check_eq(cs.countCookiesFromHost(uri.host), expected[3]);
}

