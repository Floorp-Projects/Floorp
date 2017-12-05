/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env browser */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../framework/test/shared-head.js */

"use strict";

// Load the shared-head file first.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/test/shared-head.js",
  this);

const { AddonManager } = Cu.import("resource://gre/modules/AddonManager.jsm", {});
const { Management } = Cu.import("resource://gre/modules/Extension.jsm", {});

flags.testing = true;
registerCleanupFunction(() => {
  flags.testing = false;
});

function* openAboutDebugging(page, win) {
  info("opening about:debugging");
  let url = "about:debugging";
  if (page) {
    url += "#" + page;
  }

  let tab = yield addTab(url, { window: win });
  let browser = tab.linkedBrowser;
  let document = browser.contentDocument;
  let window = browser.contentWindow;

  if (!document.querySelector(".app")) {
    yield waitForMutation(document.body, { childList: true });
  }

  return { tab, document, window };
}

/**
 * Change url hash for current about:debugging tab, return a promise after
 * new content is loaded.
 * @param  {DOMDocument}  document   container document from current tab
 * @param  {String}       hash       hash for about:debugging
 * @return {Promise}
 */
function changeAboutDebuggingHash(document, hash) {
  info(`Opening about:debugging#${hash}`);
  window.openUILinkIn(`about:debugging#${hash}`, "current");
  return waitForMutation(
    document.querySelector(".main-content"), {childList: true});
}

function openPanel(document, panelId) {
  info(`Opening ${panelId} panel`);
  document.querySelector(`[aria-controls="${panelId}"]`).click();
  return waitForMutation(
    document.querySelector(".main-content"), {childList: true});
}

function closeAboutDebugging(tab) {
  info("Closing about:debugging");
  return removeTab(tab);
}

function getSupportsFile(path) {
  let cr = Cc["@mozilla.org/chrome/chrome-registry;1"]
    .getService(Ci.nsIChromeRegistry);
  let uri = Services.io.newURI(CHROME_URL_ROOT + path);
  let fileurl = cr.convertChromeURL(uri);
  return fileurl.QueryInterface(Ci.nsIFileURL);
}

/**
 * Depending on whether there are addons installed, return either a target list
 * element or its container.
 * @param  {DOMDocument}  document   #addons section container document
 * @return {DOMNode}                 target list or container element
 */
function getAddonList(document) {
  return document.querySelector("#addons .target-list") ||
    document.querySelector("#addons .targets");
}

/**
 * Depending on whether there are temporary addons installed, return either a
 * target list element or its container.
 * @param  {DOMDocument}  document   #temporary-addons section container document
 * @return {DOMNode}                 target list or container element
 */
function getTemporaryAddonList(document) {
  return document.querySelector("#temporary-addons .target-list") ||
    document.querySelector("#temporary-addons .targets");
}

/**
 * Depending on whether the addon is installed, return either the addon list
 * element or throw an Error.
 * @param  {DOMDocument}  document   addon section container document
 * @return {DOMNode}                 target list
 * @throws {Error}                   add-on not found error
 */
function getAddonListWithAddon(document, id) {
  const addon = document.querySelector(`[data-addon-id="${id}"]`);
  if (!addon) {
    throw new Error("couldn't find add-on by id");
  }
  return addon.closest(".target-list");
}

function getInstalledAddonNames(document) {
  const selector = "#addons .target-name, #temporary-addons .target-name";
  return [...document.querySelectorAll(selector)];
}

/**
 * Depending on whether there are service workers installed, return either a
 * target list element or its container.
 * @param  {DOMDocument}  document   #service-workers section container document
 * @return {DOMNode}                 target list or container element
 */
function getServiceWorkerList(document) {
  return document.querySelector("#service-workers .target-list") ||
    document.querySelector("#service-workers.targets");
}

/**
 * Retrieve the container element for the service worker corresponding to the provided
 * name.
 *
 * @param  {String} name
 *         expected service worker name
 * @param  {DOMDocument} document
 *         #service-workers section container document
 * @return {DOMNode} container element
 */
