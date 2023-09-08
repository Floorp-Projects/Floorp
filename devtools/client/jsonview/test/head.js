/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local", "args": "none"}] */

"use strict";

// shared-head.js handles imports, constants, and utility functions
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/test/head.js",
  this
);

const JSON_VIEW_PREF = "devtools.jsonview.enabled";

// Enable JSON View for the test
Services.prefs.setBoolPref(JSON_VIEW_PREF, true);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref(JSON_VIEW_PREF);
});

// XXX move some API into devtools/shared/test/shared-head.js

/**
 * Add a new test tab in the browser and load the given url.
 * @param {String} url
 *   The url to be loaded in the new tab.
 *
 * @param {Object} [optional]
 *   An object with the following optional properties:
 *   - appReadyState: The readyState of the JSON Viewer app that you want to
 *     wait for. Its value can be one of:
 *      - "uninitialized": The converter has started the request.
 *        If JavaScript is disabled, there will be no more readyState changes.
 *      - "loading": RequireJS started loading the scripts for the JSON Viewer.
 *        If the load timeouts, there will be no more readyState changes.
 *      - "interactive": The JSON Viewer app loaded, but possibly not all the JSON
 *        data has been received.
 *      - "complete" (default): The app is fully loaded with all the JSON.
 *   - docReadyState: The standard readyState of the document that you want to
 *     wait for. Its value can be one of:
 *      - "loading": The JSON data has not been completely loaded (but the app might).
 *      - "interactive": All the JSON data has been received.
 *      - "complete" (default): Since there aren't sub-resources like images,
 *        behaves as "interactive". Note the app might not be loaded yet.
 */
async function addJsonViewTab(
  url,
  { appReadyState = "complete", docReadyState = "complete" } = {}
) {
  info("Adding a new JSON tab with URL: '" + url + "'");
  const tabAdded = BrowserTestUtils.waitForNewTab(gBrowser, url);
  const tabLoaded = addTab(url);

  // The `tabAdded` promise resolves when the JSON Viewer starts loading.
  // This is usually what we want, however, it never resolves for unrecognized
  // content types that trigger a download.
  // On the other hand, `tabLoaded` always resolves, but not until the document
  // is fully loaded, which is too late if `docReadyState !== "complete"`.
  // Therefore, we race both promises.
  const tab = await Promise.race([tabAdded, tabLoaded]);
  const browser = tab.linkedBrowser;

  const rootDir = getRootDirectory(gTestPath);

  // Catch RequireJS errors (usually timeouts)
  const error = tabLoaded.then(() =>
    SpecialPowers.spawn(browser, [], function () {
      return new Promise((resolve, reject) => {
        const { requirejs } = content.wrappedJSObject;
        if (requirejs) {
          requirejs.onError = err => {
            info(err);
            ok(false, "RequireJS error");
            reject(err);
          };
        }
      });
    })
  );

  const data = { rootDir, appReadyState, docReadyState };
  await Promise.race([
    error,
    // eslint-disable-next-line no-shadow
    ContentTask.spawn(browser, data, async function (data) {
      // Check if there is a JSONView object.
      const { JSONView } = content.wrappedJSObject;
      if (!JSONView) {
        throw new Error("The JSON Viewer did not load.");
      }

      const docReadyStates = ["loading", "interactive", "complete"];
      const docReadyIndex = docReadyStates.indexOf(data.docReadyState);
      const appReadyStates = ["uninitialized", ...docReadyStates];
      const appReadyIndex = appReadyStates.indexOf(data.appReadyState);
      if (docReadyIndex < 0 || appReadyIndex < 0) {
        throw new Error("Invalid app or doc readyState parameter.");
      }

      // Wait until the document readyState suffices.
      const { document } = content;
      while (docReadyStates.indexOf(document.readyState) < docReadyIndex) {
        info(
          `DocReadyState is "${document.readyState}". Await "${data.docReadyState}"`
        );
        await new Promise(resolve => {
          document.addEventListener("readystatechange", resolve, {
            once: true,
          });
        });
      }

      // Wait until the app readyState suffices.
      while (appReadyStates.indexOf(JSONView.readyState) < appReadyIndex) {
        info(
          `AppReadyState is "${JSONView.readyState}". Await "${data.appReadyState}"`
        );
        await new Promise(resolve => {
          content.addEventListener("AppReadyStateChange", resolve, {
            once: true,
          });
        });
      }
    }),
  ]);

  return tab;
}

