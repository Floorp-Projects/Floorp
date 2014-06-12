/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const ETHERNET_INTERFACE_NAME = "eth1";
const INTERFACE_IP_NONE = "0.0.0.0";

function checkIpAddrIsReset(ifname) {
  return gTestSuite.checkInterfaceIpAddr(ifname, INTERFACE_IP_NONE)
    .then(() => gTestSuite.checkDefaultRoute(ifname));
}

gTestSuite.doTest(function() {
  return Promise.resolve()
    .then(() => gTestSuite.checkInterfaceExist(ETHERNET_INTERFACE_NAME))
    .then(() => gTestSuite.addInterface(ETHERNET_INTERFACE_NAME))
    .then(() => gTestSuite.enableInterface(ETHERNET_INTERFACE_NAME))
    .then(() => gTestSuite.makeInterfaceConnect(ETHERNET_INTERFACE_NAME))
    .then(() => gTestSuite.makeInterfaceDisconnect(ETHERNET_INTERFACE_NAME))
    .then(() => checkIpAddrIsReset(ETHERNET_INTERFACE_NAME))
    .then(() => gTestSuite.disableInterface(ETHERNET_INTERFACE_NAME))
    .then(() => gTestSuite.removeInterface(ETHERNET_INTERFACE_NAME));
});