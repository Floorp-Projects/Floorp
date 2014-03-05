/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

/**
 * Test resulting IP address format with given APN settings.
 *
 * This test utility function performs following steps:
 *
 *   1) set "ril.data.apnSettings" to a given settings object,
 *   2) enable data connection and wait for a "datachange" event,
 *   3) check the IP address type of the active network interface,
 *   4) disable data connection.
 *
 * Fulfill params: (none)
 * Reject params: (none)
 *
 * @param aApnSettings
 *        An APN settings value.
 * @param aIsIPv6
 *        A boolean value indicating whether we're expecting an IPv6 address.
 *
 * @return A deferred promise.
 */
function doTest(aApnSettings, aIsIPv6) {
  return setDataApnSettings([])
    .then(() => setDataApnSettings(aApnSettings))
    .then(() => setDataEnabledAndWait(true))
    .then(function() {
      let nm = getNetworkManager();
      let active = nm.active;
      ok(active, "Active network interface");

      log("  Interface: " + active.name);
      log("  Address: " + active.ip);
      if (aIsIPv6) {
        ok(active.ip.indexOf(":") > 0, "IPv6 address");
      } else {
        ok(active.ip.indexOf(":") < 0, "IPv4 address");
      }
    })
    .then(() => setDataEnabledAndWait(false));
}

function doTestHome(aApnSettings, aProtocol) {
  log("Testing \"" + aProtocol + "\"@HOME... ");

  // aApnSettings is a double-array of per PDP context settings.  The first
  // index is a DSDS service ID, and the second one is the index of pre-defined
  // PDP context settings of a specified radio interface.  We use 0 for both as
  // default here.
  aApnSettings[0][0].protocol = aProtocol;
  delete aApnSettings[0][0].roaming_protocol;

  return doTest(aApnSettings, aProtocol === "IPV6");
}

function doTestRoaming(aApnSettings, aRoaminProtocol) {
  log("Testing \"" + aRoaminProtocol + "\"@ROMAING... ");

  // aApnSettings is a double-array of per PDP context settings.  The first
  // index is a DSDS service ID, and the second one is the index of pre-defined
  // PDP context settings of a specified radio interface.  We use 0 for both as
  // default here.
  delete aApnSettings[0][0].protocol;
  aApnSettings[0][0].roaming_protocol = aRoaminProtocol;

  return doTest(aApnSettings, aRoaminProtocol === "IPV6");
}

startTestCommon(function() {
  let origApnSettings;

  return setDataRoamingEnabled(true)
    .then(getDataApnSettings)
    .then(function(aResult) {
      // If already set, then save original APN settings.
      origApnSettings = JSON.parse(JSON.stringify(aResult));
      return aResult;
    }, function() {
      // Return our own default value.
      return [[{ "carrier": "T-Mobile US",
                 "apn": "epc.tmobile.com",
                 "mmsc": "http://mms.msg.eng.t-mobile.com/mms/wapenc",
                 "types": ["default", "supl", "mms"] }]];
    })

    .then(function(aApnSettings) {
      return Promise.resolve()

        .then(() => doTestHome(aApnSettings, "NoSuchProtocol"))
        .then(() => doTestHome(aApnSettings, "IP"))
        .then(() => doTestHome(aApnSettings, "IPV6"))

        .then(() => setEmulatorRoamingAndWait(true))

        .then(() => doTestRoaming(aApnSettings, "NoSuchProtocol"))
        .then(() => doTestRoaming(aApnSettings, "IP"))
        .then(() => doTestRoaming(aApnSettings, "IPV6"))

        .then(() => setEmulatorRoamingAndWait(false));
    })

    .then(() => setDataRoamingEnabled(false))
    .then(function() {
      if (origApnSettings) {
        return setDataApnSettings(origApnSettings);
      }
    });
}, ["settings-read", "settings-write"]);
