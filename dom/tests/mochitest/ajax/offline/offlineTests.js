// Utility functions for offline tests.
netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

var Cc = Components.classes;
var Ci = Components.interfaces;

var OfflineTest = {

_slaveWindow: null,

// The window where test results should be sent.
_masterWindow: null,

setupChild: function()
{
  if (window.parent.OfflineTest.hasSlave()) {
    return false;
  }

  this._slaveWindow = null;
  this._masterWindow = window.top;

  return true;
},

// Setup the tests.  This will reload the current page in a new window
// if necessary.
setup: function()
{
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  if (!window.opener || !window.opener.OfflineTest ||
      !window.opener.OfflineTest._isMaster) {
    // Offline applications must be toplevel windows and have the
    // offline-app permission.  Because we were loaded without the
    // offline-app permission and (probably) in an iframe, we need to
    // enable the pref and spawn a new window to perform the actual
    // tests.  It will use this window to report successes and
    // failures.
    var pm = Cc["@mozilla.org/permissionmanager;1"]
      .getService(Ci.nsIPermissionManager);
    var uri = Cc["@mozilla.org/network/io-service;1"]
      .getService(Ci.nsIIOService)
      .newURI(window.location.href, null, null);
    pm.add(uri, "offline-app", Ci.nsIPermissionManager.ALLOW_ACTION);

    // Tests must run as toplevel windows.  Open a slave window to run
    // the test.
    this._isMaster = true;
    this._slaveWindow = window.open(window.location, "offlinetest");

    this._slaveWindow._OfflineSlaveWindow = true;

    return false;
  }

  this._masterWindow = window.opener;

  return true;
},

teardown: function()
{
  // Remove the offline-app permission we gave ourselves.

  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  var pm = Cc["@mozilla.org/permissionmanager;1"]
           .getService(Ci.nsIPermissionManager);
  var uri = Cc["@mozilla.org/network/io-service;1"]
            .getService(Ci.nsIIOService)
            .newURI(window.location.href, null, null);
  pm.remove(uri.host, "offline-app");

  this.clear();
},

finish: function()
{
  SimpleTest.finish();

  if (this._masterWindow) {
    this._masterWindow.OfflineTest.finish();
    window.close();
  }
},

hasSlave: function()
{
  return (this._slaveWindow != null);
},

//
// Mochitest wrappers - These forward tests to the proper mochitest window.
//
ok: function(condition, name, diag)
{
  return this._masterWindow.SimpleTest.ok(condition, name, diag);
},

is: function(a, b, name)
{
  return this._masterWindow.SimpleTest.is(a, b, name);
},

clear: function()
{
  // Clear the ownership list
  var cacheService = Cc["@mozilla.org/network/cache-service;1"]
                     .getService(Ci.nsICacheService);
  var cacheSession = cacheService.createSession("HTTP-offline",
                                                Ci.nsICache.STORE_OFFLINE,
                                                true)
                     .QueryInterface(Ci.nsIOfflineCacheSession);

  // Get the asciiHost from the page URL
  var locationURI = Cc["@mozilla.org/network/standard-url;1"]
                     .createInstance(Ci.nsIURI);
  locationURI.spec = window.location.href;
  var asciiHost = locationURI.asciiHost;

  // Clear manifest-owned urls
  cacheSession.setOwnedKeys(asciiHost,
                            this.getManifestUrl() + "#manifest", 0, []);

  // Clear dynamically-owned urls
  cacheSession.setOwnedKeys(asciiHost,
                            this.getManifestUrl() + "#dynamic", 0, []);

  cacheSession.evictUnownedEntries();
},

failEvent: function(e)
{
  OfflineTest.ok(false, "Unexpected event: " + e.type);
},

// The offline API as specified has no way to watch the load of a resource
// added with applicationCache.add().
waitForAdd: function(url, onFinished) {
  // Check every half second for ten seconds.
  var numChecks = 20;
  var waitFunc = function() {
    var cacheService = Cc["@mozilla.org/network/cache-service;1"]
    .getService(Ci.nsICacheService);
    var cacheSession = cacheService.createSession("HTTP-offline",
                                                  Ci.nsICache.STORE_OFFLINE,
                                                  true);
    var entry;
    try {
      var entry = cacheSession.openCacheEntry(url, Ci.nsICache.ACCESS_READ, true);
    } catch (e) {
    }

    if (entry) {
      entry.close();
      onFinished();
      return;
    }

    if (--numChecks == 0) {
      onFinished();
      return;
    }

    setTimeout(OfflineTest.priv(waitFunc), 500);
  }

  setTimeout(this.priv(waitFunc), 500);
},

getManifestUrl: function()
{
  return window.top.document.documentElement.getAttribute("manifest");
},

priv: function(func)
{
  var self = this;
  return function() {
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
    func();
  }
},

checkCache: function(url, expectEntry)
{
  var cacheService = Cc["@mozilla.org/network/cache-service;1"]
  .getService(Ci.nsICacheService);
  var cacheSession = cacheService.createSession("HTTP-offline",
                                                Ci.nsICache.STORE_OFFLINE,
                                                true);
  try {
    var entry = cacheSession.openCacheEntry(url, Ci.nsICache.ACCESS_READ, true);
    if (expectEntry) {
      this.ok(true, url + " should exist in the offline cache");
    } else {
      this.ok(false, url + " should not exist in the offline cache");
    }
    entry.close();
  } catch (e) {
    // this constant isn't in Components.results
    const kNetBase = 2152398848; // 0x804B0000
    var NS_ERROR_CACHE_KEY_NOT_FOUND = kNetBase + 61
    if (e.result == NS_ERROR_CACHE_KEY_NOT_FOUND) {
      if (expectEntry) {
        this.ok(false, url + " should exist in the offline cache");
      } else {
        this.ok(true, url + " should not exist in the offline cache");
      }
    } else {
      throw e;
    }
  }
}

};
