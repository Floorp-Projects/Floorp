/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const ETHERNET_INTERFACE_NAME = "eth1";

function checkDhcpResult(ifname) {
  return gTestSuite.getDhcpIpAddr(ifname)
    .then(ip => gTestSuite.checkInterfaceIpAddr(ifname, ip))
    .then(() => gTestSuite.getDhcpGateway(ifname))
    .then(gateway => gTestSuite.waitForDefaultRouteSet(ifname, gateway));
}

gTestSuite.doTest(function() {
  return Promise.resolve()
    .then(() => gTestSuite.checkInterfaceExist(ETHERNET_INTERFACE_NAME))
    .then(() => gTestSuite.addInterface(ETHERNET_INTERFACE_NAME))
    .then(() => gTestSuite.enableInterface(ETHERNET_INTERFACE_NAME))
    .then(() => gTestSuite.makeInterfaceConnect(ETHERNET_INTERFACE_NAME))
    .then(() => checkDhcpResult(ETHERNET_INTERFACE_NAME))
    .then(() => gTestSuite.makeInterfaceDisconnect(ETHERNET_INTERFACE_NAME))
    .then(() => gTestSuite.makeInterfaceConnect(ETHERNET_INTERFACE_NAME))
    .then(() => checkDhcpResult(ETHERNET_INTERFACE_NAME))
    .then(() => gTestSuite.makeInterfaceDisconnect(ETHERNET_INTERFACE_NAME))
    .then(() => gTestSuite.disableInterface(ETHERNET_INTERFACE_NAME))
    .then(() => gTestSuite.removeInterface(ETHERNET_INTERFACE_NAME));
});
