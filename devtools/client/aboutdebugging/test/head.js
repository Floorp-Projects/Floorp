/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env browser */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../shared/test/shared-head.js */

"use strict";

// Load the shared-head file first.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this);

const { AddonManager } = ChromeUtils.import("resource://gre/modules/AddonManager.jsm");
const { Management } = ChromeUtils.import("resource://gre/modules/Extension.jsm", null);
const { ExtensionTestCommon } = ChromeUtils.import("resource://testing-common/ExtensionTestCommon.jsm");

async function openAboutDebugging(page, win) {
  info("Turn off the new about:debugging for the test");
  await pushPref("devtools.aboutdebugging.new-enabled", false);

  info("opening about:debugging");
  let url = "about:debugging";
  if (page) {
    url += "#" + page;
  }

  const tab = await addTab(url, { window: win });
  const browser = tab.linkedBrowser;
  const document = browser.contentDocument;
  const window = browser.contentWindow;

  info("Wait until the main about debugging container is available");
  await waitUntilElement(".app", document);

  return { tab, document, window };
}

function closeAboutDebugging(tab) {
  info("Closing about:debugging");
  return removeTab(tab);
}

function getSupportsFile(path) {
  const cr = Cc["@mozilla.org/chrome/chrome-registry;1"]
    .getService(Ci.nsIChromeRegistry);
  const uri = Services.io.newURI(CHROME_URL_ROOT + path);
  const fileurl = cr.convertChromeURL(uri);
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
  const nameElements = [...document.querySelectorAll("#service-workers .target-name")];
  const nameElement = nameElements.filter(element => element.textContent === name)[0];
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
async function waitUntilServiceWorkerContainer(name, document) {
  await waitUntil(() => {
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
async function waitUntilElement(selector, parent) {
  await waitUntil(() => {
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

async function installAddon({document, path, file, name}) {
  // Mock the file picker to select a test addon
  const MockFilePicker = SpecialPowers.MockFilePicker;
  MockFilePicker.init(window);
  if (path) {
    file = getSupportsFile(path);
    MockFilePicker.setFiles([file.file]);
  } else {
    MockFilePicker.setFiles([file]);
  }

  const onAddonInstalled = new Promise(done => {
    Management.on("startup", function listener(event, extension) {
      if (extension.name != name) {
        return;
      }

      Management.off("startup", listener);
      done();
    });
  });
  const AboutDebugging = document.defaultView.AboutDebugging;

  // List updated twice:
  // - AddonManager's onInstalled event
  // - WebExtension's Management's startup event.
  const onListUpdated = waitForNEvents(AboutDebugging, "addons-updated", 2);

  // Trigger the file picker by clicking on the button
  document.getElementById("load-addon-from-file").click();

  await onListUpdated;
  await onAddonInstalled;
  ok(true, "Addon installed and running its bootstrap.js file");

  info("Wait for the addon to appear in the UI");
  await waitUntilAddonContainer(name, document);
}

async function uninstallAddon({document, id, name}) {
  // Now uninstall this addon
  await new Promise(async done => {
    const addon = await AddonManager.getAddonByID(id);
    const listener = {
      onUninstalled: function(uninstalledAddon) {
        if (uninstalledAddon != addon) {
          return;
        }
        AddonManager.removeAddonListener(listener);

        done();
      },
    };
    AddonManager.addAddonListener(listener);
    addon.uninstall();
  });

  info("Wait until the addon is removed from about:debugging");
  await waitUntil(() => !getAddonContainer(name, document), 100);
}

function getAddonCount(document) {
  const addonListContainer = getAddonList(document);
  const addonElements = addonListContainer.querySelectorAll(".target");
  return addonElements.length;
}

/**
 * Returns a promise that will resolve when the add-on list has been updated.
 *
 * @param {Node} document
 * @return {Promise}
 */
function waitForInitialAddonList(document) {
  info("Waiting for add-ons to load. Current add-on count: " + getAddonCount(document));
  return waitUntil(() => getAddonCount(document) > 0, 100);
}

function getAddonContainer(name, document) {
  const nameElements = [...document.querySelectorAll("#addons-panel .target-name")];
  const nameElement = nameElements.filter(element => element.textContent === name)[0];
  if (nameElement) {
    return nameElement.closest(".addon-target-container");
  }

  return null;
}

async function waitUntilAddonContainer(name, document) {
  await waitUntil(() => {
    return getAddonContainer(name, document);
  });
  return getAddonContainer(name, document);
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
  return ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    // Retrieve the `sw` promise created in the html page.
    const { sw } = content.wrappedJSObject;
    await sw;
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
async function unregisterServiceWorker(tab, serviceWorkersElement) {
  // Get the initial count of service worker registrations.
  let registrations = serviceWorkersElement.querySelectorAll(".target-container");
  const registrationCount = registrations.length;

  // Wait until the registration count is decreased by one.
  const isRemoved = waitUntil(() => {
    registrations = serviceWorkersElement.querySelectorAll(".target-container");
    return registrations.length === registrationCount - 1;
  }, 100);

  // Unregister the service worker from the content page
  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    // Retrieve the `sw` promise created in the html page
    const { sw } = content.wrappedJSObject;
    const registration = await sw;
    await registration.unregister();
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
  return new Promise(function(resolve) {
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
async function setupTestAboutDebuggingWebExtension(name, file) {
  await new Promise(resolve => {
    const options = {"set": [
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

  const { tab, document } = await openAboutDebugging("addons");
  await waitForInitialAddonList(document);

  await installAddon({
    document,
    file,
    name,
  });

  // Retrieve the DEBUG button for the addon
  const names = getInstalledAddonNames(document);
  const nameEl = names.filter(element => element.textContent === name)[0];
  ok(name, "Found the addon in the list");
  const targetElement = nameEl.parentNode.parentNode;
  const debugBtn = targetElement.querySelector(".debug-button");
  ok(debugBtn, "Found its debug button");

  return { tab, document, debugBtn };
}

/**
 * Wait for aboutdebugging to be notified about the activation of the service worker
 * corresponding to the provided service worker url.
 */
async function waitForServiceWorkerActivation(swUrl, document) {
  const serviceWorkersElement = getServiceWorkerList(document);
  const names = serviceWorkersElement.querySelectorAll(".target-name");
  const name = [...names].filter(element => element.textContent === swUrl)[0];

  const targetElement = name.parentNode.parentNode;
  const targetStatus = targetElement.querySelector(".target-status");
  await waitUntil(() => {
    return targetStatus.textContent !== "Registering";
  }, 100);
}

/**
 * Set all preferences needed to enable service worker debugging and testing.
 */
async function enableServiceWorkerDebugging() {
  const options = { "set": [
    // Enable service workers.
    ["dom.serviceWorkers.enabled", true],
    // Accept workers from mochitest's http.
    ["dom.serviceWorkers.testing.enabled", true],
    // Force single content process.
    ["dom.ipc.processCount", 1],
  ]};

  // Wait for dom.ipc.processCount to be updated before releasing processes.
  await new Promise(done => {
    SpecialPowers.pushPrefEnv(options, done);
  });

  Services.ppmm.releaseCachedProcesses();
}

/**
 * Returns a promise that resolves when the given add-on event is fired. The
 * resolved value is an array of arguments passed for the event.
 */
function promiseAddonEvent(event) {
  return new Promise(resolve => {
    const listener = {
      [event]: function(...args) {
        AddonManager.removeAddonListener(listener);
        resolve(args);
      },
    };

    AddonManager.addAddonListener(listener);
  });
}

/**
 * Install an add-on using the AddonManager so it does not show up as temporary.
 */
function installAddonWithManager(filePath) {
  return new Promise(async (resolve, reject) => {
    const install = await AddonManager.getInstallForFile(filePath);
    if (!install) {
      throw new Error(`An install was not created for ${filePath}`);
    }
    install.addListener({
      onDownloadFailed: reject,
      onDownloadCancelled: reject,
      onInstallFailed: reject,
      onInstallCancelled: reject,
      onInstallEnded: resolve,
    });
    install.install();
  });
}

function getAddonByID(addonId) {
  return AddonManager.getAddonByID(addonId);
}

/**
 * Uninstall an add-on.
 */
async function tearDownAddon(AboutDebugging, addon) {
  const onUninstalled = promiseAddonEvent("onUninstalled");
  const onListUpdated = once(AboutDebugging, "addons-updated");
  addon.uninstall();
  await onListUpdated;
  const [uninstalledAddon] = await onUninstalled;
  is(uninstalledAddon.id, addon.id,
     `Add-on was uninstalled: ${uninstalledAddon.id}`);
}
