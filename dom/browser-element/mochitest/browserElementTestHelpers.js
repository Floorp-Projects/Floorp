/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Helpers for managing the browser frame preferences.
"use strict";

function _getPath() {
  if (window.location.protocol == "chrome:") {
    return "/chrome/dom/browser-element/mochitest";
  }
  return window.location.pathname
    .substring(0, window.location.pathname.lastIndexOf("/"))
    .replace("/priority", "");
}

const browserElementTestHelpers = {
  _getBoolPref(pref) {
    try {
      return SpecialPowers.getBoolPref(pref);
    } catch (e) {
      return undefined;
    }
  },

  _setPref(pref, value) {
    this.lockTestReady();
    if (value !== undefined && value !== null) {
      SpecialPowers.pushPrefEnv(
        { set: [[pref, value]] },
        this.unlockTestReady.bind(this)
      );
    } else {
      SpecialPowers.pushPrefEnv(
        { clear: [[pref]] },
        this.unlockTestReady.bind(this)
      );
    }
  },

  _setPrefs() {
    this.lockTestReady();
    SpecialPowers.pushPrefEnv(
      { set: Array.from(arguments) },
      this.unlockTestReady.bind(this)
    );
  },

  _testReadyLockCount: 0,
  _firedTestReady: false,
  lockTestReady() {
    this._testReadyLockCount++;
  },

  unlockTestReady() {
    this._testReadyLockCount--;
    if (this._testReadyLockCount == 0 && !this._firedTestReady) {
      this._firedTestReady = true;
      dispatchEvent(new Event("testready"));
    }
  },

  enableProcessPriorityManager() {
    this._setPrefs(
      ["dom.ipc.processPriorityManager.testMode", true],
      ["dom.ipc.processPriorityManager.enabled", true]
    );
  },

  setClipboardPlainTextOnlyPref(value) {
    this._setPref("clipboard.plainTextOnly", value);
  },

  setEnabledPref(value) {
    this._setPrefs(
      ["dom.mozBrowserFramesEnabled", value],
      ["network.disable.ipc.security", value]
    );
  },

  setupAccessibleCaretPref() {
    this._setPref("layout.accessiblecaret.enabled", true);
    // Disable hide carets for mouse input for select-all tests so that we can
    // get mozbrowsercaretstatechanged events.
    this._setPref("layout.accessiblecaret.hide_carets_for_mouse_input", false);
  },

  getOOPByDefaultPref() {
    return this._getBoolPref("dom.ipc.browser_frames.oop_by_default");
  },

  addPermission() {
    this.lockTestReady();
    SpecialPowers.pushPermissions(
      [{ type: "browser", allow: 1, context: document }],
      this.unlockTestReady.bind(this)
    );
  },

  allowTopLevelDataURINavigation() {
    this._setPref(
      "security.data_uri.block_toplevel_data_uri_navigations",
      false
    );
  },

  _observers: [],

  // This function is a wrapper which lets you register an observer to one of
  // the process priority manager's test-only topics.  observerFn should be a
  // function which takes (subject, topic, data).
  //
  // We'll clean up any observers you add at the end of the test.
  addProcessPriorityObserver(processPriorityTopic, observerFn) {
    var topic = "process-priority-manager:TEST-ONLY:" + processPriorityTopic;

    // SpecialPowers appears to require that the observer be an object, not a
    // function.
    var observer = {
      observe: observerFn,
    };

    SpecialPowers.addObserver(observer, topic);
    this._observers.push([observer, topic]);
  },

  cleanUp() {
    for (var i = 0; i < this._observers.length; i++) {
      SpecialPowers.removeObserver(
        this._observers[i][0],
        this._observers[i][1]
      );
    }
  },

  // Some basically-empty pages from different domains you can load.
  emptyPage1: "http://example.com" + _getPath() + "/file_empty.html",
  fileEmptyPage1: "file_empty.html",
  emptyPage2: "http://example.org" + _getPath() + "/file_empty.html",
  emptyPage3: "http://test1.example.org" + _getPath() + "/file_empty.html",
  focusPage: "http://example.org" + _getPath() + "/file_focus.html",
};

