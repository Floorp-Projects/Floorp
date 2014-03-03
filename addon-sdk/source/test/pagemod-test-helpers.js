/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc,Ci} = require("chrome");
const timer = require("sdk/timers");
const xulApp = require("sdk/system/xul-app");
const { Loader } = require("sdk/test/loader");
const { openTab, getBrowserForTab, closeTab } = require("sdk/tabs/utils");

/**
 * A helper function that creates a PageMod, then opens the specified URL
 * and checks the effect of the page mod on 'onload' event via testCallback.
 */
exports.testPageMod = function testPageMod(assert, done, testURL, pageModOptions,
                                           testCallback, timeout) {
  if (!xulApp.versionInRange(xulApp.platformVersion, "1.9.3a3", "*") &&
      !xulApp.versionInRange(xulApp.platformVersion, "1.9.2.7", "1.9.2.*")) {
    assert.pass("Note: not testing PageMod, as it doesn't work on this platform version");
    return null;
  }

  var wm = Cc['@mozilla.org/appshell/window-mediator;1']
           .getService(Ci.nsIWindowMediator);
  var browserWindow = wm.getMostRecentWindow("navigator:browser");
  if (!browserWindow) {
    assert.pass("page-mod tests: could not find the browser window, so " +
              "will not run. Use -a firefox to run the pagemod tests.")
    return null;
  }

  let loader = Loader(module);
  let pageMod = loader.require("sdk/page-mod");

  var pageMods = [new pageMod.PageMod(opts) for each(opts in pageModOptions)];

  let newTab = openTab(browserWindow, testURL, {
    inBackground: false
  });
  var b = getBrowserForTab(newTab);

  function onPageLoad() {
    b.removeEventListener("load", onPageLoad, true);
    // Delay callback execute as page-mod content scripts may be executed on
    // load event. So page-mod actions may not be already done.
    // If we delay even more contentScriptWhen:'end', we may want to modify
    // this code again.
    timer.setTimeout(testCallback, 0,
      b.contentWindow.wrappedJSObject, 
      function () {
        pageMods.forEach(function(mod) mod.destroy());
        // XXX leaks reported if we don't close the tab?
        closeTab(newTab);
        loader.unload();
        done();
      }
    );
  }
  b.addEventListener("load", onPageLoad, true);

  return pageMods;
}

/**
 * helper function that creates a PageMod and calls back the appropriate handler
 * based on the value of document.readyState at the time contentScript is attached
 */
exports.handleReadyState = function(url, contentScriptWhen, callbacks) {
  const { PageMod } = Loader(module).require('sdk/page-mod');

  let pagemod = PageMod({
    include: url,
    attachTo: ['existing', 'top'],
    contentScriptWhen: contentScriptWhen,
    contentScript: "self.postMessage(document.readyState)",
    onAttach: worker => {
      let { tab } = worker;
      worker.on('message', readyState => {
        pagemod.destroy();
        // generate event name from `readyState`, e.g. `"loading"` becomes `onLoading`.
        let type = 'on' + readyState[0].toUpperCase() + readyState.substr(1);

        if (type in callbacks)
          callbacks[type](tab); 
      })
    }
  });
}
