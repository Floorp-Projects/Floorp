/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const STATIC_IP_CONFIG = {
  enabled: true,
  ipaddr: "192.168.111.222",
  proxy: "",
  maskLength: 24,
  gateway: "192.168.111.1",
  dns1: "8.8.8.8",
  dns2: "8.8.4.4",
};

function testAssociateWithStaticIp(aNetwork, aStaticIpConfig) {
  return gTestSuite.setStaticIpMode(aNetwork, aStaticIpConfig)
    .then(() => gTestSuite.testAssociate(aNetwork))
    // Check ip address and prefix.
    .then(() => gTestSuite.exeAndParseNetcfg())
    .then((aResult) => {
      is(aResult["wlan0"].ip, aStaticIpConfig.ipaddr, "Check ip address");
      is(aResult["wlan0"].prefix, aStaticIpConfig.maskLength, "Check prefix");
    })
    // Check routing.
    .then(() => gTestSuite.exeAndParseIpRoute())
    .then((aResult) => {
      is(aResult["wlan0"].src, aStaticIpConfig.ipaddr, "Check ip address");
      is(aResult["wlan0"].default, true, "Check default route");
      is(aResult["wlan0"].gateway, aStaticIpConfig.gateway, "Check gateway");
    });
}

// Start test.
gTestSuite.doTest(function() {
  return gTestSuite.ensureWifiEnabled(true)
    .then(() => gTestSuite.requestWifiScan())
    .then((aNetworks) => testAssociateWithStaticIp(aNetworks[0],
                                                   STATIC_IP_CONFIG));
});
