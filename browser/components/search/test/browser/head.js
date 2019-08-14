/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

XPCOMUtils.defineLazyModuleGetters(this, {
  CustomizableUITestUtils:
    "resource://testing-common/CustomizableUITestUtils.jsm",
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
  let current = await Services.search.getDefault();
  let engine = await Services.search.addEngine(
    url,
    options.iconURL || "",
    false
  );
  info("Search engine added: " + basename);
  if (setAsCurrent) {
    await Services.search.setDefault(engine);
  }
  registerCleanupFunction(async () => {
    if (setAsCurrent) {
      await Services.search.setDefault(current);
    }
    await Services.search.removeEngine(engine);
    info("Search engine removed: " + basename);
  });
  return engine;
}

let promiseStateChangeFrameScript =
  "data:," +
  encodeURIComponent(
    `(${() => {
      /* globals docShell, sendAsyncMessage */

      const global = this;
      const LISTENER = Symbol("listener");
      let listener = {
        QueryInterface: ChromeUtils.generateQI([
          "nsISupportsWeakReference",
          "nsIWebProgressListener",
        ]),

        onStateChange: function onStateChange(webProgress, req, flags, status) {
          // Only care about top-level document starts
          if (
            !webProgress.isTopLevel ||
            !(flags & Ci.nsIWebProgressListener.STATE_START)
          ) {
            return;
          }

          req.QueryInterface(Ci.nsIChannel);
          let spec = req.originalURI.spec;
          if (spec == "about:blank") {
            return;
          }

          delete global[LISTENER];
          docShell.removeProgressListener(listener);

          req.cancel(Cr.NS_ERROR_FAILURE);

          sendAsyncMessage("PromiseStateChange::StateChanged", spec);
        },
      };

      // Make sure the weak reference stays alive.
      global[LISTENER] = listener;

      docShell.QueryInterface(Ci.nsIWebProgress);
      docShell.addProgressListener(
        listener,
        Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT
      );
    }})()`
  );

function promiseStateChangeURI() {
  const MSG = "PromiseStateChange::StateChanged";

  return new Promise(resolve => {
    let mm = window.getGroupMessageManager("browsers");
    mm.loadFrameScript(promiseStateChangeFrameScript, true);

    let listener = msg => {
      mm.removeMessageListener(MSG, listener);
      mm.removeDelayedFrameScript(promiseStateChangeFrameScript);

      resolve(msg.data);
    };

    mm.addMessageListener(MSG, listener);
  });
}

/**
 * Waits for a load (or custom) event to finish in a given tab. If provided
 * load an uri into the tab.
 *
 * @param {object} tab
 *        The tab to load into.
 * @param {string} [url]
 *        The url to load, or the current url.
 * @returns {Promise} resolved when the event is handled.
 * @resolves to the received event
 * @rejects if a valid load event is not received within a meaningful interval
 */
function promiseTabLoadEvent(tab, url) {
  info("Wait tab event: load");

  function handle(loadedUrl) {
    if (loadedUrl === "about:blank" || (url && loadedUrl !== url)) {
      info(`Skipping spurious load event for ${loadedUrl}`);
      return false;
    }

    info("Tab event received: load");
    return true;
  }

  let loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, handle);

  if (url) {
    BrowserTestUtils.loadURI(tab.linkedBrowser, url);
  }

  return loaded;
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
