/**
 * This file contains common functionality for ServiceWorker browser tests.
 *
 * Note that the normal auto-import mechanics for browser mochitests only
 * handles "head.js", but we currently store all of our different varieties of
 * mochitest in a single directory, which potentially results in a collision
 * for similar heuristics for xpcshell.
 *
 * Many of the storage-related helpers in this file come from:
 * https://searchfox.org/mozilla-central/source/dom/localstorage/test/unit/head.js
 **/

// To use this file, explicitly import it via:
//
// Services.scriptloader.loadSubScript("chrome://mochitests/content/browser/dom/serviceworkers/test/browser_head.js", this);

// Find the current parent directory of the test context we're being loaded into
// such that one can do `${originNoTrailingSlash}/${DIR_PATH}/file_in_dir.foo`.
const DIR_PATH = getRootDirectory(gTestPath)
  .replace("chrome://mochitests/content/", "")
  .slice(0, -1);

const SWM = Cc["@mozilla.org/serviceworkers/manager;1"].getService(
  Ci.nsIServiceWorkerManager
);

// The expected minimum usage for an origin that has any Cache API storage in
// use. Currently, the DB uses a page size of 4k and a minimum growth size of
// 32k and has enough tables/indices for this to use 15 pages (61440) which
// rounds up to 64k.  However, we also have to allow for the incremental
// vacuum heuristic only firing if we have more than the growth increment's
// worth of free pages, so we need to set the threshold at 32k * 3.
const kMinimumOriginUsageBytes = 98304;

function getPrincipal(url, attrs) {
  const uri = Services.io.newURI(url);
  if (!attrs) {
    attrs = {};
  }
  return Services.scriptSecurityManager.createContentPrincipal(uri, attrs);
}

async function _qm_requestFinished(request) {
  await new Promise(function (resolve) {
    request.callback = function () {
      resolve();
    };
  });

  if (request.resultCode !== Cr.NS_OK) {
    throw new RequestError(request.resultCode, request.resultName);
  }

  return request.result;
}

async function qm_reset_storage() {
  return new Promise(resolve => {
    let request = Services.qms.reset();
    request.callback = resolve;
  });
}

async function get_qm_origin_usage(origin) {
  return new Promise(resolve => {
    const principal =
      Services.scriptSecurityManager.createContentPrincipalFromOrigin(origin);
    Services.qms.getUsageForPrincipal(principal, request => {
      info(`QM says usage of ${origin} is ${request.result.usage}`);
      resolve(request.result.usage);
    });
  });
}

/**
 * Clear the group associated with the given origin via nsIClearDataService.  We
 * are using nsIClearDataService here because nsIQuotaManagerService doesn't
 * (directly) provide a means of clearing a group.
 */
async function clear_qm_origin_group_via_clearData(origin) {
  const uri = Services.io.newURI(origin);
  const baseDomain = Services.eTLD.getBaseDomain(uri);
  info(`Clearing storage on domain ${baseDomain} (from origin ${origin})`);

  // Initiate group clearing and wait for it.
  await new Promise((resolve, reject) => {
    Services.clearData.deleteDataFromBaseDomain(
      baseDomain,
      false,
      Services.clearData.CLEAR_DOM_QUOTA,
      failedFlags => {
        if (failedFlags) {
          reject(failedFlags);
        } else {
          resolve();
        }
      }
    );
  });
}

/**
 * Look up the nsIServiceWorkerRegistrationInfo for a given SW descriptor.
 */
function swm_lookup_reg(swDesc) {
  // Scopes always include the full origin.
  const fullScope = `${swDesc.origin}/${DIR_PATH}/${swDesc.scope}`;
  const principal = getPrincipal(fullScope);

  const reg = SWM.getRegistrationByPrincipal(principal, fullScope);

  return reg;
}

/**
 * Install a ServiceWorker according to the provided descriptor by opening a
 * fresh tab that will be closed when we are done.  Returns the
 * `nsIServiceWorkerRegistrationInfo` corresponding to the registration.
 *
 * The descriptor may have the following properties:
 * - scope: Optional.
 * - script: The script, which usually just wants to be a relative path.
 * - origin: Requred, the origin (which should not include a trailing slash).
 */
async function install_sw(swDesc) {
  info(
    `Installing ServiceWorker ${swDesc.script} at ${swDesc.scope} on origin ${swDesc.origin}`
  );
  const pageUrlStr = `${swDesc.origin}/${DIR_PATH}/empty_with_utils.html`;

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: pageUrlStr,
    },
    async browser => {
      await SpecialPowers.spawn(
        browser,
        [{ swScript: swDesc.script, swScope: swDesc.scope }],
        async function ({ swScript, swScope }) {
          await content.wrappedJSObject.registerAndWaitForActive(
            swScript,
            swScope
          );
        }
      );
    }
  );
  info(`ServiceWorker installed`);

  return swm_lookup_reg(swDesc);
}

