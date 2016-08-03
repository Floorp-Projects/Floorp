/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/Promise.jsm");

/**
 * Recursively compare two objects and check that every property of expectedObj has the same value
 * on actualObj.
 */
function isSubObjectOf(expectedObj, actualObj, name) {
  for (let prop in expectedObj) {
    if (typeof expectedObj[prop] == 'function')
      continue;
    if (expectedObj[prop] instanceof Object) {
      is(actualObj[prop].length, expectedObj[prop].length, name + "[" + prop + "]");
      isSubObjectOf(expectedObj[prop], actualObj[prop], name + "[" + prop + "]");
    } else {
      is(actualObj[prop], expectedObj[prop], name + "[" + prop + "]");
    }
  }
}

function getLocale() {
  const localePref = "general.useragent.locale";
  return getLocalizedPref(localePref, Services.prefs.getCharPref(localePref));
}

/**
 * Wrapper for nsIPrefBranch::getComplexValue.
 * @param aPrefName
 *        The name of the pref to get.
 * @returns aDefault if the requested pref doesn't exist.
 */
function getLocalizedPref(aPrefName, aDefault) {
  try {
    return Services.prefs.getComplexValue(aPrefName, Ci.nsIPrefLocalizedString).data;
  } catch (ex) {
    return aDefault;
  }

  return aDefault;
}

function promiseEvent(aTarget, aEventName, aPreventDefault) {
  function cancelEvent(event) {
    if (aPreventDefault) {
      event.preventDefault();
    }

    return true;
  }

  return BrowserTestUtils.waitForEvent(aTarget, aEventName, false, cancelEvent);
}

function promiseNewEngine(basename, options = {}) {
  return new Promise((resolve, reject) => {
    //Default the setAsCurrent option to true.
    let setAsCurrent =
      options.setAsCurrent == undefined ? true : options.setAsCurrent;
    info("Waiting for engine to be added: " + basename);
    Services.search.init({
      onInitComplete: function() {
        let url = getRootDirectory(gTestPath) + basename;
        let current = Services.search.currentEngine;
        Services.search.addEngine(url, null, options.iconURL || "", false, {
          onSuccess: function (engine) {
            info("Search engine added: " + basename);
            if (setAsCurrent) {
              Services.search.currentEngine = engine;
            }
            registerCleanupFunction(() => {
              if (setAsCurrent) {
                Services.search.currentEngine = current;
              }
              Services.search.removeEngine(engine);
              info("Search engine removed: " + basename);
            });
            resolve(engine);
          },
          onError: function (errCode) {
            ok(false, "addEngine failed with error code " + errCode);
            reject();
          }
        });
      }
    });
  });
}

/**
 * Waits for a load (or custom) event to finish in a given tab. If provided
 * load an uri into the tab.
 *
 * @param tab
 *        The tab to load into.
 * @param [optional] url
 *        The url to load, or the current url.
 * @return {Promise} resolved when the event is handled.
 * @resolves to the received event
 * @rejects if a valid load event is not received within a meaningful interval
 */
function promiseTabLoadEvent(tab, url)
{
  let deferred = Promise.defer();
  info("Wait tab event: load");

  function handle(loadedUrl) {
    if (loadedUrl === "about:blank" || (url && loadedUrl !== url)) {
      info(`Skipping spurious load event for ${loadedUrl}`);
      return false;
    }

    info("Tab event received: load");
    return true;
  }

  // Create two promises: one resolved from the content process when the page
  // loads and one that is rejected if we take too long to load the url.
  let loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, handle);

  let timeout = setTimeout(() => {
    deferred.reject(new Error("Timed out while waiting for a 'load' event"));
  }, 30000);

  loaded.then(() => {
    clearTimeout(timeout);
    deferred.resolve()
  });

  if (url)
    BrowserTestUtils.loadURI(tab.linkedBrowser, url);

  // Promise.all rejects if either promise rejects (i.e. if we time out) and
  // if our loaded promise resolves before the timeout, then we resolve the
  // timeout promise as well, causing the all promise to resolve.
  return Promise.all([deferred.promise, loaded]);
}

// Get an array of the one-off buttons.
function getOneOffs() {
  let oneOffs = [];
  let searchPopup = document.getElementById("PopupSearchAutoComplete");
  let oneOffsContainer =
    document.getAnonymousElementByAttribute(searchPopup, "anonid",
                                            "search-one-off-buttons");
  let oneOff =
    document.getAnonymousElementByAttribute(oneOffsContainer, "anonid",
                                            "search-panel-one-offs");
  for (oneOff = oneOff.firstChild; oneOff; oneOff = oneOff.nextSibling) {
    if (oneOff.nodeType == Node.ELEMENT_NODE) {
      if (oneOff.classList.contains("dummy") ||
          oneOff.classList.contains("search-setting-button-compact"))
        break;
      oneOffs.push(oneOff);
    }
  }
  return oneOffs;
}
