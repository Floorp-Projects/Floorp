const CHROME_PROCESS = E10SUtils.NOT_REMOTE;
const WEB_CONTENT_PROCESS = E10SUtils.WEB_REMOTE_TYPE;
const PRIVILEGED_CONTENT_PROCESS = E10SUtils.PRIVILEGED_REMOTE_TYPE;
const EXTENSION_PROCESS = E10SUtils.EXTENSION_REMOTE_TYPE;

const CHROME = {
  id: "cb34538a-d9da-40f3-b61a-069f0b2cb9fb",
  path: "test-chrome",
  flags: 0,
};
const CANREMOTE = {
  id: "2480d3e1-9ce4-4b84-8ae3-910b9a95cbb3",
  path: "test-allowremote",
  flags: Ci.nsIAboutModule.URI_CAN_LOAD_IN_CHILD,
};
const MUSTREMOTE = {
  id: "f849cee5-e13e-44d2-981d-0fb3884aaead",
  path: "test-mustremote",
  flags: Ci.nsIAboutModule.URI_MUST_LOAD_IN_CHILD,
};
const CANPRIVILEGEDREMOTE = {
  id: "a04ffafe-6c63-4266-acae-0f4b093165aa",
  path: "test-canprivilegedremote",
  flags: Ci.nsIAboutModule.URI_MUST_LOAD_IN_CHILD |
         Ci.nsIAboutModule.URI_CAN_LOAD_IN_PRIVILEGED_CHILD,
};
const MUSTEXTENSION = {
  id: "f7a1798f-965b-49e9-be83-ec6ee4d7d675",
  path: "test-mustextension",
  flags: Ci.nsIAboutModule.URI_MUST_LOAD_IN_EXTENSION_PROCESS,
};


const TEST_MODULES = [
  CHROME,
  CANREMOTE,
  MUSTREMOTE,
  CANPRIVILEGEDREMOTE,
  MUSTEXTENSION,
];

function AboutModule() {
}

AboutModule.prototype = {
  newChannel(aURI, aLoadInfo) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  getURIFlags(aURI) {
    for (let module of TEST_MODULES) {
      if (aURI.pathQueryRef.startsWith(module.path)) {
        return module.flags;
      }
    }

    ok(false, "Called getURIFlags for an unknown page " + aURI.spec);
    return 0;
  },

  getIndexedDBOriginPostfix(aURI) {
    return null;
  },

  QueryInterface: ChromeUtils.generateQI([Ci.nsIAboutModule]),
};

var AboutModuleFactory = {
  createInstance(aOuter, aIID) {
    if (aOuter)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return new AboutModule().QueryInterface(aIID);
  },

  lockFactory(aLock) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  QueryInterface: ChromeUtils.generateQI([Ci.nsIFactory]),
};

add_task(async function init() {
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

function test_url(url, chromeResult, webContentResult, privilegedContentResult, extensionProcessResult) {
  is(E10SUtils.canLoadURIInRemoteType(url, /* fission */ false, CHROME_PROCESS),
     chromeResult, "Check URL in chrome process.");
  is(E10SUtils.canLoadURIInRemoteType(url, /* fission */ false, WEB_CONTENT_PROCESS),
     webContentResult, "Check URL in web content process.");
  is(E10SUtils.canLoadURIInRemoteType(url, /* fission */ false, PRIVILEGED_CONTENT_PROCESS),
     privilegedContentResult, "Check URL in privileged content process.");
  is(E10SUtils.canLoadURIInRemoteType(url, /* fission */ false, EXTENSION_PROCESS),
     extensionProcessResult, "Check URL in extension process.");

  is(E10SUtils.canLoadURIInRemoteType(url + "#foo", /* fission */ false, CHROME_PROCESS),
     chromeResult, "Check URL with ref in chrome process.");
  is(E10SUtils.canLoadURIInRemoteType(url + "#foo", /* fission */ false, WEB_CONTENT_PROCESS),
     webContentResult, "Check URL with ref in web content process.");
  is(E10SUtils.canLoadURIInRemoteType(url + "#foo", /* fission */ false, PRIVILEGED_CONTENT_PROCESS),
     privilegedContentResult, "Check URL with ref in privileged content process.");
  is(E10SUtils.canLoadURIInRemoteType(url + "#foo", /* fission */ false, EXTENSION_PROCESS),
     extensionProcessResult, "Check URL with ref in extension process.");

  is(E10SUtils.canLoadURIInRemoteType(url + "?foo", /* fission */ false, CHROME_PROCESS),
     chromeResult, "Check URL with query in chrome process.");
  is(E10SUtils.canLoadURIInRemoteType(url + "?foo", /* fission */ false, WEB_CONTENT_PROCESS),
     webContentResult, "Check URL with query in web content process.");
  is(E10SUtils.canLoadURIInRemoteType(url + "?foo", /* fission */ false, PRIVILEGED_CONTENT_PROCESS),
     privilegedContentResult, "Check URL with query in privileged content process.");
  is(E10SUtils.canLoadURIInRemoteType(url + "?foo", /* fission */ false, EXTENSION_PROCESS),
     extensionProcessResult, "Check URL with query in extension process.");

  is(E10SUtils.canLoadURIInRemoteType(url + "?foo#bar", /* fission */ false, CHROME_PROCESS),
     chromeResult, "Check URL with query and ref in chrome process.");
  is(E10SUtils.canLoadURIInRemoteType(url + "?foo#bar", /* fission */ false, WEB_CONTENT_PROCESS),
     webContentResult, "Check URL with query and ref in web content process.");
  is(E10SUtils.canLoadURIInRemoteType(url + "?foo#bar", /* fission */ false, PRIVILEGED_CONTENT_PROCESS),
     privilegedContentResult, "Check URL with query and ref in privileged content process.");
  is(E10SUtils.canLoadURIInRemoteType(url + "?foo#bar", /* fission */ false, EXTENSION_PROCESS),
     extensionProcessResult, "Check URL with query and ref in extension process.");
}

add_task(async function test_chrome() {
  test_url("about:" + CHROME.path, true, false, false, false);
});

add_task(async function test_any() {
  test_url("about:" + CANREMOTE.path, true, true, false, false);
});

add_task(async function test_remote() {
  test_url("about:" + MUSTREMOTE.path, false, true, false, false);
});

add_task(async function test_privileged_remote_true() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.remote.separatePrivilegedContentProcess", true],
    ],
  });

  // This shouldn't be taken literally. We will always use the privileged
  // content type if the URI_CAN_LOAD_IN_PRIVILEGED_CHILD flag is enabled and
  // the pref is turned on.
  test_url("about:" + CANPRIVILEGEDREMOTE.path, false, false, true, false);
});

add_task(async function test_privileged_remote_false() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.remote.separatePrivilegedContentProcess", false],
    ],
  });

  // This shouldn't be taken literally. We will always use the privileged
  // content type if the URI_CAN_LOAD_IN_PRIVILEGED_CHILD flag is enabled and
  // the pref is turned on.
  test_url("about:" + CANPRIVILEGEDREMOTE.path, false, true, false, false);
});

add_task(async function test_extension() {
  test_url("about:" + MUSTEXTENSION.path, false, false, false, true);
});
