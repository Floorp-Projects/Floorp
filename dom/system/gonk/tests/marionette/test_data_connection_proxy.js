/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const HTTP_PROXY = "10.0.2.200";
const HTTP_PROXY_PORT = "8080";
const PROXY_TYPE_MANUAL = Ci.nsIProtocolProxyService.PROXYCONFIG_MANUAL;
const PROXY_TYPE_PAC = Ci.nsIProtocolProxyService.PROXYCONFIG_PAC;

// Test initial State
function verifyInitialState() {
  log("= verifyInitialState =");

  // Data should be off before starting any test.
  return getSettings(SETTINGS_KEY_DATA_ENABLED)
    .then(value => {
      is(value, false, "Data must be off");
    });
}

function setTestApn() {
  let apn = [
    [ {"carrier": "T-Mobile US",
       "apn": "epc.tmobile.com",
       "proxy": HTTP_PROXY,
       "port": HTTP_PROXY_PORT,
       "mmsc": "http://mms.msg.eng.t-mobile.com/mms/wapenc",
       "types": ["default","supl","mms"]} ]
  ];

  return setSettings(SETTINGS_KEY_DATA_APN_SETTINGS, apn);
}

function waitForHttpProxyVerified(aShouldBeSet) {
  let TIME_OUT_VALUE = 20000;

  return new Promise(function(aResolve, aReject) {
    try {
      waitFor(aResolve, () => {
        let usePAC = SpecialPowers.getBoolPref("network.proxy.pac_generator");
        let proxyType = SpecialPowers.getIntPref("network.proxy.type");
        let httpProxy = SpecialPowers.getCharPref("network.proxy.http");
        let sslProxy = SpecialPowers.getCharPref("network.proxy.ssl");
        let httpProxyPort = SpecialPowers.getIntPref("network.proxy.http_port");
        let sslProxyPort = SpecialPowers.getIntPref("network.proxy.ssl_port");

        if ((aShouldBeSet &&
             (usePAC ? proxyType == PROXY_TYPE_PAC :
                       proxyType == PROXY_TYPE_MANUAL) &&
             httpProxy == HTTP_PROXY &&
             sslProxy == HTTP_PROXY &&
             httpProxyPort == HTTP_PROXY_PORT &&
             sslProxyPort == HTTP_PROXY_PORT) ||
             (!aShouldBeSet &&
              (usePAC ? proxyType == PROXY_TYPE_PAC :
                        proxyType != PROXY_TYPE_MANUAL) &&
              !httpProxy && !sslProxy && !httpProxyPort && !sslProxyPort)) {
          return true;
        }

        return false;
      }, TIME_OUT_VALUE);
    } catch(aError) {
      // Timed out.
      aReject(aError);
    }
  });
}

function testDefaultDataHttpProxy() {
  log("= testDefaultDataHttpProxy =");

  return setDataEnabledAndWait(true)
    .then(() => waitForHttpProxyVerified(true))
    .then(() => setDataEnabledAndWait(false))
    .then(() => waitForHttpProxyVerified(false));
}

function testNonDefaultDataHttpProxy(aType) {
  log("= testNonDefaultDataHttpProxy - " + aType + " =");

  return setupDataCallAndWait(aType)
    // Http proxy should not be set for non-default data connections.
    .then(() => waitForHttpProxyVerified(false))
    .then(() => deactivateDataCallAndWait(aType));
}

// Start test
startTestBase(function() {
  let origApnSettings;
  return verifyInitialState()
    .then(() => getSettings(SETTINGS_KEY_DATA_APN_SETTINGS))
    .then(value => {
      origApnSettings = value;
    })
    .then(() => setTestApn())
    .then(() => testDefaultDataHttpProxy())
    .then(() => testNonDefaultDataHttpProxy(NETWORK_TYPE_MOBILE_MMS))
    .then(() => testNonDefaultDataHttpProxy(NETWORK_TYPE_MOBILE_SUPL))
    // Restore APN settings
    .then(() => setSettings(SETTINGS_KEY_DATA_APN_SETTINGS, origApnSettings));
});
