/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Loader } = require('sdk/test/loader');

const { loader } = LoaderWithHookedConsole(module);

const pb = loader.require('sdk/private-browsing');
const pbUtils = loader.require('sdk/private-browsing/utils');
const xulApp = require("sdk/system/xul-app");
const { open: openWindow, getMostRecentBrowserWindow } = require('sdk/window/utils');
const { openTab, getTabContentWindow, getActiveTab, setTabURL, closeTab } = require('sdk/tabs/utils');
const promise = require("sdk/core/promise");
const windowHelpers = require('sdk/window/helpers');
const events = require("sdk/system/events");

function LoaderWithHookedConsole(module) {
  let globals = {};
  let errors = [];

  globals.console = Object.create(console, {
    error: {
      value: function(e) {
        errors.push(e);
        if (!/DEPRECATED:/.test(e)) {
          console.error(e);
        }
      }
    }
  });

  let loader = Loader(module, globals);

  return {
    loader: loader,
    errors: errors
  }
}

function deactivate(callback) {
  if (pbUtils.isGlobalPBSupported) {
    if (callback)
      pb.once('stop', callback);
    pb.deactivate();
  }
}
exports.deactivate = deactivate;

exports.pb = pb;
exports.pbUtils = pbUtils;
exports.LoaderWithHookedConsole = LoaderWithHookedConsole;

exports.openWebpage = function openWebpage(url, enablePrivate) {
  if (xulApp.is("Fennec")) {
    let chromeWindow = getMostRecentBrowserWindow();
    let rawTab = openTab(chromeWindow, url, {
      isPrivate: enablePrivate
    });

    return {
      ready: promise.resolve(getTabContentWindow(rawTab)),
      close: function () {
        closeTab(rawTab);
        // Returns a resolved promise as there is no need to wait
        return promise.resolve();
      }
    };
  }
  else {
    let win = openWindow(null, {
      features: {
        private: enablePrivate
      }
    });
    let deferred = promise.defer();

    // Wait for delayed startup code to be executed, in order to ensure
    // that the window is really ready
    events.on("browser-delayed-startup-finished", function onReady({subject}) {
      if (subject == win) {
        events.off("browser-delayed-startup-finished", onReady);
        deferred.resolve(win);

        let rawTab = getActiveTab(win);
        setTabURL(rawTab, url);
        deferred.resolve(getTabContentWindow(rawTab));
      }
    }, true);

    return {
      ready: deferred.promise,
      close: function () {
        return windowHelpers.close(win);
      }
    };
  }
  return null;
}