function getServiceWorkerContainer(name, document) {
  let nameElements = [...document.querySelectorAll("#service-workers .target-name")];
  let nameElement = nameElements.filter(element => element.textContent === name)[0];
  if (nameElement) {
    return nameElement.closest(".target-container");
  }

  return null;
}

/**
 * Wait until a service worker "container" element is found with a specific service worker
 * name, in the provided document.
 * Returns a promise that resolves the service worker container element.
 *
 * @param  {String} name
 *         expected service worker name
 * @param  {DOMDocument} document
 *         #service-workers section container document
 * @return {Promise} promise that resolves the service worker container element.
 */
function* waitUntilServiceWorkerContainer(name, document) {
  yield waitUntil(() => {
    return getServiceWorkerContainer(name, document);
  }, 100);
  return getServiceWorkerContainer(name, document);
}

/**
 * Wait until a selector matches an element in a given parent node.
 * Returns a promise that resolves the matched element.
 *
 * @param {String} selector
 *        CSS selector to match.
 * @param {DOMNode} parent
 *        Parent that should contain the element.
 * @return {Promise} promise that resolves the matched DOMNode.
 */
function* waitUntilElement(selector, parent) {
  yield waitUntil(() => {
    return parent.querySelector(selector);
  }, 100);
  return parent.querySelector(selector);
}

/**
 * Depending on whether there are tabs opened, return either a
 * target list element or its container.
 * @param  {DOMDocument}  document   #tabs section container document
 * @return {DOMNode}                 target list or container element
 */
function getTabList(document) {
  return document.querySelector("#tabs .target-list") ||
    document.querySelector("#tabs.targets");
}

function* installAddon({document, path, name, isWebExtension}) {
  // Mock the file picker to select a test addon
  let MockFilePicker = SpecialPowers.MockFilePicker;
  MockFilePicker.init(window);
  let file = getSupportsFile(path);
  MockFilePicker.setFiles([file.file]);

  let addonList = getTemporaryAddonList(document);
  let addonListMutation = waitForMutation(addonList, { childList: true });

  let onAddonInstalled;

  if (isWebExtension) {
    onAddonInstalled = new Promise(done => {
      Management.on("startup", function listener(event, extension) {
        if (extension.name != name) {
          return;
        }

        Management.off("startup", listener);
        done();
      });
    });
  } else {
    // Wait for a "test-devtools" message sent by the addon's bootstrap.js file
    onAddonInstalled = new Promise(done => {
      Services.obs.addObserver(function listener() {
        Services.obs.removeObserver(listener, "test-devtools");

        done();
      }, "test-devtools");
    });
  }
  // Trigger the file picker by clicking on the button
  document.getElementById("load-addon-from-file").click();

  yield onAddonInstalled;
  ok(true, "Addon installed and running its bootstrap.js file");

  // Check that the addon appears in the UI
  yield addonListMutation;
  let names = [...addonList.querySelectorAll(".target-name")];
  names = names.map(element => element.textContent);
  ok(names.includes(name),
    "The addon name appears in the list of addons: " + names);
}

function* uninstallAddon({document, id, name}) {
  let addonList = getAddonListWithAddon(document, id);
  let addonListMutation = waitForMutation(addonList.parentNode,
                                          { childList: true, subtree: true });

  // Now uninstall this addon
  yield new Promise(done => {
    AddonManager.getAddonByID(id, addon => {
      let listener = {
        onUninstalled: function (uninstalledAddon) {
          if (uninstalledAddon != addon) {
            return;
          }
          AddonManager.removeAddonListener(listener);

          done();
        }
      };
      AddonManager.addAddonListener(listener);
      addon.uninstall();
    });
  });

  yield addonListMutation;

  // If parentNode is none, that means the entire addonList was removed from the
  // document. This happens when the addon we are removing is the last one.
  if (addonList.parentNode !== null) {
    // Ensure that the UI removes the addon from the list
    let names = [...addonList.querySelectorAll(".target-name")];
    names = names.map(element => element.textContent);
    ok(!names.includes(name),
      "After uninstall, the addon name disappears from the list of addons: "
      + names);
  }
}

