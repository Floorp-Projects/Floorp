/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

Cu.import("resource://gre/modules/Promise.jsm");

const DATA_KEY = "ril.data.enabled";
const APN_KEY  = "ril.data.apnSettings";
const TOPIC_CONNECTION_STATE_CHANGED = "network-connection-state-changed";

let ril = Cc["@mozilla.org/ril;1"].getService(Ci.nsIRadioInterfaceLayer);
ok(ril, "ril.constructor is " + ril.constructor);

let radioInterface = ril.getRadioInterface(0);
ok(radioInterface, "radioInterface.constructor is " + radioInterface.constrctor);

function setEmulatorAPN() {
  let apn = [
    [{"carrier":"T-Mobile US",
      "apn":"epc.tmobile.com",
      "mmsc":"http://mms.msg.eng.t-mobile.com/mms/wapenc",
      "types":["default","supl","mms","ims","dun"]}]
  ];

  return setSettings(APN_KEY, apn);
}

function setupDataCallAndWait(type, networkType) {
  let promises = [];
  promises.push(waitForObserverEvent(TOPIC_CONNECTION_STATE_CHANGED));
  promises.push(radioInterface.setupDataCallByType(type));

  return Promise.all(promises).then(function(results) {
    let subject = results[0];
    ok(subject instanceof Ci.nsIRilNetworkInterface,
       "subject should be an instance of nsIRILNetworkInterface");
    is(subject.type, networkType,
       "subject.type should be " + networkType);
    is(subject.state, Ci.nsINetworkInterface.NETWORK_STATE_CONNECTED,
       "subject.state should be CONNECTED");
  });
}

function deactivateDataCallAndWait(type, networkType) {
  let promises = [];
  promises.push(waitForObserverEvent(TOPIC_CONNECTION_STATE_CHANGED));
  promises.push(radioInterface.deactivateDataCallByType(type));

  return Promise.all(promises).then(function(results) {
    let subject = results[0];
    ok(subject instanceof Ci.nsIRilNetworkInterface,
       "subject should be an instance of nsIRILNetworkInterface");
    is(subject.type, networkType,
       "subject.type should be " + networkType);
    is(subject.state, Ci.nsINetworkInterface.NETWORK_STATE_DISCONNECTED,
       "subject.state should be DISCONNECTED");
  });
}

function setDataEnabledAndWait(enabled) {
  let promises = [];
  promises.push(waitForObserverEvent(TOPIC_CONNECTION_STATE_CHANGED));
  promises.push(setSettings(DATA_KEY, enabled));

  return Promise.all(promises).then(function(results) {
    let subject = results[0];
    ok(subject instanceof Ci.nsIRilNetworkInterface,
       "subject should be an instance of nsIRILNetworkInterface");
    is(subject.type, Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE,
       "subject.type should be " + Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE);
    is(subject.state,
       enabled ? Ci.nsINetworkInterface.NETWORK_STATE_CONNECTED
               : Ci.nsINetworkInterface.NETWORK_STATE_DISCONNECTED,
       "subject.state should be " + enabled ? "CONNECTED" : "DISCONNECTED");
  });
}

// Test initial State
function testInitialState() {
  log("= testInitialState =");

  // Data should be off before starting any test.
  return getSettings(DATA_KEY)
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

    let typeMapping = {
      "mms": Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS,
      "supl": Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_SUPL,
      "ims": Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_IMS,
      "dun": Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_DUN
    };
    let networkType = typeMapping[type];

    return setupDataCallAndWait(type, networkType)
      .then(() => deactivateDataCallAndWait(type, networkType));
  }

  let currentApn;
  return getSettings(APN_KEY)
    .then(value => {
      currentApn = value;
    })
    .then(setEmulatorAPN)
    .then(() => doTestNonDefaultDataConnection("mms"))
    .then(() => doTestNonDefaultDataConnection("supl"))
    .then(() => doTestNonDefaultDataConnection("ims"))
    .then(() => doTestNonDefaultDataConnection("dun"))
    // Restore APN settings
    .then(() => setSettings(APN_KEY, currentApn));
}

// Start test
startTestBase(function() {
  return testInitialState()
    .then(() => testDefaultDataConnection())
    .then(() => testNonDefaultDataConnection());
});
