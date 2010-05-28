/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components.utils.import("resource://gre/modules/Services.jsm");

const _Cc = Components.classes;
const _Ci = Components.interfaces;

// Ensure 'db' is closed; if not, spin until it is.
function do_wait_for_db_close(dbfile) {
  // If there is no database then it cannot be locked.
  if (!dbfile.exists())
    return;

  let thr = _Cc["@mozilla.org/thread-manager;1"].
            getService(_Ci.nsIThreadManager).
            mainThread;

  // Wait until we can write to the database
  while (true) {
    // Poll for write access
    try {
      db = Services.storage.openUnsharedDatabase(dbfile);
      db.schemaVersion = 0;
      db.schemaVersion = 2;
      db.close();
      db = null;
      break;
    }
    catch (e) {
      if (thr.hasPendingEvents())
        thr.processNextEvent(false);
    }
  }
}

// Reload the profile by calling the 'observe' method on the given service.
function do_reload_profile(profile, observer, cleanse) {
  let dbfile = profile.QueryInterface(_Ci.nsILocalFile).clone();
  dbfile.append("cookies.sqlite");

  observer.observe(null, "profile-before-change", cleanse ? cleanse : "");
  do_wait_for_db_close(dbfile);
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

