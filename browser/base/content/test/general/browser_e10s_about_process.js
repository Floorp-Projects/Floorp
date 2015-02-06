const CHROME_PROCESS = Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
const CONTENT_PROCESS = Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT;

const CHROME = {
  id: "cb34538a-d9da-40f3-b61a-069f0b2cb9fb",
  path: "test-chrome",
  flags: 0,
}
const CANREMOTE = {
  id: "2480d3e1-9ce4-4b84-8ae3-910b9a95cbb3",
  path: "test-allowremote",
  flags: Ci.nsIAboutModule.URI_CAN_LOAD_IN_CHILD,
}
const MUSTREMOTE = {
  id: "f849cee5-e13e-44d2-981d-0fb3884aaead",
  path: "test-mustremote",
  flags: Ci.nsIAboutModule.URI_MUST_LOAD_IN_CHILD,
}

const TEST_MODULES = [
  CHROME,
  CANREMOTE,
  MUSTREMOTE
]

function AboutModule() {
}

AboutModule.prototype = {
  newChannel: function(aURI, aLoadInfo) {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },

  getURIFlags: function(aURI) {
    for (let module of TEST_MODULES) {
      if (aURI.path.startsWith(module.path)) {
        return module.flags;
      }
    }

    ok(false, "Called getURIFlags for an unknown page " + aURI.spec);
    return 0;
  },

  getIndexedDBOriginPostfix: function(aURI) {
    return null;
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAboutModule])
};

let AboutModuleFactory = {
  createInstance: function(aOuter, aIID) {
    if (aOuter)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return new AboutModule().QueryInterface(aIID);
  },

  lockFactory: function(aLock) {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFactory])
};

add_task(function* init() {
  let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  for (let module of TEST_MODULES) {
    registrar.registerFactory(Components.ID(module.id), "",
                              "@mozilla.org/network/protocol/about;1?what=" + module.path,
                              AboutModuleFactory);
  }
});

registerCleanupFunction(() => {
  let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  for (let module of TEST_MODULES) {
    registrar.unregisterFactory(Components.ID(module.id), AboutModuleFactory);
  }
});

function test_url(url, chromeResult, contentResult) {
  is(E10SUtils.canLoadURIInProcess(url, CHROME_PROCESS),
     chromeResult, "Check URL in chrome process.");
  is(E10SUtils.canLoadURIInProcess(url, CONTENT_PROCESS),
     contentResult, "Check URL in content process.");

  is(E10SUtils.canLoadURIInProcess(url + "#foo", CHROME_PROCESS),
     chromeResult, "Check URL with ref in chrome process.");
  is(E10SUtils.canLoadURIInProcess(url + "#foo", CONTENT_PROCESS),
     contentResult, "Check URL with ref in content process.");

  is(E10SUtils.canLoadURIInProcess(url + "?foo", CHROME_PROCESS),
     chromeResult, "Check URL with query in chrome process.");
  is(E10SUtils.canLoadURIInProcess(url + "?foo", CONTENT_PROCESS),
     contentResult, "Check URL with query in content process.");

  is(E10SUtils.canLoadURIInProcess(url + "?foo#bar", CHROME_PROCESS),
     chromeResult, "Check URL with query and ref in chrome process.");
  is(E10SUtils.canLoadURIInProcess(url + "?foo#bar", CONTENT_PROCESS),
     contentResult, "Check URL with query and ref in content process.");
}

add_task(function* test_chrome() {
  test_url("about:" + CHROME.path, true, false);
});

add_task(function* test_any() {
  test_url("about:" + CANREMOTE.path, true, true);
});

add_task(function* test_remote() {
  test_url("about:" + MUSTREMOTE.path, false, true);
});