/**
 * Consume storage in the given origin by storing randomly generated Blobs into
 * Cache API storage and IndexedDB storage.  We use both APIs in order to
 * ensure that data clearing wipes both QM clients.
 *
 * Randomly generated Blobs means Blobs with literally random content.  This is
 * done to compensate for the Cache API using snappy for compression.
 */
async function consume_storage(origin, storageDesc) {
  info(`Consuming storage on origin ${origin}`);
  const pageUrlStr = `${origin}/${DIR_PATH}/empty_with_utils.html`;

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: pageUrlStr,
    },
    async browser => {
      await SpecialPowers.spawn(
        browser,
        [storageDesc],
        async function ({ cacheBytes, idbBytes }) {
          await content.wrappedJSObject.fillStorage(cacheBytes, idbBytes);
        }
      );
    }
  );
}

// Check if the origin is effectively empty, but allowing for the minimum size
// Cache API database to be present.
function is_minimum_origin_usage(originUsageBytes) {
  return originUsageBytes <= kMinimumOriginUsageBytes;
}

/**
 * Perform a navigation, waiting until the navigation stops, then returning
 * the `textContent` of the body node.  The expectation is this will be used
 * with ServiceWorkers that return a body that indicates the ServiceWorker
 * provided the result (possibly derived from the request) versus if
 * interception didn't happen.
 */
async function navigate_and_get_body(swDesc, debugTag) {
  let pageUrlStr = `${swDesc.origin}/${DIR_PATH}/${swDesc.scope}`;
  if (debugTag) {
    pageUrlStr += "?" + debugTag;
  }
  info(`Navigating to ${pageUrlStr}`);

  const tabResult = await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: pageUrlStr,
      // In the event of an aborted navigation, the load event will never
      // happen...
      waitForLoad: false,
      // ...but the stop will.
      waitForStateStop: true,
    },
    async browser => {
      info(` Tab opened, querying body content.`);
      const spawnResult = await SpecialPowers.spawn(browser, [], function () {
        const controlled = !!content.navigator.serviceWorker.controller;
        // Special-case about: URL's.
        let loc = content.document.documentURI;
        if (loc.startsWith("about:")) {
          // about:neterror is parameterized by query string, so truncate that
          // off because our tests just care if we're seeing the neterror page.
          const idxQuestion = loc.indexOf("?");
          if (idxQuestion !== -1) {
            loc = loc.substring(0, idxQuestion);
          }
          return { controlled, body: loc };
        }
        return {
          controlled,
          body: content.document?.body?.textContent?.trim(),
        };
      });

      return spawnResult;
    }
  );

  return tabResult;
}

function waitForIframeLoad(iframe) {
  return new Promise(function (resolve) {
    iframe.onload = resolve;
  });
}

function waitForRegister(scope, callback) {
  return new Promise(function (resolve) {
    let listener = {
      onRegister(registration) {
        if (registration.scope !== scope) {
          return;
        }
        SWM.removeListener(listener);
        resolve(callback ? callback(registration) : registration);
      },
    };
    SWM.addListener(listener);
  });
}

function waitForUnregister(scope) {
  return new Promise(function (resolve) {
    let listener = {
      onUnregister(registration) {
        if (registration.scope !== scope) {
          return;
        }
        SWM.removeListener(listener);
        resolve(registration);
      },
    };
    SWM.addListener(listener);
  });
}

// Be careful using this helper function, please make sure QuotaUsageCheck must
// happen, otherwise test would be stucked in this function.
function waitForQuotaUsageCheckFinish(scope) {
  return new Promise(function (resolve) {
    let listener = {
      onQuotaUsageCheckFinish(registration) {
        if (registration.scope !== scope) {
          return;
        }
        SWM.removeListener(listener);
        resolve(registration);
      },
    };
    SWM.addListener(listener);
  });
}

function waitForServiceWorkerRegistrationChange(registration, callback) {
  return new Promise(function (resolve) {
    let listener = {
      onChange() {
        registration.removeListener(listener);
        if (callback) {
          callback();
        }
        resolve(callback ? callback() : undefined);
      },
    };
    registration.addListener(listener);
  });
}

function waitForServiceWorkerShutdown() {
  return new Promise(function (resolve) {
    let observer = {
      observe(subject, topic, data) {
        if (topic !== "service-worker-shutdown") {
          return;
        }
        SpecialPowers.removeObserver(observer, "service-worker-shutdown");
        resolve();
      },
    };
    SpecialPowers.addObserver(observer, "service-worker-shutdown");
  });
}
