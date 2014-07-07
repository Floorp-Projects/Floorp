/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const ETHERNET_INTERFACE_NAME = "eth1";

let staticConfig = {
  ip: "1.2.3.4",
  gateway: "1.2.3.5",
  prefixLength: 24,
  dnses: ["1.2.3.6"],
  ipMode: "static"
};

function checkStaticResult(ifname) {
  return gTestSuite.checkInterfaceIpAddr(ifname, staticConfig.ip)
    .then(() => gTestSuite.checkDefaultRoute(ifname, staticConfig.gateway));
}

gTestSuite.doTest(function() {
  return Promise.resolve()
    .then(() => gTestSuite.checkInterfaceExist(ETHERNET_INTERFACE_NAME))
    .then(() => gTestSuite.addInterface(ETHERNET_INTERFACE_NAME))
    .then(() => gTestSuite.enableInterface(ETHERNET_INTERFACE_NAME))
    .then(() => gTestSuite.updateInterfaceConfig(ETHERNET_INTERFACE_NAME, staticConfig))
    .then(() => gTestSuite.makeInterfaceConnect(ETHERNET_INTERFACE_NAME))
    .then(() => checkStaticResult(ETHERNET_INTERFACE_NAME))
    .then(() => gTestSuite.makeInterfaceDisconnect(ETHERNET_INTERFACE_NAME))
    .then(() => gTestSuite.makeInterfaceConnect(ETHERNET_INTERFACE_NAME))
    .then(() => checkStaticResult(ETHERNET_INTERFACE_NAME))
    .then(() => gTestSuite.makeInterfaceDisconnect(ETHERNET_INTERFACE_NAME))
    .then(() => gTestSuite.disableInterface(ETHERNET_INTERFACE_NAME))
    .then(() => gTestSuite.removeInterface(ETHERNET_INTERFACE_NAME));
});