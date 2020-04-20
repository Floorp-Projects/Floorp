/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

XPCOMUtils.defineLazyModuleGetters(this, {
  CustomizableUITestUtils:
    "resource://testing-common/CustomizableUITestUtils.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  SearchUtils: "resource://gre/modules/SearchUtils.jsm",
});

let gCUITestUtils = new CustomizableUITestUtils(window);

/**
 * Recursively compare two objects and check that every property of expectedObj has the same value
 * on actualObj.
 *
 * @param {object} expectedObj
 *        The expected object to find.
 * @param {object} actualObj
 *        The object to inspect.
 * @param {string} name
 *        The name of the engine, used for test detail logging.
 */
function isSubObjectOf(expectedObj, actualObj, name) {
  for (let prop in expectedObj) {
    if (typeof expectedObj[prop] == "function") {
      continue;
    }
    if (expectedObj[prop] instanceof Object) {
      is(
        actualObj[prop].length,
        expectedObj[prop].length,
        name + "[" + prop + "]"
      );
      isSubObjectOf(
        expectedObj[prop],
        actualObj[prop],
        name + "[" + prop + "]"
      );
    } else {
      is(actualObj[prop], expectedObj[prop], name + "[" + prop + "]");
    }
  }
}

function getLocale() {
  return Services.locale.requestedLocale || undefined;
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

/**
 * Adds a new search engine to the search service and confirms it completes.
 *
 * @param {string} basename  The file to load that contains the search engine
 *                           details.
 * @param {object} [options] Options for the test:
 *   - {String} [iconURL]       The icon to use for the search engine.
 *   - {Boolean} [setAsCurrent] Whether to set the new engine to be the
 *                              current engine or not.
 *   - {Boolean} [setAsCurrentPrivate] Whether to set the new engine to be the
 *                              current private browsing mode engine or not.
 *                              Defaults to false.
 *   - {String} [testPath]      Used to override the current test path if this
 *                              file is used from a different directory.
 * @returns {Promise} The promise is resolved once the engine is added, or
 *                    rejected if the addition failed.
 */
async function promiseNewEngine(basename, options = {}) {
  // Default the setAsCurrent option to true.
  let setAsCurrent =
    options.setAsCurrent == undefined ? true : options.setAsCurrent;
  info("Waiting for engine to be added: " + basename);
  let url = getRootDirectory(options.testPath || gTestPath) + basename;
  let engine = await Services.search.addEngine(
    url,
    options.iconURL || "",
    false
  );
  info("Search engine added: " + basename);
  const current = await Services.search.getDefault();
  if (setAsCurrent) {
    await Services.search.setDefault(engine);
  }
  const currentPrivate = await Services.search.getDefaultPrivate();
  if (options.setAsCurrentPrivate) {
    await Services.search.setDefaultPrivate(engine);
  }
  registerCleanupFunction(async () => {
    if (setAsCurrent) {
      await Services.search.setDefault(current);
    }
    if (options.setAsCurrentPrivate) {
      await Services.search.setDefaultPrivate(currentPrivate);
    }
    await Services.search.removeEngine(engine);
    info("Search engine removed: " + basename);
  });
  return engine;
}

// Get an array of the one-off buttons.
function getOneOffs() {
  let oneOffs = [];
  let searchPopup = document.getElementById("PopupSearchAutoComplete");
  let oneOffsContainer = searchPopup.searchOneOffsContainer;
  let oneOff = oneOffsContainer.querySelector(".search-panel-one-offs");
  for (oneOff = oneOff.firstChild; oneOff; oneOff = oneOff.nextSibling) {
    if (oneOff.nodeType == Node.ELEMENT_NODE) {
      oneOffs.push(oneOff);
    }
  }
  return oneOffs;
}
