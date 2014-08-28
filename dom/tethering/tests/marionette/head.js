/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const TYPE_WIFI = "wifi";
const TYPE_BLUETOOTH = "bt";
const TYPE_USB = "usb";

/**
 * General tethering setting.
 */
const TETHERING_SETTING_IP = "192.168.1.1";
const TETHERING_SETTNG_PREFIX = "24";
const TETHERING_SETTING_START_IP = "192.168.1.10";
const TETHERING_SETTING_END_IP = "192.168.1.30";
const TETHERING_SETTING_DNS1 = "8.8.8.8";
const TETHERING_SETTING_DNS2 = "8.8.4.4";

/**
 * Wifi tethering setting.
 */
const TETHERING_SETTING_SSID = "FirefoxHotSpot";
const TETHERING_SETTING_SECURITY = "open";
const TETHERING_SETTING_KEY = "1234567890";

const SETTINGS_RIL_DATA_ENABLED = 'ril.data.enabled';

let Promise =
  SpecialPowers.Cu.import("resource://gre/modules/Promise.jsm").Promise;

let gTestSuite = (function() {
  let suite = {};

  let tetheringManager;
  let pendingEmulatorShellCount = 0;

  /**
   * A wrapper function of "is".
   *
   * Calls the marionette function "is" as well as throws an exception
   * if the givens values are not equal.
   *
   * @param value1
   *        Any type of value to compare.
   *
   * @param value2
   *        Any type of value to compare.
   *
   * @param message
   *        Debug message for this check.
   *
   */
  function isOrThrow(value1, value2, message) {
    is(value1, value2, message);
    if (value1 !== value2) {
      throw message;
    }
  }

  /**
   * Send emulator shell command with safe guard.
   *
   * We should only call |finish()| after all emulator command transactions
   * end, so here comes with the pending counter.  Resolve when the emulator
   * gives positive response, and reject otherwise.
   *
   * Fulfill params:
   *   result -- an array of emulator response lines.
   * Reject params:
   *   result -- an array of emulator response lines.
   *
   * @param aCommand
   *        A string command to be passed to emulator through its telnet console.
   *
   * @return A deferred promise.
   */
  function runEmulatorShellSafe(aCommand) {
    let deferred = Promise.defer();

    ++pendingEmulatorShellCount;
    runEmulatorShell(aCommand, function(aResult) {
      --pendingEmulatorShellCount;

      ok(true, "Emulator shell response: " + JSON.stringify(aResult));
      if (Array.isArray(aResult)) {
        deferred.resolve(aResult);
      } else {
        deferred.reject(aResult);
      }
    });

    return deferred.promise;
  }

  /**
   * Wait for timeout.
   *
   * Resolve when the given duration elapsed. Never reject.
   *
   * Fulfill params: (none)
   *
   * @param aTimeoutMs
   *        The duration after which the timeout event should occurs.
   *
   * @return A deferred promise.
   */
  function waitForTimeout(aTimeoutMs) {
    let deferred = Promise.defer();

    setTimeout(function() {
      deferred.resolve();
    }, aTimeoutMs);

    return deferred.promise;
  }

  /**
   * Get mozSettings value specified by @aKey.
   *
   * Resolve if that mozSettings value is retrieved successfully, reject
   * otherwise.
   *
   * Fulfill params:
   *   The corresponding mozSettings value of the key.
   * Reject params: (none)
   *
   * @param aKey
   *        A string.
   *
   * @return A deferred promise.
   */
  function getSettings(aKey) {
    let request = navigator.mozSettings.createLock().get(aKey);

    return wrapDomRequestAsPromise(request)
      .then(function resolve(aEvent) {
        ok(true, "getSettings(" + aKey + ") - success");
        return aEvent.target.result[aKey];
      }, function reject(aEvent) {
        ok(false, "getSettings(" + aKey + ") - error");
        throw aEvent.target.error;
      });
  }

  /**
   * Set mozSettings values.
   *
   * Resolve if that mozSettings value is set successfully, reject otherwise.
   *
   * Fulfill params: (none)
   * Reject params: (none)
   *
   * @param aSettings
   *        An object of format |{key1: value1, key2: value2, ...}|.
   * @return A deferred promise.
   */
  function setSettings(aSettings) {
    let lock = window.navigator.mozSettings.createLock();
    let request = lock.set(aSettings);
    let deferred = Promise.defer();
    lock.onsettingstransactionsuccess = function () {
        ok(true, "setSettings(" + JSON.stringify(aSettings) + ")");
      deferred.resolve();
    };
    lock.onsettingstransactionfailure = function (aEvent) {
        ok(false, "setSettings(" + JSON.stringify(aSettings) + ")");
      deferred.reject();
        throw aEvent.target.error;
    };
    return deferred.promise;
  }

  /**
   * Set mozSettings value with only one key.
   *
   * Resolve if that mozSettings value is set successfully, reject otherwise.
   *
   * Fulfill params: (none)
   * Reject params: (none)
   *
   * @param aKey
   *        A string key.
   * @param aValue
   *        An object value.
   * @param aAllowError [optional]
   *        A boolean value.  If set to true, an error response won't be treated
   *        as test failure.  Default: false.
   *
   * @return A deferred promise.
   */
  function setSettings1(aKey, aValue, aAllowError) {
    let settings = {};
    settings[aKey] = aValue;
    return setSettings(settings, aAllowError);
  }

  /**
   * Wrap DOMRequest onsuccess/onerror events to Promise resolve/reject.
   *
   * Fulfill params: A DOMEvent.
   * Reject params: A DOMEvent.
   *
   * @param aRequest
   *        A DOMRequest instance.
   *
   * @return A deferred promise.
   */
  function wrapDomRequestAsPromise(aRequest) {
    let deffered = Promise.defer();

    ok(aRequest instanceof DOMRequest,
      "aRequest is instanceof" + aRequest.constructor);

    aRequest.onsuccess = function(aEvent) {
      deffered.resolve(aEvent);
    };
    aRequest.onerror = function(aEvent) {
      deffered.reject(aEvent);
    };

    return deffered.promise;
  }

  /**
   * Wait for one named MozMobileConnection event.
   *
   * Resolve if that named event occurs.  Never reject.
   *
   * Fulfill params: the DOMEvent passed.
   *
   * @param aEventName
   *        A string event name.
   *
   * @return A deferred promise.
   */
  function waitForMobileConnectionEventOnce(aEventName, aServiceId) {
    aServiceId = aServiceId || 0;

    let deferred = Promise.defer();
    let mobileconnection = navigator.mozMobileConnections[aServiceId];

    mobileconnection.addEventListener(aEventName, function onevent(aEvent) {
      mobileconnection.removeEventListener(aEventName, onevent);

      ok(true, "Mobile connection event '" + aEventName + "' got.");
      deferred.resolve(aEvent);
    });

    return deferred.promise;
  }

  /**
   * Wait for RIL data being connected.
   *
   * This function will check |MozMobileConnection.data.connected| on
   * every 'datachange' event. Resolve when |MozMobileConnection.data.connected|
   * becomes the expected state. Never reject.
   *
   * Fulfill params: (none)
   * Reject params: (none)
   *
   * @param aConnected
   *        Boolean that indicates the desired data state.
   *
   * @param aServiceId [optional]
   *        A numeric DSDS service id. Default: 0.
   *
   * @return A deferred promise.
   */
  function waitForRilDataConnected(aConnected, aServiceId) {
    aServiceId = aServiceId || 0;

    return waitForMobileConnectionEventOnce('datachange', aServiceId)
      .then(function () {
        let mobileconnection = navigator.mozMobileConnections[aServiceId];
        if (mobileconnection.data.connected !== aConnected) {
          return waitForRilDataConnected(aConnected, aServiceId);
        }
      });
  }

  /**
   * Verify everything about routing when the wifi tethering is either on or off.
   *
   * We use two unix commands to verify the routing: 'netcfg' and 'ip route'.
   * For now the following two things will be checked:
   *   1) The default route interface should be 'rmnet0'.
   *   2) wlan0 is up and its ip is set to a private subnet.
   *
   * We also verify iptables output as netd's NatController will execute
   *   $ iptables -t nat -A POSTROUTING -o rmnet0 -j MASQUERADE
   *
   * Resolve when the verification is successful and reject otherwise.
   *
   * Fulfill params: (none)
   * Reject params: String that indicates the reason of rejection.
   *
   * @return A deferred promise.
   */
  function verifyTetheringRouting(aEnabled) {
    let netcfgResult = {};
    let ipRouteResult = {};

    // Execute 'netcfg' and parse to |netcfgResult|, each key of which is the
    // interface name and value is { ip(string) }.
    function exeAndParseNetcfg() {
      return runEmulatorShellSafe(['netcfg'])
        .then(function (aLines) {
          // Sample output:
          //
          // lo       UP     127.0.0.1/8   0x00000049 00:00:00:00:00:00
          // eth0     UP     10.0.2.15/24  0x00001043 52:54:00:12:34:56
          // rmnet1   DOWN   0.0.0.0/0   0x00001002 52:54:00:12:34:58
          // rmnet2   DOWN   0.0.0.0/0   0x00001002 52:54:00:12:34:59
          // rmnet3   DOWN   0.0.0.0/0   0x00001002 52:54:00:12:34:5a
          // wlan0    UP     192.168.1.1/24  0x00001043 52:54:00:12:34:5b
          // sit0     DOWN   0.0.0.0/0   0x00000080 00:00:00:00:00:00
          // rmnet0   UP     10.0.2.100/24  0x00001043 52:54:00:12:34:57
          //
          aLines.forEach(function (aLine) {
            let tokens = aLine.split(/\s+/);
            if (tokens.length < 5) {
              return;
            }
            let ifname = tokens[0];
            let ip = (tokens[2].split('/'))[0];
            netcfgResult[ifname] = { ip: ip };
          });
        });
    }

    // Execute 'ip route' and parse to |ipRouteResult|, each key of which is the
    // interface name and value is { src(string), default(boolean) }.
    function exeAndParseIpRoute() {
      return runEmulatorShellSafe(['ip', 'route'])
        .then(function (aLines) {
          // Sample output:
          //
          // 10.0.2.4 via 10.0.2.2 dev rmnet0
          // 10.0.2.3 via 10.0.2.2 dev rmnet0
          // 192.168.1.0/24 dev wlan0  proto kernel  scope link  src 192.168.1.1
          // 10.0.2.0/24 dev eth0  proto kernel  scope link  src 10.0.2.15
          // 10.0.2.0/24 dev rmnet0  proto kernel  scope link  src 10.0.2.100
          // default via 10.0.2.2 dev rmnet0
          // default via 10.0.2.2 dev eth0  metric 2
          //

          // Parse source ip for each interface.
          aLines.forEach(function (aLine) {
            let tokens = aLine.trim().split(/\s+/);
            let srcIndex = tokens.indexOf('src');
            if (srcIndex < 0 || srcIndex + 1 >= tokens.length) {
              return;
            }
            let ifname = tokens[2];
            let src = tokens[srcIndex + 1];
            ipRouteResult[ifname] = { src: src, default: false };
          });

          // Parse default interfaces.
          aLines.forEach(function (aLine) {
            let tokens = aLine.split(/\s+/);
            if (tokens.length < 2) {
              return;
            }
            if ('default' === tokens[0]) {
              let ifnameIndex = tokens.indexOf('dev');
              if (ifnameIndex < 0 || ifnameIndex + 1 >= tokens.length) {
                return;
              }
              let ifname = tokens[ifnameIndex + 1];
              if (ipRouteResult[ifname]) {
                ipRouteResult[ifname].default = true;
              }
              return;
            }
          });

        });

    }

    // Find MASQUERADE in POSTROUTING section. 'MASQUERADE' should be found
    // when tethering is enabled. 'MASQUERADE' shouldn't be found when tethering
    // is disabled.
    function verifyIptables() {
      return runEmulatorShellSafe(['iptables', '-t', 'nat', '-L', 'POSTROUTING'])
        .then(function(aLines) {
          // $ iptables -t nat -L POSTROUTING
          //
          // Sample output (tethering on):
          //
          // Chain POSTROUTING (policy ACCEPT)
          // target     prot opt source               destination
          // MASQUERADE  all  --  anywhere             anywhere
          //
          let found = (function find_MASQUERADE() {
            // Skip first two lines.
            for (let i = 2; i < aLines.length; i++) {
              if (-1 !== aLines[i].indexOf('MASQUERADE')) {
                return true;
              }
            }
            return false;
          })();

          if ((aEnabled && !found) || (!aEnabled && found)) {
            throw 'MASQUERADE' + (found ? '' : ' not') + ' found while tethering is ' +
                  (aEnabled ? 'enabled' : 'disabled');
          }
        });
    }

    function verifyDefaultRouteAndIp(aExpectedWifiTetheringIp) {
      log(JSON.stringify(ipRouteResult));
      log(JSON.stringify(netcfgResult));

      if (aEnabled) {
        isOrThrow(ipRouteResult['rmnet0'].src, netcfgResult['rmnet0'].ip, 'rmnet0.ip');
        isOrThrow(ipRouteResult['rmnet0'].default, true, 'rmnet0.default');

        isOrThrow(ipRouteResult['wlan0'].src, netcfgResult['wlan0'].ip, 'wlan0.ip');
        isOrThrow(ipRouteResult['wlan0'].src, aExpectedWifiTetheringIp, 'expected ip');
        isOrThrow(ipRouteResult['wlan0'].default, false, 'wlan0.default');
      }
    }

    return verifyIptables()
      .then(exeAndParseNetcfg)
      .then(exeAndParseIpRoute)
      .then(() => verifyDefaultRouteAndIp(TETHERING_SETTING_IP));
  }

  /**
   * Request to enable/disable wifi tethering.
   *
   * Enable/disable wifi tethering by using setTetheringEnabled API
   * Resolve when the routing is verified to set up successfully in 20 seconds. The polling
   * period is 1 second.
   *
   * Fulfill params: (none)
   * Reject params: The error message.
   *
   * @param aEnabled
   *        Boolean that indicates to enable or disable wifi tethering.
   *
   * @return A deferred promise.
   */
  function setWifiTetheringEnabled(aEnabled) {
    let RETRY_INTERVAL_MS = 1000;
    let retryCnt = 20;

    let config = {
      "ip"        : TETHERING_SETTING_IP,
      "prefix"    : TETHERING_SETTNG_PREFIX,
      "startIp"   : TETHERING_SETTING_START_IP,
      "endIp"     : TETHERING_SETTING_END_IP,
      "dns1"      : TETHERING_SETTING_DNS1,
      "dns2"      : TETHERING_SETTING_DNS2,
      "wifiConfig": {
        "ssid"      : TETHERING_SETTING_SSID,
        "security"  : TETHERING_SETTING_SECURITY
      }
    };

    return tetheringManager.setTetheringEnabled(aEnabled, TYPE_WIFI, config)
      .then(function waitForRoutingVerified() {
        return verifyTetheringRouting(aEnabled)
          .then(null, function onreject(aReason) {

            log('verifyTetheringRouting rejected due to ' + aReason +
                ' (' + retryCnt + ')');

            if (!retryCnt--) {
              throw aReason;
            }

            return waitForTimeout(RETRY_INTERVAL_MS).then(waitForRoutingVerified);
          });
      });
  }

  /**
   * Ensure wifi is enabled/disabled.
   *
   * Issue a wifi enable/disable request if wifi is not in the desired state;
   * return a resolved promise otherwise.
   *
   * Fulfill params: (none)
   * Reject params: (none)
   *
   * @return a resolved promise or deferred promise.
   */
  function ensureWifiEnabled(aEnabled) {
    let wifiManager = window.navigator.mozWifiManager;
    if (wifiManager.enabled === aEnabled) {
      return Promise.resolve();
    }
    let request = wifiManager.setWifiEnabled(aEnabled);
    return wrapDomRequestAsPromise(request)
  }

  /**
   * Ensure tethering manager exists.
   *
   * Check navigator property |mozTetheringManager| to ensure we could access
   * tethering related function.
   *
   * Fulfill params: (none)
   * Reject params: (none)
   *
   * @return A deferred promise.
   */
  function ensureTetheringManager() {
    let deferred = Promise.defer();

    tetheringManager = window.navigator.mozTetheringManager;

    if (tetheringManager instanceof MozTetheringManager) {
      deferred.resolve();
    } else {
      log("navigator.mozTetheringManager is unavailable");
      deferred.reject();
    }

    return deferred.promise;
  }

  /**
   * Add required permissions for tethering. Never reject.
   *
   * The permissions required for wifi testing are 'wifi-manage' and 'settings-write'.
   * Never reject.
   *
   * Fulfill params: (none)
   *
   * @return A deferred promise.
   */
  function acquirePermission() {
    let deferred = Promise.defer();

    let permissions = [{ 'type': 'wifi-manage', 'allow': 1, 'context': window.document },
                       { 'type': 'settings-write', 'allow': 1, 'context': window.document },
                       { 'type': 'settings-read', 'allow': 1, 'context': window.document },
                       { 'type': 'settings-api-write', 'allow': 1, 'context': window.document },
                       { 'type': 'settings-api-read', 'allow': 1, 'context': window.document },
                       { 'type': 'mobileconnection', 'allow': 1, 'context': window.document }];

    SpecialPowers.pushPermissions(permissions, function() {
      deferred.resolve();
    });

    return deferred.promise;
  }

  /**
   * Common test routine.
   *
   * Start a test with the given test case chain. The test environment will be
   * settled down before the test. After the test, all the affected things will
   * be restored.
   *
   * Fulfill params: (none)
   * Reject params: (none)
   *
   * @param aTestCaseChain
   *        The test case entry point, which can be a function or a promise.
   *
   * @return A deferred promise.
   */
  suite.startTest = function(aTestCaseChain) {
    function setUp() {
      return ensureTetheringManager()
        .then(acquirePermission);
    }

    function tearDown() {
      waitFor(finish, function() {
        return pendingEmulatorShellCount === 0;
      });
    }

    return setUp()
      .then(aTestCaseChain)
      .then(function onresolve() {
        tearDown();
      }, function onreject(aReason) {
        ok(false, 'Promise rejects during test' + (aReason ? '(' + aReason + ')' : ''));
        tearDown();
      });
  };

  //---------------------------------------------------
  // Public test suite functions
  //---------------------------------------------------
  suite.ensureWifiEnabled = ensureWifiEnabled;
  suite.setWifiTetheringEnabled = setWifiTetheringEnabled;

  /**
   * The common test routine for wifi tethering.
   *
   * Set 'ril.data.enabled' to true
   * before testing and restore it afterward. It will also verify 'ril.data.enabled'
   * and 'tethering.wifi.enabled' to be false in the beginning. Note that this routine
   * will NOT change the state of 'tethering.wifi.enabled' so the user should enable
   * than disable on his/her own. This routine will only check if tethering is turned
   * off after testing.
   *
   * Fulfill params: (none)
   * Reject params: (none)
   *
   * @param aTestCaseChain
   *        The test case entry point, which can be a function or a promise.
   *
   * @return A deferred promise.
   */
  suite.startTetheringTest = function(aTestCaseChain) {
    let oriDataEnabled;
    function verifyInitialState() {
      return getSettings(SETTINGS_RIL_DATA_ENABLED)
        .then(enabled => initTetheringTestEnvironment(enabled));
    }

    function initTetheringTestEnvironment(aEnabled) {
      oriDataEnabled = aEnabled;
      if (aEnabled) {
        return Promise.resolve();
      } else {
        return Promise.all([waitForRilDataConnected(true),
                            setSettings1(SETTINGS_RIL_DATA_ENABLED, true)]);
      }
    }

    function restoreToInitialState() {
      return setSettings1(SETTINGS_RIL_DATA_ENABLED, oriDataEnabled);
    }

    return suite.startTest(function() {
      return verifyInitialState()
        .then(aTestCaseChain)
        .then(restoreToInitialState, function onreject(aReason) {
          return restoreToInitialState()
            .then(() => { throw aReason; }); // Re-throw the orignal reject reason.
        });
    });
  };

  return suite;
})();
