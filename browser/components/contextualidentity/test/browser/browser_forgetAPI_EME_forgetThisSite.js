/*
 * Bug 1278037 - A Test case for checking whether forgetting APIs are working for the media key.
 */

const { classes: Cc, Constructor: CC, interfaces: Ci, utils: Cu } = Components;

const TEST_HOST = "example.com";
const TEST_URL = "http://" + TEST_HOST + "/browser/browser/components/contextualidentity/test/browser/";

const USER_CONTEXTS = [
  "default",
  "personal",
];

const TEST_EME_KEY = {
  initDataType: 'keyids',
  initData: '{"kids":["LwVHf8JLtPrv2GUXFW2v_A"], "type":"persistent-license"}',
  kid: "LwVHf8JLtPrv2GUXFW2v_A",
  key: "97b9ddc459c8d5ff23c1f2754c95abe8",
  sessionType: 'persistent-license',
};

//
// Support functions.
//

function* openTabInUserContext(uri, userContextId) {
  // Open the tab in the correct userContextId.
  let tab = gBrowser.addTab(uri, {userContextId});

  // Select tab and make sure its browser is focused.
  gBrowser.selectedTab = tab;
  tab.ownerGlobal.focus();

  let browser = gBrowser.getBrowserForTab(tab);
  yield BrowserTestUtils.browserLoaded(browser);
  return {tab, browser};
}

function HexToBase64(hex) {
  var bin = "";
  for (var i = 0; i < hex.length; i += 2) {
    bin += String.fromCharCode(parseInt(hex.substr(i, 2), 16));
  }
  return window.btoa(bin).replace(/=/g, "").replace(/\+/g, "-").replace(/\//g, "_");
}

function Base64ToHex(str) {
  var bin = window.atob(str.replace(/-/g, "+").replace(/_/g, "/"));
  var res = "";
  for (var i = 0; i < bin.length; i++) {
    res += ("0" + bin.charCodeAt(i).toString(16)).substr(-2);
  }
  return res;
}

function ByteArrayToHex(array) {
  let bin = String.fromCharCode.apply(null, new Uint8Array(array));
  let res = "";

  for (let i = 0; i < bin.length; i++) {
    res += ("0" + bin.charCodeAt(i).toString(16)).substr(-2);
  }

  return res;
}

function generateKeyObject(aKid, aKey) {
  let keyObj = {
    kty: 'oct',
    kid: aKid,
    k: HexToBase64(aKey),
  };

  return new TextEncoder().encode(JSON.stringify({
    keys: [keyObj]
  }));
}

function generateKeyInfo(aData) {
  let keyInfo = {
    initDataType: aData.initDataType,
    initData: new TextEncoder().encode(aData.initData),
    sessionType: aData.sessionType,
    keyObj: generateKeyObject(aData.kid, aData.key),
  };

  return keyInfo;
}

// Setup a EME key for the given browser, and return the sessionId.
function* setupEMEKey(browser) {
  // Generate the key info.
  let keyInfo = generateKeyInfo(TEST_EME_KEY);

  // Setup the EME key.
  let result = yield ContentTask.spawn(browser, keyInfo, function* (aKeyInfo) {
    let access = yield content.navigator.requestMediaKeySystemAccess('org.w3.clearkey',
                                                                     [{
                                                                       initDataTypes: [aKeyInfo.initDataType],
                                                                       videoCapabilities: [{contentType: 'video/webm'}],
                                                                       sessionTypes: ['persistent-license'],
                                                                       persistentState: 'required',
                                                                     }]);
    let mediaKeys = yield access.createMediaKeys();
    let session = mediaKeys.createSession(aKeyInfo.sessionType);
    let res = {};

    // Insert the EME key.
    let result = yield new Promise(resolve => {
      session.addEventListener("message", function(event) {
        session.update(aKeyInfo.keyObj).then(
          () => { resolve(); }
        ).catch(
          () => {
            ok(false, "Update the EME key fail.");
            resolve();
          }
        );
      });

      session.generateRequest(aKeyInfo.initDataType, aKeyInfo.initData);
    });

    let map = session.keyStatuses;

    is(map.size, 1, "One EME key has been added.");

    if (map.size === 1) {
      res.keyId = map.keys().next().value;
      res.sessionId = session.sessionId;
    }

    // Close the session.
    session.close();
    yield session.closed;

    return res;
  });

  // Check the EME key ID.
  is(ByteArrayToHex(result.keyId), Base64ToHex(TEST_EME_KEY.kid), "The key Id is correct.");
  return result.sessionId;
}

// Check whether the EME key has been cleared.
function* checkEMEKey(browser, emeSessionId) {
  // Generate the key info.
  let keyInfo = generateKeyInfo(TEST_EME_KEY);
  keyInfo.sessionId = emeSessionId;

  yield ContentTask.spawn(browser, keyInfo, function* (aKeyInfo) {
    let access = yield content.navigator.requestMediaKeySystemAccess('org.w3.clearkey',
                                                                     [{
                                                                       initDataTypes: [aKeyInfo.initDataType],
                                                                       videoCapabilities: [{contentType: 'video/webm'}],
                                                                       sessionTypes: ['persistent-license'],
                                                                       persistentState: 'required',
                                                                     }]);
    let mediaKeys = yield access.createMediaKeys();
    let session = mediaKeys.createSession(aKeyInfo.sessionType);

    // First, load the session with the sessionId.
    yield session.load(aKeyInfo.sessionId);

    let map = session.keyStatuses;

    // Check that there is no media key here.
    is(map.size, 0, "No media key should be here after forgetThisSite() was called.");
  });
}

//
// Test functions.
//

add_task(function* setup() {
  // Make sure userContext is enabled.
  yield SpecialPowers.pushPrefEnv({"set": [
      [ "privacy.userContext.enabled", true ],
      [ "media.mediasource.enabled", true ],
      [ "media.eme.apiVisible", true ],
      [ "media.mediasource.webm.enabled", true ],
  ]});
});

add_task(function* test_EME_forgetThisSite() {
  let tabs = [];
  let emeSessionIds = [];

  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    // Open our tab in the given user context.
    tabs[userContextId] = yield* openTabInUserContext(TEST_URL+ "empty_file.html", userContextId);

    // Setup EME Key.
    emeSessionIds[userContextId] = yield setupEMEKey(tabs[userContextId].browser);

    // Close this tab.
    yield BrowserTestUtils.removeTab(tabs[userContextId].tab);
  }

  // Clear all EME data for a given domain with originAttributes pattern.
  let mps = Cc["@mozilla.org/gecko-media-plugin-service;1"].
               getService(Ci.mozIGeckoMediaPluginChromeService);
  mps.forgetThisSite(TEST_HOST, JSON.stringify({}));

  // Open tabs again to check EME keys have been cleared.
  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    // Open our tab in the given user context.
    tabs[userContextId] = yield* openTabInUserContext(TEST_URL+ "empty_file.html", userContextId);

    // Check whether EME Key has been cleared.
    yield checkEMEKey(tabs[userContextId].browser, emeSessionIds[userContextId]);

    // Close this tab.
    yield BrowserTestUtils.removeTab(tabs[userContextId].tab);
  }
});