// Returns a promise which is resolved when a subprocess is created.  The
// argument to resolve() is the childID of the subprocess.
function expectProcessCreated(/* optional */ initialPriority) {
  return new Promise(function(resolve, reject) {
    var observed = false;
    browserElementTestHelpers.addProcessPriorityObserver(
      "process-created",
      function(subject, topic, data) {
        // Don't run this observer twice, so we don't ok(true) twice.  (It's fine
        // to resolve a promise twice; the second resolve() call does nothing.)
        if (observed) {
          return;
        }
        observed = true;

        var childID = parseInt(data);
        ok(true, "Got new process, id=" + childID);
        if (initialPriority) {
          expectPriorityChange(childID, initialPriority).then(function() {
            resolve(childID);
          });
        } else {
          resolve(childID);
        }
      }
    );
  });
}

// Just like expectProcessCreated(), except we'll call ok(false) if a second
// process is created.
function expectOnlyOneProcessCreated(/* optional */ initialPriority) {
  var p = expectProcessCreated(initialPriority);
  p.then(function() {
    expectProcessCreated().then(function(childID) {
      ok(false, "Got unexpected process creation, childID=" + childID);
    });
  });
  return p;
}

// Returns a promise which is resolved or rejected the next time the process
// childID changes its priority. We resolve if the priority matches
// expectedPriority, and we reject otherwise.

function expectPriorityChange(childID, expectedPriority) {
  return new Promise(function(resolve, reject) {
    var observed = false;
    browserElementTestHelpers.addProcessPriorityObserver(
      "process-priority-set",
      function(subject, topic, data) {
        if (observed) {
          return;
        }

        var [id, priority] = data.split(":");
        if (id != childID) {
          return;
        }

        // Make sure we run the is() calls in this observer only once, otherwise
        // we'll expect /every/ priority change to match expectedPriority.
        observed = true;

        is(
          priority,
          expectedPriority,
          "Expected priority of childID " +
            childID +
            " to change to " +
            expectedPriority
        );

        if (priority == expectedPriority) {
          resolve();
        } else {
          reject();
        }
      }
    );
  });
}

// Returns a promise which is resolved the first time the given iframe fires
// the mozbrowser##eventName event.
function expectMozbrowserEvent(iframe, eventName) {
  return new Promise(function(resolve, reject) {
    iframe.addEventListener("mozbrowser" + eventName, function handler(e) {
      iframe.removeEventListener("mozbrowser" + eventName, handler);
      resolve(e);
    });
  });
}

// Set some prefs:
//
//  * browser.pagethumbnails.capturing_disabled: true
//
//    Disable tab view; it seriously messes us up.
//
//  * dom.ipc.browser_frames.oop_by_default
//
//    Enable or disable OOP-by-default depending on the test's filename.  You
//    can still force OOP on or off with <iframe mozbrowser remote=true/false>,
//    at least until bug 756376 lands.
//
//  * dom.ipc.tabs.disabled: false
//
//    Allow us to create OOP frames.  Even if they're not the default, some
//    "in-process" tests create OOP frames.
//
//  * network.disable.ipc.security: true
//
//    Disable the networking security checks; our test harness just tests
//    browser elements without sticking them in apps, and the security checks
//    dislike that.
//
//    Unfortunately setting network.disable.ipc.security to false before the
//    child process(es) created by this test have shut down can cause us to
//    assert and kill the child process.  That doesn't cause the tests to fail,
//    but it's still scary looking.  So we just set the pref to true and never
//    pop that value.  We'll rely on the tests which test IPC security to set
//    it to false.
//
//  * security.mixed_content.block_active_content: false
//
//    Disable mixed active content blocking, so that tests can confirm that mixed
//    content results in a broken security state.

(function() {
  var oop = !location.pathname.includes("_inproc_");

  browserElementTestHelpers.lockTestReady();
  SpecialPowers.setBoolPref("network.disable.ipc.security", true);
  SpecialPowers.pushPrefEnv(
    {
      set: [
        ["browser.pagethumbnails.capturing_disabled", true],
        ["dom.ipc.browser_frames.oop_by_default", oop],
        ["dom.ipc.tabs.disabled", false],
        ["security.mixed_content.block_active_content", false],
      ],
    },
    browserElementTestHelpers.unlockTestReady.bind(browserElementTestHelpers)
  );
})();

addEventListener("unload", function() {
  browserElementTestHelpers.cleanUp();
});

// Wait for the load event before unlocking the test-ready event.
browserElementTestHelpers.lockTestReady();
addEventListener("load", function() {
  SimpleTest.executeSoon(
    browserElementTestHelpers.unlockTestReady.bind(browserElementTestHelpers)
  );
});
