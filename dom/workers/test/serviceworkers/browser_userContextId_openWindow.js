let { classes: Cc, interfaces: Ci, manager: Cm, results: Cr } = Components;

let swm = Cc["@mozilla.org/serviceworkers/manager;1"].
          getService(Ci.nsIServiceWorkerManager);

const URI = "https://example.com/browser/dom/workers/test/serviceworkers/empty.html";
const MOCK_CID = Components.ID("{2a0f83c4-8818-4914-a184-f1172b4eaaa7}");
const ALERTS_SERVICE_CONTRACT_ID = "@mozilla.org/alerts-service;1";
const USER_CONTEXT_ID = 3

let mockAlertsService = {
  showAlert: function(alert, alertListener) {
    ok(true, "Showing alert");
    setTimeout(function () {
      alertListener.observe(null, "alertshow", alert.cookie);
    }, 100);
    setTimeout(function () {
      alertListener.observe(null, "alertclickcallback", alert.cookie);
    }, 100);
  },

  showAlertNotification: function(imageUrl, title, text, textClickable,
                                  cookie, alertListener, name, dir, lang, data) {
    this.showAlert();
  },

  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsISupports) || aIID.equals(Ci.nsIAlertsService)) {
      return this;
    }
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  createInstance: function(aOuter, aIID) {
    if (aOuter != null) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }
    return this.QueryInterface(aIID);
  }
};

registerCleanupFunction(() => {
  Cm.QueryInterface(Ci.nsIComponentRegistrar).
    unregisterFactory(MOCK_CID, mockAlertsService);
});

add_task(async function setup() {
  // make sure userContext, SW and notifications are enabled.
  await SpecialPowers.pushPrefEnv({"set": [
    ["privacy.userContext.enabled", true],
    ["dom.serviceWorkers.exemptFromPerDomainMax", true],
    ["dom.serviceWorkers.enabled", true],
    ["dom.serviceWorkers.testing.enabled", true],
    ["dom.webnotifications.workers.enabled", true],
    ["dom.webnotifications.serviceworker.enabled", true],
    ["notification.prompt.testing", true],
    ["dom.disable_open_click_delay", 1000],
    ["dom.serviceWorkers.idle_timeout", 299999],
    ["dom.serviceWorkers.idle_extended_timeout", 299999],
    ["browser.link.open_newwindow", 3],
  ]});
});

add_task(async function test() {
  Cm.QueryInterface(Ci.nsIComponentRegistrar).
    registerFactory(MOCK_CID, "alerts service", ALERTS_SERVICE_CONTRACT_ID,
                    mockAlertsService);

  // open the tab in the correct userContextId
  let tab = BrowserTestUtils.addTab(gBrowser, URI, {userContextId: USER_CONTEXT_ID});
  let browser = gBrowser.getBrowserForTab(tab);

  // select tab and make sure its browser is focused
  gBrowser.selectedTab = tab;
  tab.ownerGlobal.focus();

  // wait for tab load
  await BrowserTestUtils.browserLoaded(gBrowser.getBrowserForTab(tab));

  // Waiting for new tab.
  let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, null, true);

  // here the test.
  let uci = await ContentTask.spawn(browser, URI, uri => {
    let uci = content.document.nodePrincipal.userContextId;

    // Registration of the SW
    return content.navigator.serviceWorker.register("file_userContextId_openWindow.js")

    // Activation
    .then(swr => {
      return new content.window.Promise(resolve => {
        let worker = swr.installing;
        worker.addEventListener('statechange', () => {
          if (worker.state === 'activated') {
            resolve(swr);
          }
        });
      });
    })

    // Ask for an openWindow.
    .then(swr => {
      swr.showNotification("testPopup");
      return uci;
    });
  });

  is(uci, USER_CONTEXT_ID, "Tab runs with UCI " + USER_CONTEXT_ID);

  let newTab = await newTabPromise;

  is(newTab.getAttribute("usercontextid"), USER_CONTEXT_ID, "New tab has UCI equal " + USER_CONTEXT_ID);

  // wait for SW unregistration
  uci = await ContentTask.spawn(browser, null, () => {
    let uci = content.document.nodePrincipal.userContextId;

    return content.navigator.serviceWorker.getRegistration(".").then(registration => {
      return registration.unregister();
    })
    .then(() => {
      return uci;
    });
  });

  is(uci, USER_CONTEXT_ID, "Tab runs with UCI " + USER_CONTEXT_ID);

  await BrowserTestUtils.removeTab(newTab);
  await BrowserTestUtils.removeTab(tab);
});
