/* eslint-env mozilla/frame-script */

XPCOMUtils.defineLazyModuleGetters(this, {
  FormHistory: "resource://gre/modules/FormHistory.jsm",
});

function getSecurityInfo(securityInfoAsString) {
  const serhelper = Cc[
    "@mozilla.org/network/serialization-helper;1"
  ].getService(Ci.nsISerializationHelper);
  let securityInfo = serhelper.deserializeObject(securityInfoAsString);
  securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
  return securityInfo;
}

function getCertChain(securityInfoAsString) {
  let certChain = "";
  let securityInfo = getSecurityInfo(securityInfoAsString);
  for (let cert of securityInfo.failedCertChain.getEnumerator()) {
    certChain += getPEMString(cert);
  }
  return certChain;
}

function getPEMString(cert) {
  var derb64 = cert.getBase64DERString();
  // Wrap the Base64 string into lines of 64 characters,
  // with CRLF line breaks (as specified in RFC 1421).
  var wrapped = derb64.replace(/(\S{64}(?!$))/g, "$1\r\n");
  return (
    "-----BEGIN CERTIFICATE-----\r\n" +
    wrapped +
    "\r\n-----END CERTIFICATE-----\r\n"
  );
}

function injectErrorPageFrame(tab, src) {
  return ContentTask.spawn(
    tab.linkedBrowser,
    { frameSrc: src },
    async function({ frameSrc }) {
      let loaded = ContentTaskUtils.waitForEvent(
        content.wrappedJSObject,
        "DOMFrameContentLoaded"
      );
      let iframe = content.document.createElement("iframe");
      iframe.src = frameSrc;
      content.document.body.appendChild(iframe);
      await loaded;
      // We will have race conditions when accessing the frame content after setting a src,
      // so we can't wait for AboutNetErrorLoad. Let's wait for the certerror class to
      // appear instead (which should happen at the same time as AboutNetErrorLoad).
      await ContentTaskUtils.waitForCondition(() =>
        iframe.contentDocument.body.classList.contains("certerror")
      );
    }
  );
}

async function openErrorPage(src, useFrame) {
  let dummyPage =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.com"
    ) + "dummy_page.html";

  let tab;
  if (useFrame) {
    info("Loading cert error page in an iframe");
    tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, dummyPage);
    await injectErrorPageFrame(tab, src);
  } else {
    let certErrorLoaded;
    tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      () => {
        gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, src);
        let browser = gBrowser.selectedBrowser;
        certErrorLoaded = BrowserTestUtils.waitForErrorPage(browser);
      },
      false
    );
    info("Loading and waiting for the cert error");
    await certErrorLoaded;
  }

  return tab;
}

function waitForCondition(condition, nextTest, errorMsg, retryTimes) {
  retryTimes = typeof retryTimes !== "undefined" ? retryTimes : 30;
  var tries = 0;
  var interval = setInterval(function() {
    if (tries >= retryTimes) {
      ok(false, errorMsg);
      moveOn();
    }
    var conditionPassed;
    try {
      conditionPassed = condition();
    } catch (e) {
      ok(false, e + "\n" + e.stack);
      conditionPassed = false;
    }
    if (conditionPassed) {
      moveOn();
    }
    tries++;
  }, 100);
  var moveOn = function() {
    clearInterval(interval);
    nextTest();
  };
}

function whenTabLoaded(aTab, aCallback) {
  promiseTabLoadEvent(aTab).then(aCallback);
}

function promiseTabLoaded(aTab) {
  return new Promise(resolve => {
    whenTabLoaded(aTab, resolve);
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

/**
 * Wait for the search engine to change.
 */
function promiseContentSearchChange(browser, newEngineName) {
  return ContentTask.spawn(browser, { newEngineName }, async function(args) {
    return new Promise(resolve => {
      content.addEventListener("ContentSearchService", function listener(
        aEvent
      ) {
        if (
          aEvent.detail.type == "CurrentState" &&
          content.wrappedJSObject.gContentSearchController.defaultEngine.name ==
            args.newEngineName
        ) {
          content.removeEventListener("ContentSearchService", listener);
          resolve();
        }
      });
    });
  });
}

/**
 * Wait for the search engine to be added.
 */
async function promiseNewEngine(basename) {
  info("Waiting for engine to be added: " + basename);
  let url = getRootDirectory(gTestPath) + basename;
  let engine;
  try {
    engine = await Services.search.addEngine(url, "", false);
  } catch (errCode) {
    ok(false, "addEngine failed with error code " + errCode);
    throw errCode;
  }

  info("Search engine added: " + basename);
  registerCleanupFunction(async () => {
    try {
      await Services.search.removeEngine(engine);
    } catch (ex) {
      /* Can't remove the engine more than once */
    }
  });

  return engine;
}