/**
 * Returns a promise that will resolve when the add-on list has been updated.
 *
 * @param {Node} document
 * @return {Promise}
 */
function waitForInitialAddonList(document) {
  const addonListContainer = getAddonList(document);
  let addonCount = addonListContainer.querySelectorAll(".target");
  addonCount = addonCount ? [...addonCount].length : -1;
  info("Waiting for add-ons to load. Current add-on count: " + addonCount);

  // This relies on the network speed of the actor responding to the
  // listAddons() request and also the speed of openAboutDebugging().
  let result;
  if (addonCount > 0) {
    info("Actually, the add-ons have already loaded");
    result = Promise.resolve();
  } else {
    result = waitForMutation(addonListContainer, { childList: true });
  }
  return result;
}

function waitForInstallMessages(target) {
  return new Promise(resolve => {
    let observer = new MutationObserver((mutations) => {
      const messageAdded = mutations.some((mutation) => {
        return [...mutation.addedNodes].some((node) => {
          return node.classList.contains("addon-target-messages");
        });
      });
      if (messageAdded) {
        observer.disconnect();
        resolve();
      }
    });
    observer.observe(target, { childList: true });
  });
}

/**
 * Returns a promise that will resolve after receiving a mutation matching the
 * provided mutation options on the provided target.
 * @param {Node} target
 * @param {Object} mutationOptions
 * @return {Promise}
 */
function waitForMutation(target, mutationOptions) {
  return new Promise(resolve => {
    let observer = new MutationObserver(() => {
      observer.disconnect();
      resolve();
    });
    observer.observe(target, mutationOptions);
  });
}

/**
 * Returns a promise that will resolve after receiving a mutation in the subtree of the
 * provided target. Depending on the current React implementation, a text change might be
 * observable as a childList mutation or a characterData mutation.
 *
 * @param {Node} target
 * @return {Promise}
 */
function waitForContentMutation(target) {
  return waitForMutation(target, {
    characterData: true,
    childList: true,
    subtree: true
  });
}

/**
 * Checks if an about:debugging TargetList element contains a Target element
 * corresponding to the specified name.
 * @param {Boolean} expected
 * @param {Document} document
 * @param {String} type
 * @param {String} name
 */
function assertHasTarget(expected, document, type, name) {
  let names = [...document.querySelectorAll("#" + type + " .target-name")];
  names = names.map(element => element.textContent);
  is(names.includes(name), expected,
    "The " + type + " url appears in the list: " + names);
}

/**
 * Returns a promise that will resolve after the service worker in the page
 * has successfully registered itself.
 * @param {Tab} tab
 * @return {Promise} Resolves when the service worker is registered.
 */
function waitForServiceWorkerRegistered(tab) {
  return ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    // Retrieve the `sw` promise created in the html page.
    let { sw } = content.wrappedJSObject;
    yield sw;
  });
}

/**
 * Asks the service worker within the test page to unregister, and returns a
 * promise that will resolve when it has successfully unregistered itself and the
 * about:debugging UI has fully processed this update.
 *
 * @param {Tab} tab
 * @param {Node} serviceWorkersElement
 * @return {Promise} Resolves when the service worker is unregistered.
 */
function* unregisterServiceWorker(tab, serviceWorkersElement) {
  // Get the initial count of service worker registrations.
  let registrations = serviceWorkersElement.querySelectorAll(".target-container");
  let registrationCount = registrations.length;

  // Wait until the registration count is decreased by one.
  let isRemoved = waitUntil(() => {
    registrations = serviceWorkersElement.querySelectorAll(".target-container");
    return registrations.length === registrationCount - 1;
  }, 100);

  // Unregister the service worker from the content page
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    // Retrieve the `sw` promise created in the html page
    let { sw } = content.wrappedJSObject;
    let registration = yield sw;
    yield registration.unregister();
  });

  return isRemoved;
}

/**
 * Waits for the creation of a new window, usually used with create private
 * browsing window.
 * Returns a promise that will resolve when the window is successfully created.
 * @param {window} win
 */