/**
 * Expanding a node in the JSON tree
 */
function clickJsonNode(selector) {
  info("Expanding node: '" + selector + "'");

  // eslint-disable-next-line no-shadow
  return ContentTask.spawn(gBrowser.selectedBrowser, selector, selector => {
    content.document.querySelector(selector).click();
  });
}

/**
 * Select JSON View tab (in the content).
 */
function selectJsonViewContentTab(name) {
  info("Selecting tab: '" + name + "'");

  // eslint-disable-next-line no-shadow
  return ContentTask.spawn(gBrowser.selectedBrowser, name, async name => {
    const tabsSelector = ".tabs-menu .tabs-menu-item";
    const targetTabSelector = `${tabsSelector}.${CSS.escape(name)}`;
    const targetTab = content.document.querySelector(targetTabSelector);
    const targetTabIndex = Array.prototype.indexOf.call(
      content.document.querySelectorAll(tabsSelector),
      targetTab
    );
    const targetTabButton = targetTab.querySelector("a");
    await new Promise(resolve => {
      content.addEventListener(
        "TabChanged",
        ({ detail: { index } }) => {
          is(index, targetTabIndex, "Hm?");
          if (index === targetTabIndex) {
            resolve();
          }
        },
        { once: true }
      );
      targetTabButton.click();
    });
    is(
      targetTabButton.getAttribute("aria-selected"),
      "true",
      "Tab is now selected"
    );
  });
}

function getElementCount(selector) {
  info("Get element count: '" + selector + "'");

  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [selector],
    selectorChild => {
      return content.document.querySelectorAll(selectorChild).length;
    }
  );
}

function getElementText(selector) {
  info("Get element text: '" + selector + "'");

  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [selector],
    selectorChild => {
      const element = content.document.querySelector(selectorChild);
      return element ? element.textContent : null;
    }
  );
}

function getElementAttr(selector, attr) {
  info("Get attribute '" + attr + "' for element '" + selector + "'");

  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [selector, attr],
    (selectorChild, attrChild) => {
      const element = content.document.querySelector(selectorChild);
      return element ? element.getAttribute(attrChild) : null;
    }
  );
}

function focusElement(selector) {
  info("Focus element: '" + selector + "'");

  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [selector],
    selectorChild => {
      const element = content.document.querySelector(selectorChild);
      if (element) {
        element.focus();
      }
    }
  );
}

/**
 * Send the string aStr to the focused element.
 *
 * For now this method only works for ASCII characters and emulates the shift
 * key state on US keyboard layout.
 */
function sendString(str, selector) {
  info("Send string: '" + str + "'");

  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [selector, str],
    (selectorChild, strChild) => {
      if (selectorChild) {
        const element = content.document.querySelector(selectorChild);
        if (element) {
          element.focus();
        }
      }

      EventUtils.sendString(strChild, content);
    }
  );
}

function waitForTime(delay) {
  return new Promise(resolve => setTimeout(resolve, delay));
}

function waitForFilter() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    return new Promise(resolve => {
      const firstRow = content.document.querySelector(
        ".jsonPanelBox .treeTable .treeRow"
      );

      // Check if the filter is already set.
      if (firstRow.classList.contains("hidden")) {
        resolve();
        return;
      }

      // Wait till the first row has 'hidden' class set.
      const observer = new content.MutationObserver(function (mutations) {
        for (let i = 0; i < mutations.length; i++) {
          const mutation = mutations[i];
          if (mutation.attributeName == "class") {
            if (firstRow.classList.contains("hidden")) {
              observer.disconnect();
              resolve();
              break;
            }
          }
        }
      });

      observer.observe(firstRow, { attributes: true });
    });
  });
}

function normalizeNewLines(value) {
  return value.replace("(\r\n|\n)", "\n");
}
