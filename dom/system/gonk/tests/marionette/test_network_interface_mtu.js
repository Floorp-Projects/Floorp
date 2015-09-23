/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_MTU1 = "1410";
const TEST_MTU2 = "1440";

function setEmulatorAPN() {
  let apn = [
    [ { "carrier":"T-Mobile US",
        "apn":"epc1.tmobile.com",
        "types":["default"],
        "mtu": TEST_MTU1 },
      { "carrier":"T-Mobile US",
        "apn":"epc2.tmobile.com",
        "mmsc":"http://mms.msg.eng.t-mobile.com/mms/wapenc",
        "types":["supl","mms","ims","dun", "fota"],
        "mtu": TEST_MTU2 } ]
  ];

  return setSettings(SETTINGS_KEY_DATA_APN_SETTINGS, apn);
}

function verifyInitialState() {
  // Data should be off before starting any test.
  return getSettings(SETTINGS_KEY_DATA_ENABLED)
    .then(value => {
      is(value, false, "Data must be off");
    });
}

function verifyMtu(aInterfaceName, aMtu) {
  runEmulatorShellCmdSafe(['ip', 'link', 'show', 'dev', aInterfaceName])
    .then(aLines => {
      // Sample output:
      //
      // 4: rmnet0: <BROADCAST,MULTICAST> mtu 1410 qdisc pfifo_fast state DOWN mode DEFAULT qlen 1000
      // link/ether 52:54:00:12:34:58 brd ff:ff:ff:ff:ff:ff
      //
      let mtu;
      aLines.some(function (aLine) {
        let tokens = aLine.trim().split(/\s+/);
        let mtuIndex = tokens.indexOf('mtu');
        if (mtuIndex < 0 || mtuIndex + 1 >= tokens.length) {
          return false;
        }

        mtu = tokens[mtuIndex + 1];
        return true;
      });

      is(mtu, aMtu, aInterfaceName + "'s mtu.");
    });
}

function testDefaultDataCallMtu() {
  log("= testDefaultDataCallMtu =");

  return setDataEnabledAndWait(true)
    .then(aNetworkInfo => {
      verifyMtu(aNetworkInfo.name, TEST_MTU1);
    })
    .then(() => setDataEnabledAndWait(false));
}

function testNonDefaultDataCallMtu() {
  log("= testNonDefaultDataCallMtu =");

  function doTestNonDefaultDataCallMtu(aType) {
    log("doTestNonDefaultDataCallMtu: " + aType);

    return setupDataCallAndWait(aType)
      .then(aNetworkInfo => {
        verifyMtu(aNetworkInfo.name, TEST_MTU2);
      })
      .then(() => deactivateDataCallAndWait(aType));
  }

  return doTestNonDefaultDataCallMtu(NETWORK_TYPE_MOBILE_MMS)
    .then(() => doTestNonDefaultDataCallMtu(NETWORK_TYPE_MOBILE_SUPL))
    .then(() => doTestNonDefaultDataCallMtu(NETWORK_TYPE_MOBILE_IMS))
    .then(() => doTestNonDefaultDataCallMtu(NETWORK_TYPE_MOBILE_DUN))
    .then(() => doTestNonDefaultDataCallMtu(NETWORK_TYPE_MOBILE_FOTA));
}

// Start test
startTestBase(function() {
  let origApnSettings;
  return verifyInitialState()
    .then(() => getSettings(SETTINGS_KEY_DATA_APN_SETTINGS))
    .then(value => {
      origApnSettings = value;
    })
    .then(() => setEmulatorAPN())
    .then(() => testDefaultDataCallMtu())
    .then(() => testNonDefaultDataCallMtu())
    .then(() => {
      if (origApnSettings) {
        return setSettings(SETTINGS_KEY_DATA_APN_SETTINGS, origApnSettings);
      }
    });
});
