/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/NetUtil.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

XPCOMUtils.defineLazyServiceGetter(Services, "cookies",
                                   "@mozilla.org/cookieService;1",
                                   "nsICookieService");
XPCOMUtils.defineLazyServiceGetter(Services, "cookiemgr",
                                   "@mozilla.org/cookiemanager;1",
                                   "nsICookieManager2");

// Close and reload the cookie database.
function do_reload_profile(generator, profile, cleanse) {
  function _observer(generator, service, topic) {
    Services.obs.addObserver(this, topic, false);

    this.service = service;
    this.generator = generator;
    this.topic = topic;
  }

  _observer.prototype = {
    observe: function (subject, topic, data) {
      do_check_eq(this.topic, topic);

      Services.obs.removeObserver(this, this.topic);

      // Fire the notification to reload the database, and continue executing
      // the generator function.
      this.service.observe(null, "profile-do-change", "");
      this.generator.next();

      this.generator = null;
      this.service = null;
      this.topic = null;
    }
  }

  let dbfile = profile.QueryInterface(Ci.nsILocalFile).clone();
  dbfile.append("cookies.sqlite");

  // Register an observer for db close.
  let service = Services.cookies.QueryInterface(Ci.nsIObserver);
  let obs = new _observer(generator, service, "cookie-db-closed");

  // Close the db.
  service.observe(null, "profile-before-change", cleanse ? cleanse : "");
}

// Set four cookies; with & without channel, http and non-http; and test
// the cookie count against 'expected' after each set.
function do_set_cookies(uri, channel, session, expected) {
  let suffix = session ? "" : "; max-age=1000";

  // without channel
  Services.cookies.setCookieString(uri, null, "oh=hai" + suffix, null);
  do_check_eq(Services.cookiemgr.countCookiesFromHost(uri.host), expected[0]);
  // with channel
  Services.cookies.setCookieString(uri, null, "can=has" + suffix, channel);
  do_check_eq(Services.cookiemgr.countCookiesFromHost(uri.host), expected[1]);
  // without channel, from http
  Services.cookies.setCookieStringFromHttp(uri, null, null, "cheez=burger" + suffix, null, null);
  do_check_eq(Services.cookiemgr.countCookiesFromHost(uri.host), expected[2]);
  // with channel, from http
  Services.cookies.setCookieStringFromHttp(uri, null, null, "hot=dog" + suffix, null, channel);
  do_check_eq(Services.cookiemgr.countCookiesFromHost(uri.host), expected[3]);
}
