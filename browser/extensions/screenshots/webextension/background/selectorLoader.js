/* globals catcher, communication, log */

"use strict";

// eslint-disable-next-line no-var
var global = this;

this.selectorLoader = (function() {
  const exports = {};

  // These modules are loaded in order, first standardScripts, then optionally onboardingScripts, and then selectorScripts
  // The order is important due to dependencies
  const standardScripts = [
    "build/buildSettings.js",
    "log.js",
    "catcher.js",
    "assertIsTrusted.js",
    "assertIsBlankDocument.js",
    "blobConverters.js",
    "background/selectorLoader.js",
    "selector/callBackground.js",
    "selector/util.js"
  ];

  const selectorScripts = [
    "clipboard.js",
    "makeUuid.js",
    "build/shot.js",
    "randomString.js",
    "domainFromUrl.js",
    "build/inlineSelectionCss.js",
    "selector/documentMetadata.js",
    "selector/ui.js",
    "selector/shooter.js",
    "selector/uicontrol.js"
  ];

  // These are loaded on request (by the selector worker) to activate the onboarding:
  const onboardingScripts = [
    "build/onboardingCss.js",
    "build/onboardingHtml.js",
    "onboarding/slides.js"
  ];

  exports.unloadIfLoaded = function(tabId) {
    return browser.tabs.executeScript(tabId, {
      code: "this.selectorLoader && this.selectorLoader.unloadModules()",
      runAt: "document_start"
    }).then(result => {
      return result && result[0];
    });
  };

  exports.testIfLoaded = function(tabId) {
    if (loadingTabs.has(tabId)) {
      return true;
    }
    return browser.tabs.executeScript(tabId, {
      code: "!!this.selectorLoader",
      runAt: "document_start"
    }).then(result => {
      return result && result[0];
    });
  };

  const loadingTabs = new Set();

  exports.loadModules = function(tabId, hasSeenOnboarding) {
    catcher.watchPromise(hasSeenOnboarding.then(onboarded => {
      loadingTabs.add(tabId);
      let promise = downloadOnlyCheck(tabId);
      if (onboarded) {
        promise = promise.then(() => {
          return executeModules(tabId, standardScripts.concat(selectorScripts));
        });
      } else {
        promise = promise.then(() => {
          return executeModules(tabId, standardScripts.concat(onboardingScripts).concat(selectorScripts));
        });
      }
      return promise.then((result) => {
        loadingTabs.delete(tabId);
        return result;
      }, (error) => {
        loadingTabs.delete(tabId);
        throw error;
      });
    }));
  };

  // TODO: since bootstrap communication is now required, would this function
  // make more sense inside background/main?
  function downloadOnlyCheck(tabId) {
    return communication.sendToBootstrap("isHistoryEnabled").then((historyEnabled) => {
      return communication.sendToBootstrap("isUploadDisabled").then((uploadDisabled) => {
        return browser.tabs.get(tabId).then(tab => {
          const downloadOnly = !historyEnabled || uploadDisabled || tab.incognito;
          return browser.tabs.executeScript(tabId, {
            // Note: `window` here refers to a global accessible to content
            // scripts, but not the scripts in the underlying page. For more
            // details, see https://mdn.io/WebExtensions/Content_scripts#Content_script_environment
            code: `window.downloadOnly = ${downloadOnly}`,
            runAt: "document_start"
          });
        });
      });
    });
  }

  function executeModules(tabId, scripts) {
    let lastPromise = Promise.resolve(null);
    scripts.forEach((file) => {
      lastPromise = lastPromise.then(() => {
        return browser.tabs.executeScript(tabId, {
          file,
          runAt: "document_start"
        }).catch((error) => {
          log.error("error in script:", file, error);
          error.scriptName = file;
          throw error;
        });
      });
    });
    return lastPromise.then(() => {
      log.debug("finished loading scripts:", scripts.join(" "));
    },
    (error) => {
      exports.unloadIfLoaded(tabId);
      catcher.unhandled(error);
      throw error;
    });
  }

  exports.unloadModules = function() {
    const watchFunction = catcher.watchFunction;
    const allScripts = standardScripts.concat(onboardingScripts).concat(selectorScripts);
    const moduleNames = allScripts.map((filename) =>
      filename.replace(/^.*\//, "").replace(/\.js$/, ""));
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

  exports.toggle = function(tabId, hasSeenOnboarding) {
    return exports.unloadIfLoaded(tabId)
      .then(wasLoaded => {
        if (!wasLoaded) {
          exports.loadModules(tabId, hasSeenOnboarding);
        }
        return !wasLoaded;
      })
  };

  return exports;
})();
null;
