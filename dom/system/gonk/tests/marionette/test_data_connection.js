/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

function setEmulatorAPN() {
  let apn = [
    [{"carrier":"T-Mobile US",
      "apn":"epc.tmobile.com",
      "mmsc":"http://mms.msg.eng.t-mobile.com/mms/wapenc",
      "types":["default","supl","mms","ims","dun"]}]
  ];

  return setSettings(SETTINGS_KEY_DATA_APN_SETTINGS, apn);
}

// Test initial State
function testInitialState() {
  log("= testInitialState =");

  // Data should be off before starting any test.
  return getSettings(SETTINGS_KEY_DATA_ENABLED)
    .then(value => {
      is(value, false, "Data must be off");
    });
}

// Test default data Connection
function testDefaultDataConnection() {
  log("= testDefaultDataConnection =");

  // Enable default data
  return setDataEnabledAndWait(true)
    // Disable default data
    .then(() => setDataEnabledAndWait(false));
}

// Test non default data connection
function testNonDefaultDataConnection() {
  log("= testNonDefaultDataConnection =");

  function doTestNonDefaultDataConnection(type) {
    log("doTestNonDefaultDataConnection: " + type);

    return setupDataCallAndWait(type)
      .then(() => deactivateDataCallAndWait(type));
  }

  let currentApn;
  return getSettings(SETTINGS_KEY_DATA_APN_SETTINGS)
    .then(value => {
      currentApn = value;
    })
    .then(setEmulatorAPN)
    .then(() => doTestNonDefaultDataConnection("mms"))
    .then(() => doTestNonDefaultDataConnection("supl"))
    .then(() => doTestNonDefaultDataConnection("ims"))
    .then(() => doTestNonDefaultDataConnection("dun"))
    // Restore APN settings
    .then(() => setSettings(SETTINGS_KEY_DATA_APN_SETTINGS, currentApn));
}

// Start test
startTestBase(function() {
  return testInitialState()
    .then(() => testDefaultDataConnection())
    .then(() => testNonDefaultDataConnection());
});
