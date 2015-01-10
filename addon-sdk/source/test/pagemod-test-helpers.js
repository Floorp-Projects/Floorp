/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci } = require("chrome");
const { setTimeout } = require("sdk/timers");
const { Loader } = require("sdk/test/loader");
const { openTab, getBrowserForTab, closeTab } = require("sdk/tabs/utils");
const { getMostRecentBrowserWindow } = require("sdk/window/utils");
const { merge } = require("sdk/util/object");
const httpd = require("./lib/httpd");
const { cleanUI } = require("sdk/test/utils");

const PORT = 8099;
const PATH = '/test-contentScriptWhen.html';

function createLoader () {
  let options = merge({}, require('@loader/options'),
                      { prefixURI: require('./fixtures').url() });
  return Loader(module, null, options);
}
exports.createLoader = createLoader;

function openNewTab(url) {
  return openTab(getMostRecentBrowserWindow(), url, {
    inBackground: false
  });
}
exports.openNewTab = openNewTab;

// an evil function enables the creation of tests
// that depend on delicate event timing. do not use.
function testPageMod(assert, done, testURL, pageModOptions,
                                           testCallback, timeout) {
  let loader = createLoader();
  let { PageMod } = loader.require("sdk/page-mod");
  let pageMods = [new PageMod(opts) for each (opts in pageModOptions)];
  let newTab = openNewTab(testURL);
  let b = getBrowserForTab(newTab);

  function onPageLoad() {
    b.removeEventListener("load", onPageLoad, true);
    // Delay callback execute as page-mod content scripts may be executed on
    // load event. So page-mod actions may not be already done.
    // If we delay even more contentScriptWhen:'end', we may want to modify
    // this code again.
    setTimeout(testCallback, timeout,
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
exports.testPageMod = testPageMod;

/**
 * helper function that creates a PageMod and calls back the appropriate handler
 * based on the value of document.readyState at the time contentScript is attached
 */
exports.handleReadyState = function(url, contentScriptWhen, callbacks) {
  const loader = Loader(module);
  const { PageMod } = loader.require('sdk/page-mod');

  let pagemod = PageMod({
    include: url,
    attachTo: ['existing', 'top'],
    contentScriptWhen: contentScriptWhen,
    contentScript: "self.postMessage(document.readyState)",
    onAttach: worker => {
      let { tab } = worker;
      worker.on('message', readyState => {
        // generate event name from `readyState`, e.g. `"loading"` becomes `onLoading`.
        let type = 'on' + readyState[0].toUpperCase() + readyState.substr(1);

        if (type in callbacks)
          callbacks[type](tab);

        pagemod.destroy();
        loader.unload();
      })
    }
  });
}

// serves a slow page which takes 1.5 seconds to load,
// 0.5 seconds in each readyState: uninitialized, loading, interactive.
function contentScriptWhenServer() {
  const URL = 'http://localhost:' + PORT + PATH;

  const HTML = `/* polyglot js
    <script src="${URL}"></script>
    delay both the "DOMContentLoaded"
    <script async src="${URL}"></script>
    and "load" events */`;

  let srv = httpd.startServerAsync(PORT);

  srv.registerPathHandler(PATH, (_, response) => {
    response.processAsync();
    response.setHeader('Content-Type', 'text/html', false);
    setTimeout(_ => response.finish(), 500);
    response.write(HTML);
  })

  srv.URL = URL;
  return srv;
}
exports.contentScriptWhenServer = contentScriptWhenServer;