function waitForDelayedStartupFinished(win) {
  return new Promise(function (resolve) {
    Services.obs.addObserver(function observer(subject, topic) {
      if (win == subject) {
        Services.obs.removeObserver(observer, topic);
        resolve();
      }
    }, "browser-delayed-startup-finished");
  });
}

/**
 * open the about:debugging page and install an addon
 */
function* setupTestAboutDebuggingWebExtension(name, path) {
  yield new Promise(resolve => {
    let options = {"set": [
      // Force enabling of addons debugging
      ["devtools.chrome.enabled", true],
      ["devtools.debugger.remote-enabled", true],
      // Disable security prompt
      ["devtools.debugger.prompt-connection", false],
      // Enable Browser toolbox test script execution via env variable
      ["devtools.browser-toolbox.allow-unsafe-script", true],
    ]};
    SpecialPowers.pushPrefEnv(options, resolve);
  });

  let { tab, document } = yield openAboutDebugging("addons");
  yield waitForInitialAddonList(document);

  yield installAddon({
    document,
    path,
    name,
    isWebExtension: true,
  });

  // Retrieve the DEBUG button for the addon
  let names = getInstalledAddonNames(document);
  let nameEl = names.filter(element => element.textContent === name)[0];
  ok(name, "Found the addon in the list");
  let targetElement = nameEl.parentNode.parentNode;
  let debugBtn = targetElement.querySelector(".debug-button");
  ok(debugBtn, "Found its debug button");

  return { tab, document, debugBtn };
}

/**
 * Wait for aboutdebugging to be notified about the activation of the service worker
 * corresponding to the provided service worker url.
 */
function* waitForServiceWorkerActivation(swUrl, document) {
  let serviceWorkersElement = getServiceWorkerList(document);
  let names = serviceWorkersElement.querySelectorAll(".target-name");
  let name = [...names].filter(element => element.textContent === swUrl)[0];

  let targetElement = name.parentNode.parentNode;
  let targetStatus = targetElement.querySelector(".target-status");
  yield waitUntil(() => {
    return targetStatus.textContent !== "Registering";
  }, 100);
}

/**
 * Set all preferences needed to enable service worker debugging and testing.
 */
function enableServiceWorkerDebugging() {
  return new Promise(done => {
    let options = { "set": [
      // Enable service workers.
      ["dom.serviceWorkers.enabled", true],
      // Accept workers from mochitest's http.
      ["dom.serviceWorkers.testing.enabled", true],
      // Force single content process.
      ["dom.ipc.processCount", 1],
    ]};
    SpecialPowers.pushPrefEnv(options, done);
    Services.ppmm.releaseCachedProcesses();
  });
}

/**
 * Returns a promise that resolves when the given add-on event is fired. The
 * resolved value is an array of arguments passed for the event.
 */
function promiseAddonEvent(event) {
  return new Promise(resolve => {
    let listener = {
      [event]: function (...args) {
        AddonManager.removeAddonListener(listener);
        resolve(args);
      }
    };

    AddonManager.addAddonListener(listener);
  });
}

/**
 * Install an add-on using the AddonManager so it does not show up as temporary.
 */
function installAddonWithManager(filePath) {
  return new Promise((resolve, reject) => {
    AddonManager.getInstallForFile(filePath, install => {
      if (!install) {
        throw new Error(`An install was not created for ${filePath}`);
      }
      install.addListener({
        onDownloadFailed: reject,
        onDownloadCancelled: reject,
        onInstallFailed: reject,
        onInstallCancelled: reject,
        onInstallEnded: resolve
      });
      install.install();
    });
  });
}

function getAddonByID(addonId) {
  return new Promise(resolve => {
    AddonManager.getAddonByID(addonId, addon => resolve(addon));
  });
}

/**
 * Uninstall an add-on.
 */
function* tearDownAddon(addon) {
  const onUninstalled = promiseAddonEvent("onUninstalled");
  addon.uninstall();
  const [uninstalledAddon] = yield onUninstalled;
  is(uninstalledAddon.id, addon.id,
     `Add-on was uninstalled: ${uninstalledAddon.id}`);
}
