/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */


const ETHERNET_MANAGER_CONTRACT_ID = "@mozilla.org/ethernetManager;1";

const INTERFACE_UP = "UP";
const INTERFACE_DOWN = "DOWN";

let gTestSuite = (function() {
  let suite = {};

  // Private member variables of the returned object |suite|.
  let ethernetManager = SpecialPowers.Cc[ETHERNET_MANAGER_CONTRACT_ID]
                                     .getService(SpecialPowers.Ci.nsIEthernetManager);
  let pendingEmulatorShellCount = 0;

  /**
   * Send emulator shell command with safe guard.
   *
   * We should only call |finish()| after all emulator command transactions
   * end, so here comes with the pending counter.  Resolve when the emulator
   * gives positive response, and reject otherwise.
   *
   * Fulfill params: an array of emulator response lines.
   * Reject params: an array of emulator response lines.
   *
   * @param command
   *        A string command to be passed to emulator through its telnet console.
   *
   * @return A deferred promise.
   */
  function runEmulatorShellSafe(command) {
    return new Promise((resolve, reject) => {

      ++pendingEmulatorShellCount;
      runEmulatorShell(command, function(aResult) {
        --pendingEmulatorShellCount;

        ok(true, "Emulator shell response: " + JSON.stringify(aResult));
        if (Array.isArray(aResult)) {
          resolve(aResult);
        } else {
          reject(aResult);
        }
      });

    });
  }

  /**
   * Get the system network conifg by the given interface name.
   *
   * Use shell command 'netcfg' to get the list of network cofig.
   *
   * Fulfill params: An object of { name, flag, ip }
   *
   * @parm ifname
   *       Interface name.
   *
   * @return A deferred promise.
   */
  function getNetworkConfig(ifname) {
    return runEmulatorShellSafe(['netcfg'])
      .then(result => {
        // Sample 'netcfg' output:
        //
        // lo       UP                                   127.0.0.1/8   0x00000049 00:00:00:00:00:00
        // eth0     UP                                   10.0.2.15/24  0x00001043 52:54:00:12:34:56
        // eth1     DOWN                                   0.0.0.0/0   0x00001002 52:54:00:12:34:57
        // rmnet1   DOWN                                   0.0.0.0/0   0x00001002 52:54:00:12:34:59

        let config;

        for (let i = 0; i < result.length; i++) {
          let tokens = result[i].split(/\s+/);
          let name = tokens[0];
          let flag = tokens[1];
          let ip = tokens[2].split(/\/+/)[0];
          if (name == ifname) {
            config = { name: name, flag: flag, ip: ip };
            break;
          }
        }

        return config;
      });
  }

  /**
   * Get the ip assigned by dhcp server of a given interface name.
   *
   * Get the ip from android property 'dhcp.[ifname].ipaddress'.
   *
   * Fulfill params: A string of ip address.
   *
   * @parm ifname
   *       Interface name.
   *
   * @return A deferred promise.
   */
  function getDhcpIpAddr(ifname) {
    return runEmulatorShellSafe(['getprop', 'dhcp.' + ifname + '.ipaddress'])
      .then(function(ipAddr) {
        return ipAddr[0];
      });
  }

  /**
   * Get the gateway assigned by dhcp server of a given interface name.
   *
   * Get the ip from android property 'dhcp.[ifname].gateway'.
   *
   * Fulfill params: A string of gateway.
   *
   * @parm ifname
   *       Interface name.
   *
   * @return A deferred promise.
   */
  function getDhcpGateway(ifname) {
    return runEmulatorShellSafe(['getprop', 'dhcp.' + ifname + '.gateway'])
      .then(function(gateway) {
        return gateway[0];
      });
  }

  /**
   * Get the default route.
   *
   * Use shell command 'ip route' to get the default of device.
   *
   * Fulfill params: An array of { name, gateway }
   *
   * @return A deferred promise.
   */
  function getDefaultRoute() {
    return runEmulatorShellSafe(['ip', 'route'])
      .then(result => {
        // Sample 'ip route' output:
        //
        // 10.0.2.0/24 dev eth0  proto kernel  scope link  src 10.0.2.15
        // default via 10.0.2.2 dev eth0  metric 2

        let routeInfo = [];

        for (let i = 0; i < result.length; i++) {
          if (!result[i].match('default')) {
            continue;
          }

          let tokens = result[i].split(/\s+/);
          let name = tokens[4];
          let gateway = tokens[2];
          routeInfo.push({ name: name, gateway: gateway });
        }

        return routeInfo;
      });
  }

  /**
   * Check a specific interface is enabled or not.
   *
   * @parm ifname
   *       Interface name.
   * @parm enabled
   *       A boolean value used to check interface is disable or not.
   *
   * @return A deferred promise.
   */
  function checkInterfaceIsEnabled(ifname, enabled) {
    return getNetworkConfig(ifname)
      .then(function(config) {
        if (enabled) {
          is(config.flag, INTERFACE_UP, "Interface is enabled as expected.");
        } else {
          is(config.flag, INTERFACE_DOWN, "Interface is disabled as expected.");
        }
      });
  }

  /**
   * Check the ip of a specific interface is equal to given ip or not.
   *
   * @parm ifname
   *       Interface name.
   * @parm ip
   *       Given ip address.
   *
   * @return A deferred promise.
   */
  function checkInterfaceIpAddr(ifname, ip) {
    return getNetworkConfig(ifname)
      .then(function(config) {
        is(config.ip, ip, "IP is right as expected.");
      });
  }

  /**
   * Check the default gateway of a specific interface is equal to given gateway
   * or not.
   *
   * @parm ifname
   *       Interface name.
   * @parm gateway
   *       Given gateway.
   *
   * @return A deferred promise.
   */
  function checkDefaultRoute(ifname, gateway) {
    return getDefaultRoute()
      .then(function(routeInfo) {
        for (let i = 0; i < routeInfo.length; i++) {
          if (routeInfo[i].name == ifname) {
            is(routeInfo[i].gateway, gateway,
               "Default gateway is right as expected.");
            return true;
          }
        }

        if (!gateway) {
          ok(true, "Default route is cleared.");
          return true;
        }

        // TODO: should we ok(false, ......) here?
        return false;
      });
  }

  /**
   * Check the length of interface list in EthernetManager is equal to given
   * length or not.
   *
   * @parm length
   *       Given length.
   */
  function checkInterfaceListLength(length) {
    let list = ethernetManager.interfaceList;
    is(length, list.length, "List length is equal as expected.");
  }

  /**
   * Check the given interface exists on device or not.
   *
   * @parm ifname
   *       Interface name.
   *
   * @return A deferred promise.
   */
  function checkInterfaceExist(ifname) {
    return scanInterfaces()
      .then(list => {
        let index = list.indexOf(ifname);
        if (index < 0) {
          throw "Interface " + ifname + " not found.";
        }

        ok(true, ifname + " exists.");
      });
  }

  /**
   * Scan for available ethernet interfaces.
   *
   * Fulfill params: A list of available interfaces found in device.
   *
   * @return A deferred promise.
   */
  function scanInterfaces() {
    return new Promise(resolve => {

      ethernetManager.scan(function onScan(list) {
        resolve(list);
      });

    });
  }

  /**
   * Add an interface into interface list.
   *
   * Fulfill params: A boolean value indicates success or not.
   *
   * @param ifname
   *        Interface name.
   *
   * @return A deferred promise.
   */
  function addInterface(ifname) {
    return new Promise(resolve => {

      ethernetManager.addInterface(ifname, function onAdd(success, message) {
        ok(success, "Add interface " + ifname + " succeeded.");
        is(message, "ok", "Message is as expected.");

        resolve(success);
      });

    });
  }

  /**
   * Remove an interface form the interface list.
   *
   * Fulfill params: A boolean value indicates success or not.
   *
   * @param ifname
   *        Interface name.
   *
   * @return A deferred promise.
   */
  function removeInterface(ifname) {
    return new Promise(resolve => {

      ethernetManager.removeInterface(ifname, function onRemove(success, message) {
        ok(success, "Remove interface " + ifname + " succeeded.");
        is(message, "ok", "Message is as expected.");

        resolve(success);
      });

    });
  }

  /**
   * Enable networking of an interface in the interface list.
   *
   * Fulfill params: A boolean value indicates success or not.
   *
   * @param ifname
   *        Interface name.
   *
   * @return A deferred promise.
   */
  function enableInterface(ifname) {
    return new Promise(resolve => {

      ethernetManager.enable(ifname, function onEnable(success, message) {
        ok(success, "Enable interface " + ifname + " succeeded.");
        is(message, "ok", "Message is as expected.");

        resolve(success);
      });

    });
  }

  /**
   * Disable networking of an interface in the interface list.
   *
   * Fulfill params: A boolean value indicates success or not.
   *
   * @param ifname
   *        Interface name.
   *
   * @return A deferred promise.
   */
  function disableInterface(ifname) {
    return new Promise(resolve => {

      ethernetManager.disable(ifname, function onDisable(success, message) {
        ok(success, "Disable interface " + ifname + " succeeded.");
        is(message, "ok", "Message is as expected.");

        resolve(success);
      });

    });
  }

  /**
   * Make an interface connect to network.
   *
   * Fulfill params: A boolean value indicates success or not.
   *
   * @param ifname
   *        Interface name.
   *
   * @return A deferred promise.
   */
  function makeInterfaceConnect(ifname) {
    return new Promise(resolve => {

      ethernetManager.connect(ifname, function onConnect(success, message) {
        ok(success, "Interface " + ifname + " is connected successfully.");
        is(message, "ok", "Message is as expected.");

        resolve(success);
      });

    });
  }

  /**
   * Make an interface disconnect to network.
   *
   * Fulfill params: A boolean value indicates success or not.
   *
   * @param ifname
   *        Interface name.
   *
   * @return A deferred promise.
   */
  function makeInterfaceDisconnect(ifname) {
    return new Promise(resolve => {

      ethernetManager.disconnect(ifname, function onDisconnect(success, message) {
        ok(success, "Interface " + ifname + " is disconnected successfully.");
        is(message, "ok", "Message is as expected.");

        resolve(success);
      });

    });
  }

  /**
   * Update the config the an interface in the interface list.
   *
   * @param ifname
   *        Interface name.
   * @param config
   *        .ip: ip address.
   *        .prefixLength: mask length.
   *        .gateway: gateway.
   *        .dnses: dnses.
   *        .httpProxyHost: http proxy host.
   *        .httpProxyPort: http porxy port.
   *        .usingDhcp: an boolean value indicates using dhcp or not.
   *
   * @return A deferred promise.
   */
  function updateInterfaceConfig(ifname, config) {
    return new Promise(resolve => {

      ethernetManager.updateInterfaceConfig(ifname, config,
                                            function onUpdated(success, message) {
        ok(success, "Interface " + ifname + " config is updated successfully " +
                    "with " + JSON.stringify(config));
        is(message, "ok", "Message is as expected.");

        resolve(success);
      });

    });
  }

  /**
   * Wait for timeout.
   *
   * @param timeout
   *        Time in ms.
   *
   * @return A deferred promise.
   */
  function waitForTimeout(timeout) {
    return new Promise(resolve => {

      setTimeout(function() {
        ok(true, "waitForTimeout " + timeout);
        resolve();
      }, timeout);

    });
  }

  /**
   * Wait for default route of a specific interface being set and
   * check.
   *
   * @param ifname
   *        Interface name.
   * @param gateway
   *        Target gateway.
   *
   * @return A deferred promise.
   */
  function waitForDefaultRouteSet(ifname, gateway) {
    return gTestSuite.waitForTimeout(500)
      .then(() => gTestSuite.checkDefaultRoute(ifname, gateway))
      .then(success => {
        if (success) {
          ok(true, "Default route is set as expected." + gateway);
          return;
        }

        ok(true, "Default route is not set yet, check again. " + success);
        return waitForDefaultRouteSet(ifname, gateway);
      });
  }

  //---------------------------------------------------
  // Public test suite functions
  //---------------------------------------------------
  suite.scanInterfaces = scanInterfaces;
  suite.addInterface = addInterface;
  suite.removeInterface = removeInterface;
  suite.enableInterface = enableInterface;
  suite.disableInterface = disableInterface;
  suite.makeInterfaceConnect = makeInterfaceConnect;
  suite.makeInterfaceDisconnect = makeInterfaceDisconnect;
  suite.updateInterfaceConfig = updateInterfaceConfig;
  suite.getDhcpIpAddr = getDhcpIpAddr;
  suite.getDhcpGateway = getDhcpGateway;
  suite.checkInterfaceExist = checkInterfaceExist;
  suite.checkInterfaceIsEnabled = checkInterfaceIsEnabled;
  suite.checkInterfaceIpAddr = checkInterfaceIpAddr;
  suite.checkDefaultRoute = checkDefaultRoute;
  suite.checkInterfaceListLength = checkInterfaceListLength;
  suite.waitForTimeout = waitForTimeout;
  suite.waitForDefaultRouteSet = waitForDefaultRouteSet;

  /**
   * End up the test run.
   *
   * Wait until all pending emulator shell commands are done and then |finish|
   * will be called in the end.
   */
  function cleanUp() {
    waitFor(finish, function() {
      return pendingEmulatorShellCount === 0;
    });
  }

  /**
   * Common test routine.
   *
   * Start a test with the given test case chain. The test environment will be
   * settled down before the test. After the test, all the affected things will
   * be restored.
   *
   * @param aTestCaseChain
   *        The test case entry point, which can be a function or a promise.
   *
   * @return A deferred promise.
   */
  suite.doTest = function(aTestCaseChain) {
    return Promise.resolve()
      .then(aTestCaseChain)
      .then(function onresolve() {
        cleanUp();
      }, function onreject(aReason) {
        ok(false, 'Promise rejects during test' + (aReason ? '(' + aReason + ')' : ''));
        cleanUp();
      });
  };

  return suite;
})();
