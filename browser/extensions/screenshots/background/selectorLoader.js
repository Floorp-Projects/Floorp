/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals browser, catcher, communication, log, main */

"use strict";

// eslint-disable-next-line no-var
var global = this;

this.selectorLoader = (function() {
  const exports = {};

  // These modules are loaded in order, first standardScripts and then selectorScripts
  // The order is important due to dependencies
  const standardScripts = [
    "log.js",
    "catcher.js",
    "assertIsTrusted.js",
    "assertIsBlankDocument.js",
    "blobConverters.js",
    "background/selectorLoader.js",
    "selector/callBackground.js",
    "selector/util.js",
  ];

  const selectorScripts = [
    "clipboard.js",
    "makeUuid.js",
    "build/selection.js",
    "build/shot.js",
    "randomString.js",
    "domainFromUrl.js",
    "build/inlineSelectionCss.js",
    "selector/documentMetadata.js",
    "selector/ui.js",
    "selector/shooter.js",
    "selector/uicontrol.js",
  ];

  exports.unloadIfLoaded = function(tabId) {
    return browser.tabs
      .executeScript(tabId, {
        code: "this.selectorLoader && this.selectorLoader.unloadModules()",
        runAt: "document_start",
      })
      .then(result => {
        return result && result[0];
      });
  };

  exports.testIfLoaded = function(tabId) {
    if (loadingTabs.has(tabId)) {
      return true;
    }
    return browser.tabs
      .executeScript(tabId, {
        code: "!!this.selectorLoader",
        runAt: "document_start",
      })
      .then(result => {
        return result && result[0];
      });
  };

  const loadingTabs = new Set();

  exports.loadModules = function(tabId) {
    loadingTabs.add(tabId);
    catcher.watchPromise(
      executeModules(tabId, standardScripts.concat(selectorScripts)).then(
        () => {
          loadingTabs.delete(tabId);
        }
      )
    );
  };

  function executeModules(tabId, scripts) {
    let lastPromise = Promise.resolve(null);
    scripts.forEach(file => {
      lastPromise = lastPromise.then(() => {
        return browser.tabs
          .executeScript(tabId, {
            file,
            runAt: "document_start",
          })
          .catch(error => {
            log.error("error in script:", file, error);
            error.scriptName = file;
            throw error;
          });
      });
    });
    return lastPromise.then(
      () => {
        log.debug("finished loading scripts:", scripts.join(" "));
      },
      error => {
        exports.unloadIfLoaded(tabId);
        catcher.unhandled(error);
        throw error;
      }
    );
  }

  exports.unloadModules = function() {
    const watchFunction = catcher.watchFunction;
    const allScripts = standardScripts.concat(selectorScripts);
    const moduleNames = allScripts.map(filename =>
      filename.replace(/^.*\//, "").replace(/\.js$/, "")
    );
    moduleNames.reverse();
    for (const moduleName of moduleNames) {
      const moduleObj = global[moduleName];
      if (moduleObj && moduleObj.unload) {
        try {
          watchFunction(moduleObj.unload)();
        } catch (e) {
          // ignore (watchFunction handles it)
        }
      }
      delete global[moduleName];
    }
    return true;
  };

  exports.toggle = function(tabId) {
    return exports.unloadIfLoaded(tabId).then(wasLoaded => {
      if (!wasLoaded) {
        exports.loadModules(tabId);
      }
      return !wasLoaded;
    });
  };

  return exports;
})();
null;
