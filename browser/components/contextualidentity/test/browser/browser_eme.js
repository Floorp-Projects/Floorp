/*
 * Bug 1283325 - A test case to test the EME is originAttributes aware or not.
 */
const { classes: Cc, Constructor: CC, interfaces: Ci, utils: Cu } = Components;

const TEST_HOST = "example.com";
const TEST_URL = "http://" + TEST_HOST + "/browser/browser/components/contextualidentity/test/browser/";

const TESTKEY = {
  initDataType: 'keyids',
  initData: '{"kids":["LwVHf8JLtPrv2GUXFW2v_A"], "type":"persistent-license"}',
  kid: "LwVHf8JLtPrv2GUXFW2v_A",
  key: "97b9ddc459c8d5ff23c1f2754c95abe8",
  sessionType: 'persistent-license',
};

const USER_ID_DEFAULT = 0;
const USER_ID_PERSONAL = 1;

function* openTabInUserContext(uri, userContextId) {
  // Open the tab in the correct userContextId.
  let tab = gBrowser.addTab(uri, {userContextId});

  // Select tab and make sure its browser is focused.
  gBrowser.selectedTab = tab;
  tab.ownerDocument.defaultView.focus();

  let browser = gBrowser.getBrowserForTab(tab);
  yield BrowserTestUtils.browserLoaded(browser);
  return {tab, browser};
}

function HexToBase64(hex)
{
  var bin = "";
  for (var i = 0; i < hex.length; i += 2) {
    bin += String.fromCharCode(parseInt(hex.substr(i, 2), 16));
  }
  return window.btoa(bin).replace(/=/g, "").replace(/\+/g, "-").replace(/\//g, "_");
}

function Base64ToHex(str)
{
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

add_task(function* setup() {
  // Make sure userContext is enabled.
  yield new Promise(resolve => {
    SpecialPowers.pushPrefEnv({"set": [
      [ "privacy.userContext.enabled", true ],
      [ "media.mediasource.enabled", true ],
      [ "media.eme.apiVisible", true ],
      [ "media.mediasource.webm.enabled", true ],
      [ "media.clearkey.persistent-license.enabled", true ],
    ]}, resolve);
  });
});

add_task(function* test() {
  // Open a tab with the default container.
  let defaultContainer = yield openTabInUserContext(TEST_URL + "empty_file.html", USER_ID_DEFAULT);

  // Generate the key info for the default container.
  let keyInfo = generateKeyInfo(TESTKEY);

  // Update the media key for the default container.
  let result = yield ContentTask.spawn(defaultContainer.browser, keyInfo, function* (aKeyInfo) {
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

    // Insert the media key.
    let result = yield new Promise(resolve => {
      session.addEventListener("message", function(event) {
        session.update(aKeyInfo.keyObj).then(
          () => { resolve(); }
        ).catch(
          () => {
            ok(false, "Update the media key fail.");
            resolve();
          }
        );
      });

      session.generateRequest(aKeyInfo.initDataType, aKeyInfo.initData);
    });

    let map = session.keyStatuses;

    is(map.size, 1, "One media key has been added.");

    if (map.size === 1) {
      res.keyId = map.keys().next().value;
      res.sessionId = session.sessionId;
    }

    // Close the session.
    session.close();
    yield session.closed;

    return res;
  });

  // Check the media key ID.
  is(ByteArrayToHex(result.keyId), Base64ToHex(TESTKEY.kid), "The key Id of the default container is correct.");

  // Store the sessionId for the further checking.
  keyInfo.sessionId = result.sessionId;

  // Open a tab with personal container.
  let personalContainer = yield openTabInUserContext(TEST_URL + "empty_file.html", USER_ID_PERSONAL);

  yield ContentTask.spawn(personalContainer.browser, keyInfo, function* (aKeyInfo) {
    let access = yield content.navigator.requestMediaKeySystemAccess('org.w3.clearkey',
                                                                     [{
                                                                       initDataTypes: [aKeyInfo.initDataType],
                                                                       videoCapabilities: [{contentType: 'video/webm'}],
                                                                       sessionTypes: ['persistent-license'],
                                                                       persistentState: 'required',
                                                                     }]);
    let mediaKeys = yield access.createMediaKeys();
    let session = mediaKeys.createSession(aKeyInfo.sessionType);

    // First, load the session to check that mediakeys do not share with
    // default container.
    yield session.load(aKeyInfo.sessionId);

    let map = session.keyStatuses;

    // Check that there is no media key here.
    is(map.size, 0, "No media key should be here for the personal container.");
  });

  // Close default container tab.
  yield BrowserTestUtils.removeTab(defaultContainer.tab);

  // Close personal container tab.
  yield BrowserTestUtils.removeTab(personalContainer.tab);
});
