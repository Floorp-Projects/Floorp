/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let console = (function() {
  let tempScope = {};
  Components.utils.import("resource:///modules/devtools/Console.jsm", tempScope);
  return tempScope.console;
})();

/**
 * Open a new tab at a URL and call a callback on load
 */
function addTab(aURL, aCallback)
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  content.location = aURL;

  let tab = gBrowser.selectedTab;
  let browser = gBrowser.getBrowserForTab(tab);

  function onTabLoad() {
    browser.removeEventListener("load", onTabLoad, true);
    aCallback(browser, tab, browser.contentDocument);
  }

  browser.addEventListener("load", onTabLoad, true);
}

registerCleanupFunction(function tearDown() {
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }

  console = undefined;
});

/**
 * Various functions for testing DeveloperToolbar.
 * Parts of this code exist in:
 * - browser/devtools/commandline/test/head.js
 * - browser/devtools/shared/test/head.js
 */
let DeveloperToolbarTest = {
  /**
   * Paranoid DeveloperToolbar.show();
   */
  show: function DTT_show(aCallback) {
    if (DeveloperToolbar.visible) {
      ok(false, "DeveloperToolbar.visible at start of openDeveloperToolbar");
    }
    else {
      DeveloperToolbar.show(true, aCallback);
    }
  },

  /**
   * Paranoid DeveloperToolbar.hide();
   */
  hide: function DTT_hide() {
    if (!DeveloperToolbar.visible) {
      ok(false, "!DeveloperToolbar.visible at start of closeDeveloperToolbar");
    }
    else {
      DeveloperToolbar.display.inputter.setInput("");
      DeveloperToolbar.hide();
    }
  },

  /**
   * Quick wrapper around the things you need to do to run DeveloperToolbar
   * command tests:
   * - Set the pref 'devtools.toolbar.enabled' to true
   * - Add a tab pointing at |uri|
   * - Open the DeveloperToolbar
   * - Register a cleanup function to undo the above
   * - Run the tests
   *
   * @param uri The uri of a page to load. Can be 'about:blank' or 'data:...'
   * @param testFunc A function containing the tests to run. This should
   * arrange for 'finish()' to be called on completion.
   */
  test: function DTT_test(uri, testFunc) {
    let menuItem = document.getElementById("menu_devToolbar");
    let command = document.getElementById("Tools:DevToolbar");
    let appMenuItem = document.getElementById("appmenu_devToolbar");

    registerCleanupFunction(function() {
      DeveloperToolbarTest.hide();

      // a.k.a Services.prefs.clearUserPref("devtools.toolbar.enabled");
      if (menuItem) menuItem.hidden = true;
      if (command) command.setAttribute("disabled", "true");
      if (appMenuItem) appMenuItem.hidden = true;
    });

    // a.k.a: Services.prefs.setBoolPref("devtools.toolbar.enabled", true);
    if (menuItem) menuItem.hidden = false;
    if (command) command.removeAttribute("disabled");
    if (appMenuItem) appMenuItem.hidden = false;

    addTab(uri, function(browser, tab) {
      DeveloperToolbarTest.show(function() {

        try {
          testFunc(browser, tab);
        }
        catch (ex) {
          ok(false, "" + ex);
          console.error(ex);
          finish();
          throw ex;
        }
      });
    });
  },
};

function catchFail(func) {
  return function() {
    try {
      return func.apply(null, arguments);
    }
    catch (ex) {
      ok(false, ex);
      console.error(ex);
      finish();
      throw ex;
    }
  };
}

/**
 * Polls a given function waiting for the given value.
 *
 * @param object aOptions
 *        Options object with the following properties:
 *        - validator
 *        A validator function that should return the expected value. This is
 *        called every few milliseconds to check if the result is the expected
 *        one. When the returned result is the expected one, then the |success|
 *        function is called and polling stops. If |validator| never returns
 *        the expected value, then polling timeouts after several tries and
 *        a failure is recorded - the given |failure| function is invoked.
 *        - success
 *        A function called when the validator function returns the expected
 *        value.
 *        - failure
 *        A function called if the validator function timeouts - fails to return
 *        the expected value in the given time.
 *        - name
 *        Name of test. This is used to generate the success and failure
 *        messages.
 *        - timeout
 *        Timeout for validator function, in milliseconds. Default is 5000 ms.
 *        - value
 *        The expected value. If this option is omitted then the |validator|
 *        function must return a trueish value.
 *        Each of the provided callback functions will receive two arguments:
 *        the |aOptions| object and the last value returned by |validator|.
 */
function waitForValue(aOptions)
{
  let start = Date.now();
  let timeout = aOptions.timeout || 5000;
  let lastValue;

  function wait(validatorFn, successFn, failureFn)
  {
    if ((Date.now() - start) > timeout) {
      // Log the failure.
      ok(false, "Timed out while waiting for: " + aOptions.name);
      let expected = "value" in aOptions ?
                     "'" + aOptions.value + "'" :
                     "a trueish value";
      info("timeout info :: got '" + lastValue + "', expected " + expected);
      failureFn(aOptions, lastValue);
      return;
    }

    lastValue = validatorFn(aOptions, lastValue);
    let successful = "value" in aOptions ?
                      lastValue == aOptions.value :
                      lastValue;
    if (successful) {
      ok(true, aOptions.name);
      successFn(aOptions, lastValue);
    }
    else {
      setTimeout(function() wait(validatorFn, successFn, failureFn), 100);
    }
  }

  wait(aOptions.validator, aOptions.success, aOptions.failure);
}
