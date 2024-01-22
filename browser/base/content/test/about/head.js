ChromeUtils.defineESModuleGetters(this, {
  FormHistory: "resource://gre/modules/FormHistory.sys.mjs",
  SearchTestUtils: "resource://testing-common/SearchTestUtils.sys.mjs",
});

SearchTestUtils.init(this);

function getCertChainAsString(certBase64Array) {
  let certChain = "";
  for (let cert of certBase64Array) {
    certChain += getPEMString(cert);
  }
  return certChain;
}

function getPEMString(derb64) {
  // Wrap the Base64 string into lines of 64 characters,
  // with CRLF line breaks (as specified in RFC 1421).
  var wrapped = derb64.replace(/(\S{64}(?!$))/g, "$1\r\n");
  return (
    "-----BEGIN CERTIFICATE-----\r\n" +
    wrapped +
    "\r\n-----END CERTIFICATE-----\r\n"
  );
}

async function injectErrorPageFrame(tab, src, sandboxed) {
  let loadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    true,
    null,
    true
  );

  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [src, sandboxed],
    async function (frameSrc, frameSandboxed) {
      let iframe = content.document.createElement("iframe");
      iframe.src = frameSrc;
      if (frameSandboxed) {
        iframe.setAttribute("sandbox", "allow-scripts");
      }
      content.document.body.appendChild(iframe);
    }
  );

  await loadedPromise;
}

async function openErrorPage(src, useFrame, sandboxed) {
  let dummyPage =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.com"
    ) + "dummy_page.html";

  let tab;
  if (useFrame) {
    info("Loading cert error page in an iframe");
    tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, dummyPage);
    await injectErrorPageFrame(tab, src, sandboxed);
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
  var interval = setInterval(function () {
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
  var moveOn = function () {
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
    BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, url);
  }

  return loaded;
}

/**
 * Wait for the user's default search engine to change. searchEngineChangeFn is
 * a function that will be called to change the search engine.
 */
async function promiseContentSearchChange(browser, searchEngineChangeFn) {
  // Add an event listener manually then perform the action, rather than using
  // BrowserTestUtils.addContentEventListener as that doesn't add the listener
  // early enough.
  await SpecialPowers.spawn(browser, [], async () => {
    // Store the results in a temporary place.
    content._searchDetails = {
      defaultEnginesList: [],
      listener: event => {
        if (event.detail.type == "CurrentEngine") {
          content._searchDetails.defaultEnginesList.push(
            content.wrappedJSObject.gContentSearchController.defaultEngine.name
          );
        }
      },
    };

    // Listen using the system group to ensure that it fires after
    // the default behaviour.
    content.addEventListener(
      "ContentSearchService",
      content._searchDetails.listener,
      { mozSystemGroup: true }
    );
  });

  let expectedEngineName = await searchEngineChangeFn();

  await SpecialPowers.spawn(
    browser,
    [expectedEngineName],
    async expectedEngineNameChild => {
      await ContentTaskUtils.waitForCondition(
        () =>
          content._searchDetails.defaultEnginesList &&
          content._searchDetails.defaultEnginesList[
            content._searchDetails.defaultEnginesList.length - 1
          ] == expectedEngineNameChild,
        `Waiting for ${expectedEngineNameChild} to be set`
      );
      content.removeEventListener(
        "ContentSearchService",
        content._searchDetails.listener,
        { mozSystemGroup: true }
      );
      delete content._searchDetails;
    }
  );
}

async function waitForBookmarksToolbarVisibility({
  win = window,
  visible,
  message,
}) {
  let result = await TestUtils.waitForCondition(() => {
    let toolbar = win.document.getElementById("PersonalToolbar");
    return toolbar && (visible ? !toolbar.collapsed : toolbar.collapsed);
  }, message || "waiting for toolbar to become " + (visible ? "visible" : "hidden"));
  ok(result, message);
  return result;
}

function isBookmarksToolbarVisible(win = window) {
  let toolbar = win.document.getElementById("PersonalToolbar");
  return !toolbar.collapsed;
}
