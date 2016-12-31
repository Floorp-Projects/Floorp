/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

var Social, SocialService;

var manifests = [
  {
    name: "provider 1",
    origin: "https://example1.com",
    sidebarURL: "https://example1.com/sidebar/",
  },
  {
    name: "provider 2",
    origin: "https://example2.com",
    sidebarURL: "https://example1.com/sidebar/",
  }
];

const MANIFEST_PREFS = Services.prefs.getBranch("social.manifest.");

// SocialProvider class relies on blocklisting being enabled.  To enable
// blocklisting, we have to setup an app and initialize the blocklist (see
// initApp below).
const gProfD = do_get_profile();

function createAppInfo(ID, name, version, platformVersion = "1.0") {
  let tmp = {};
  Cu.import("resource://testing-common/AppInfo.jsm", tmp);
  tmp.updateAppInfo({
    ID, name, version, platformVersion,
    crashReporter: true,
  });
  gAppInfo = tmp.getAppInfo();
}

function initApp() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");
  // prepare a blocklist file for the blocklist service
  var blocklistFile = gProfD.clone();
  blocklistFile.append("blocklist.xml");
  if (blocklistFile.exists())
    blocklistFile.remove(false);
  var source = do_get_file("blocklist.xml");
  source.copyTo(gProfD, "blocklist.xml");
  blocklistFile.lastModifiedTime = Date.now();


  let internalManager = Cc["@mozilla.org/addons/integration;1"].
                     getService(Ci.nsIObserver).
                     QueryInterface(Ci.nsITimerCallback);

  internalManager.observe(null, "addons-startup", null);
}

function setManifestPref(manifest) {
  let string = Cc["@mozilla.org/supports-string;1"].
               createInstance(Ci.nsISupportsString);
  string.data = JSON.stringify(manifest);
  Services.prefs.setComplexValue("social.manifest." + manifest.origin, Ci.nsISupportsString, string);
}

function do_wait_observer(obsTopic, cb) {
  function observer(subject, topic, data) {
    Services.obs.removeObserver(observer, topic);
    cb();
  }
  Services.obs.addObserver(observer, obsTopic, false);
}

function do_add_providers(cb) {
  // run only after social is already initialized
  SocialService.addProvider(manifests[0], function() {
    do_wait_observer("social:providers-changed", function() {
      do_check_eq(Social.providers.length, 2, "2 providers installed");
      do_execute_soon(cb);
    });
    SocialService.addProvider(manifests[1]);
  });
}

function do_initialize_social(enabledOnStartup, cb) {
  initApp();

  if (enabledOnStartup) {
    // set prefs before initializing social
    manifests.forEach(function(manifest) {
      setManifestPref(manifest);
    });
    // Set both providers active and flag the first one as "current"
    let activeVal = Cc["@mozilla.org/supports-string;1"].
               createInstance(Ci.nsISupportsString);
    let active = {};
    for (let m of manifests)
      active[m.origin] = 1;
    activeVal.data = JSON.stringify(active);
    Services.prefs.setComplexValue("social.activeProviders",
                                   Ci.nsISupportsString, activeVal);

    do_register_cleanup(function() {
      manifests.forEach(function(manifest) {
        Services.prefs.clearUserPref("social.manifest." + manifest.origin);
      });
      Services.prefs.clearUserPref("social.activeProviders");
    });

    // expecting 2 providers installed
    do_wait_observer("social:providers-changed", function() {
      do_check_eq(Social.providers.length, 2, "2 providers installed");
      do_execute_soon(cb);
    });
  }

  // import and initialize everything
  SocialService = Cu.import("resource:///modules/SocialService.jsm", {}).SocialService;
  do_check_eq(enabledOnStartup, SocialService.hasEnabledProviders, "Service has enabled providers");
  Social = Cu.import("resource:///modules/Social.jsm", {}).Social;
  do_check_false(Social.initialized, "Social is not initialized");
  Social.init();
  do_check_true(Social.initialized, "Social is initialized");
  if (!enabledOnStartup)
    do_execute_soon(cb);
}

function AsyncRunner() {
  do_test_pending();
  do_register_cleanup(() => this.destroy());

  this._callbacks = {
    done: do_test_finished,
    error(err) {
      // xpcshell test functions like do_check_eq throw NS_ERROR_ABORT on
      // failure.  Ignore those so they aren't rethrown here.
      if (err !== Cr.NS_ERROR_ABORT) {
        if (err.stack) {
          err = err + " - See following stack:\n" + err.stack +
                      "\nUseless do_throw stack";
        }
        do_throw(err);
      }
    },
    consoleError(scriptErr) {
      // Try to ensure the error is related to the test.
      let filename = scriptErr.sourceName || scriptErr.toString() || "";
      if (filename.indexOf("/toolkit/components/social/") >= 0)
        do_throw(scriptErr);
    },
  };
  this._iteratorQueue = [];

  // This catches errors reported to the console, e.g., via Cu.reportError, but
  // not on the runner's stack.
  Cc["@mozilla.org/consoleservice;1"].
    getService(Ci.nsIConsoleService).
    registerListener(this);
}

AsyncRunner.prototype = {

  appendIterator: function appendIterator(iter) {
    this._iteratorQueue.push(iter);
  },

  next: function next(arg) {
    if (!this._iteratorQueue.length) {
      this.destroy();
      this._callbacks.done();
      return;
    }

    try {
      var { done, value: val } = this._iteratorQueue[0].next(arg);
      if (done) {
        this._iteratorQueue.shift();
        this.next();
        return;
      }
    } catch (err) {
      this._callbacks.error(err);
    }

    // val is an iterator => prepend it to the queue and start on it
    // val is otherwise truthy => call next
    if (val) {
      if (typeof(val) != "boolean")
        this._iteratorQueue.unshift(val);
      this.next();
    }
  },

  destroy: function destroy() {
    Cc["@mozilla.org/consoleservice;1"].
      getService(Ci.nsIConsoleService).
      unregisterListener(this);
    this.destroy = function alreadyDestroyed() {};
  },

  observe: function observe(msg) {
    if (msg instanceof Ci.nsIScriptError &&
        !(msg.flags & Ci.nsIScriptError.warningFlag)) {
      this._callbacks.consoleError(msg);
    }
  },
};
