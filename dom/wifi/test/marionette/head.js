/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Emulate Promise.jsm semantics.
Promise.defer = function() { return new Deferred(); }
function Deferred()  {
  this.promise = new Promise(function(resolve, reject) {
    this.resolve = resolve;
    this.reject = reject;
  }.bind(this));
  Object.freeze(this);
}

const STOCK_HOSTAPD_NAME = 'goldfish-hostapd';
const HOSTAPD_CONFIG_PATH = '/data/misc/wifi/remote-hostapd/';

const SETTINGS_RIL_DATA_ENABLED = 'ril.data.enabled';
const SETTINGS_TETHERING_WIFI_ENABLED = 'tethering.wifi.enabled';
const SETTINGS_TETHERING_WIFI_IP = 'tethering.wifi.ip';
const SETTINGS_TETHERING_WIFI_SECURITY = 'tethering.wifi.security.type';

const HOSTAPD_COMMON_CONFIG = {
  driver: 'test',
  ctrl_interface: '/data/misc/wifi/remote-hostapd',
  test_socket: 'DIR:/data/misc/wifi/sockets',
  hw_mode: 'b',
  channel: '2',
};

const HOSTAPD_CONFIG_LIST = [
  { ssid: 'ap0' },

  { ssid: 'ap1',
    wpa: 1,
    wpa_pairwise: 'TKIP CCMP',
    wpa_passphrase: '12345678'
  },

  { ssid: 'ap2',
    wpa: 2,
    rsn_pairwise: 'CCMP',
    wpa_passphrase: '12345678',
  },
];

let gTestSuite = (function() {
  let suite = {};

  // Private member variables of the returned object |suite|.
  let wifiManager;
  let wifiOrigEnabled;
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
   * Wait for one named MozWifiManager event.
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
  function waitForWifiManagerEventOnce(aEventName) {
    let deferred = Promise.defer();

    wifiManager.addEventListener(aEventName, function onevent(aEvent) {
      wifiManager.removeEventListener(aEventName, onevent);

      ok(true, "WifiManager event '" + aEventName + "' got.");
      deferred.resolve(aEvent);
    });

    return deferred.promise;
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
   * Get the detail of currently running processes containing the given name.
   *
   * Use shell command 'ps' to get the desired process's detail. Never reject.
   *
   * Fulfill params:
   *   result -- an array of { pname, pid }
   *
   * @param aProcessName
   *        The process to get the detail.
   *
   * @return A deferred promise.
   */
  function getProcessDetail(aProcessName) {
    return runEmulatorShellSafe(['ps'])
      .then(processes => {
        // Sample 'ps' output:
        //
        // USER     PID   PPID  VSIZE  RSS     WCHAN    PC         NAME
        // root      1     0     284    204   c009e6c4 0000deb4 S /init
        // root      2     0     0      0     c0052c64 00000000 S kthreadd
        // root      3     2     0      0     c0044978 00000000 S ksoftirqd/0
        //
        let detail = [];

        processes.shift(); // Skip the first line.
        for (let i = 0; i < processes.length; i++) {
          let tokens = processes[i].split(/\s+/);
          let pname = tokens[tokens.length - 1];
          let pid = tokens[1];
          if (-1 !== pname.indexOf(aProcessName)) {
            detail.push({ pname: pname, pid: pid });
          }
        }

        return detail;
      });
  }

  /**
   * Add required permissions for wifi testing. Never reject.
   *
   * The permissions required for wifi testing are 'wifi-manage' and 'settings-write'.
   * Never reject.
   *
   * Fulfill params: (none)
   *
   * @return A deferred promise.
   */
  function addRequiredPermissions() {
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
    let deferred = Promise.defer();

    ok(aRequest instanceof DOMRequest,
       "aRequest is instanceof " + aRequest.constructor);

    aRequest.addEventListener("success", function(aEvent) {
      deferred.resolve(aEvent);
    });
    aRequest.addEventListener("error", function(aEvent) {
      deferred.reject(aEvent);
    });

    return deferred.promise;
  }

  /**
   * Ensure wifi is enabled/disabled.
   *
   * Issue a wifi enable/disable request if wifi is not in the desired state;
   * return a resolved promise otherwise. Note that you cannot rely on this
   * function to test the correctness of enabling/disabling wifi.
   * (use requestWifiEnabled instead)
   *
   * Fulfill params: (none)
   * Reject params: (none)
   *
   * @return a resolved promise or deferred promise.
   */
  function ensureWifiEnabled(aEnabled, useAPI) {
    if (wifiManager.enabled === aEnabled) {
      log('Already ' + (aEnabled ? 'enabled' : 'disabled'));
      return Promise.resolve();
    }
    return requestWifiEnabled(aEnabled, useAPI);
  }

  /**
   * Issue a request to enable/disable wifi.
   *
   * This function will attempt to enable/disable wifi, by calling API or by
   * writing settings 'wifi.enabled' regardless of the wifi state, based on the
   * value of |userAPI| parameter.
   * Default is using settings.
   *
   * Note there's a limitation of co-existance of both method, per bug 930355,
   * that once enable/disable wifi by API, the settings method won't work until
   * reboot. So the test of wifi enable API should be executed last.
   * TODO: Remove settings method after enable/disable wifi by settings is
   *       removed after bug 1050147.
   *
   * Fulfill params: (none)
   * Reject params: (none)
   *
   * @return A deferred promise.
   */
  function requestWifiEnabled(aEnabled, useAPI) {
    return Promise.all([
      waitForWifiManagerEventOnce(aEnabled ? 'enabled' : 'disabled'),
      useAPI ?
        wrapDomRequestAsPromise(wifiManager.setWifiEnabled(aEnabled)) :
        setSettings({ 'wifi.enabled': aEnabled }),
    ]);
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
   * Request to enable/disable wifi tethering.
   *
   * Enable/disable wifi tethering by changing the settings value 'tethering.wifi.enabled'.
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
  function requestTetheringEnabled(aEnabled) {
    let RETRY_INTERVAL_MS = 1000;
    let retryCnt = 20;

    return setSettings1(SETTINGS_TETHERING_WIFI_ENABLED, aEnabled)
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
   * Forget the given network.
   *
   * Resolve when we successfully forget the given network; reject when any error
   * occurs.
   *
   * Fulfill params: (none)
   * Reject params: (none)
   *
   * @param aNetwork
   *        An object of MozWifiNetwork.
   *
   * @return A deferred promise.
   */
  function forgetNetwork(aNetwork) {
    let request = wifiManager.forget(aNetwork);
    return wrapDomRequestAsPromise(request)
      .then(event => event.target.result);
  }

  /**
   * Forget all known networks.
   *
   * Resolve when we successfully forget all the known network;
   * reject when any error occurs.
   *
   * Fulfill params: (none)
   * Reject params: (none)
   *
   * @return A deferred promise.
   */
  function forgetAllKnownNetworks() {

    function createForgetNetworkChain(aNetworks) {
      let chain = Promise.resolve();

      aNetworks.forEach(function (aNetwork) {
        chain = chain.then(() => forgetNetwork(aNetwork));
      });

      return chain;
    }

    return getKnownNetworks()
      .then(networks => createForgetNetworkChain(networks));
  }

  /**
   * Get all known networks.
   *
   * Resolve when we get all the known networks; reject when any error
   * occurs.
   *
   * Fulfill params: An array of MozWifiNetwork
   * Reject params: (none)
   *
   * @return A deferred promise.
   */
  function getKnownNetworks() {
    let request = wifiManager.getKnownNetworks();
    return wrapDomRequestAsPromise(request)
      .then(event => event.target.result);
  }

  /**
   * Issue a request to scan all wifi available networks.
   *
   * Resolve when we get the scan result; reject when any error
   * occurs.
   *
   * Fulfill params: An array of MozWifiNetwork
   * Reject params: (none)
   *
   * @return A deferred promise.
   */
  function requestWifiScan() {
    let request = wifiManager.getNetworks();
    return wrapDomRequestAsPromise(request)
      .then(event => event.target.result);
  }

  /**
   * Request wifi scan and verify the scan result as well.
   *
   * Issue a wifi scan request and check if the result is expected.
   * Since the old APs may be cached and the newly added APs may be
   * still not scan-able, a couple of attempts are acceptable.
   * Resolve if we eventually get the expected scan result; reject otherwise.
   *
   * Fulfill params: The scan result, which is an array of MozWifiNetwork
   * Reject params: (none)
   *
   * @param aRetryCnt
   *        The maxmimum number of attempts until we get the expected scan result.
   * @param aExpectedNetworks
   *        An array of object, each of which contains at least the |ssid| property.
   *
   * @return A deferred promise.
   */
  function testWifiScanWithRetry(aRetryCnt, aExpectedNetworks) {

    // Check if every single ssid of each |aScanResult| exists in |aExpectedNetworks|
    // as well as the length of |aScanResult| equals to |aExpectedNetworks|.
    function isScanResultExpected(aScanResult) {
      if (aScanResult.length !== aExpectedNetworks.length) {
        return false;
      }

      for (let i = 0; i < aScanResult.length; i++) {
        if (-1 === getFirstIndexBySsid(aScanResult[i].ssid, aExpectedNetworks)) {
          return false;
        }
      }
      return true;
    }

    return requestWifiScan()
      .then(function (networks) {
        if (isScanResultExpected(networks, aExpectedNetworks)) {
          return networks;
        }
        if (aRetryCnt > 0) {
          return testWifiScanWithRetry(aRetryCnt - 1, aExpectedNetworks);
        }
        throw 'Unexpected scan result!';
      });
  }

  /**
   * Test wifi association.
   *
   * Associate with the given network object which is obtained by
   * MozWifiManager.getNetworks() (i.e. MozWifiNetwork).
   * Resolve when the 'connected' status change event is received.
   * Note that we might see other events like 'connecting'
   * before 'connected'. So we need to call |waitForWifiManagerEventOnce|
   * again whenever non 'connected' event is seen. Never reject.
   *
   * Fulfill params: (none)
   *
   * @param aNetwork
   *        An object of MozWifiNetwork.
   *
   * @return A deferred promise.
   */
  function testAssociate(aNetwork) {
    setPasswordIfNeeded(aNetwork);

    let promises = [];

    // Register the event listerner to wait for 'connected' event first
    // to avoid racing issue.
    promises.push(waitForConnected(aNetwork));

    // Then we do the association.
    let request = wifiManager.associate(aNetwork);
    promises.push(wrapDomRequestAsPromise(request));

    return Promise.all(promises);
  }

  function waitForConnected(aExpectedNetwork) {
    return waitForWifiManagerEventOnce('statuschange')
      .then(function onstatuschange(event) {
        log("event.status: " + event.status);
        log("event.network.ssid: " + (event.network ? event.network.ssid : ''));

        if ("connected" === event.status &&
            event.network.ssid === aExpectedNetwork.ssid) {
          return; // Got expected 'connected' event from aNetwork.ssid.
        }

        log('Not expected "connected" statuschange event. Wait again!');
        return waitForConnected(aExpectedNetwork);
      });
  }

  /**
   * Set the password for associating the given network if needed.
   *
   * Set the password by looking up HOSTAPD_CONFIG_LIST. This function
   * will also set |keyManagement| properly.
   *
   * @param aNetwork
   *        The MozWifiNetwork object.
   */
  function setPasswordIfNeeded(aNetwork) {
    let i = getFirstIndexBySsid(aNetwork.ssid, HOSTAPD_CONFIG_LIST);
    if (-1 === i) {
      log('unknown ssid: ' + aNetwork.ssid);
      return; // Unknown network. Assume insecure.
    }

    if (!aNetwork.security.length) {
      return; // No need to set password.
    }

    let security = aNetwork.security[0];
    if (/PSK$/.test(security)) {
      aNetwork.psk = HOSTAPD_CONFIG_LIST[i].wpa_passphrase;
      aNetwork.keyManagement = 'WPA-PSK';
    } else if (/WEP$/.test(security)) {
      aNetwork.wep = HOSTAPD_CONFIG_LIST[i].wpa_passphrase;
      aNetwork.keyManagement = 'WEP';
    }
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
   * @param aAllowError [optional]
   *        A boolean value.  If set to true, an error response won't be treated
   *        as test failure.  Default: false.
   *
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
   * @param aAllowError [optional]
   *        A boolean value.  If set to true, an error response won't be treated
   *        as test failure.  Default: false.
   *
   * @return A deferred promise.
   */
  function getSettings(aKey, aAllowError) {
    let request =
      navigator.mozSettings.createLock().get(aKey);
    return wrapDomRequestAsPromise(request)
      .then(function resolve(aEvent) {
        ok(true, "getSettings(" + aKey + ") - success");
        return aEvent.target.result[aKey];
      }, function reject(aEvent) {
        ok(aAllowError, "getSettings(" + aKey + ") - error");
      });
  }


  /**
   * Start hostapd processes with given configuration list.
   *
   * For starting one hostapd, we need to generate a specific config file
   * then launch a hostapd process with the confg file path passed. The
   * config file is generated by two sources: one is the common
   * part (HOSTAPD_COMMON_CONFIG) and the other is from the given |aConfigList|.
   * Resolve when all the hostpads are requested to start. It is not guaranteed
   * that all the hostapds will be up and running successfully. Never reject.
   *
   * Fulfill params: (none)
   *
   * @param aConfigList
   *        An array of config objects, each property in which will be
   *        output to the confg file with the format: [key]=[value] in one line.
   *        See http://hostap.epitest.fi/cgit/hostap/plain/hostapd/hostapd.conf
   *        for more information.
   *
   * @return A deferred promise.
   */
  function startHostapds(aConfigList) {

    function createConfigFromCommon(aIndex) {
      // Create an copy of HOSTAPD_COMMON_CONFIG.
      let config = JSON.parse(JSON.stringify(HOSTAPD_COMMON_CONFIG));

      // Add user config.
      for (let key in aConfigList[aIndex]) {
        config[key] = aConfigList[aIndex][key];
      }

      // 'interface' is a required field but no need of being configurable
      // for a test case. So we initialize this field on our own.
      config.interface = 'AP-' + aIndex;

      return config;
    }

    function startOneHostapd(aIndex) {
      let configFileName = HOSTAPD_CONFIG_PATH + 'ap' + aIndex + '.conf';
      return writeHostapdConfFile(configFileName, createConfigFromCommon(aIndex))
        .then(() => runEmulatorShellSafe(['hostapd', '-B', configFileName]))
        .then(function (reply) {
          // It may fail at the first time due to the previous ungracefully terminated one.
          if (reply[0] === 'bind(PF_UNIX): Address already in use') {
            return startOneHostapd(aIndex);
          }
        });
    }

    return Promise.all(aConfigList.map(function(aConfig, aIndex) {
      return startOneHostapd(aIndex);
    }));
  }

  /**
   * Kill all the running hostapd processes.
   *
   * Use shell command 'kill -9' to kill all hostapds. Never reject.
   *
   * Fulfill params: (none)
   *
   * @return A deferred promise.
   */
  function killAllHostapd() {
    return getProcessDetail('hostapd')
      .then(function (runningHostapds) {
        let promises = runningHostapds.map(runningHostapd => {
          return runEmulatorShellSafe(['kill', '-9', runningHostapd.pid]);
        });
        return Promise.all(promises);
      });
  }

  /**
   * Write out the config file to the given path.
   *
   * For each key/value pair in |aConfig|,
   *
   * [key]=[value]
   *
   * will be output to one new line. Never reject.
   *
   * Fulfill params: (none)
   *
   * @param aFilePath
   *        The file path that we desire the config file to be located.
   *
   * @param aConfig
   *        The config object.
   *
   * @return A deferred promise.
   */
  function writeHostapdConfFile(aFilePath, aConfig) {
    let content = '';
    for (let key in aConfig) {
      if (aConfig.hasOwnProperty(key)) {
        content += (key + '=' + aConfig[key] + '\n');
      }
    }
    return writeFile(aFilePath, content);
  }

  /**
   * Write file to the given path filled with given content.
   *
   * For now it is implemented by shell command 'echo'. Also, if the
   * content contains whitespace, we need to quote the content to
   * avoid error. Never reject.
   *
   * Fulfill params: (none)
   *
   * @param aFilePath
   *        The file path that we desire the file to be located.
   *
   * @param aContent
   *        The content as string which should be written to the file.
   *
   * @return A deferred promise.
   */
  function writeFile(aFilePath, aContent) {
    if (-1 === aContent.indexOf(' ')) {
      aContent = '"' + aContent + '"';
    }
    return runEmulatorShellSafe(['echo', aContent, '>', aFilePath]);
  }

  /**
   * Check if a init service is running or not.
   *
   * Check the android property 'init.svc.[aServiceName]' to determine if
   * a init service is running. Reject if the propery is neither 'running'
   * nor 'stopped'.
   *
   * Fulfill params:
   *   result -- |true| if the init service is running; |false| otherwise.
   * Reject params: (none)
   *
   * @param aServiceName
   *        The init service name.
   *
   * @return A deferred promise.
   */
  function isInitServiceRunning(aServiceName) {
    return runEmulatorShellSafe(['getprop', 'init.svc.' + aServiceName])
      .then(function (result) {
        if ('running' !== result[0] && 'stopped' !== result[0]) {
          throw 'Init service running state should be "running" or "stopped".';
        }
        return 'running' === result[0];
      });
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
   * Start or stop an init service.
   *
   * Use shell command 'start'/'stop' to start/stop an init service.
   * The running state will also be checked after we start/stop the service.
   * Resolve if the service is successfully started/stopped; Reject otherwise.
   *
   * Fulfill params: (none)
   * Reject params: (none)
   *
   * @param aServiceName
   *        The name of the service we want to start/stop.
   *
   * @param aStart
   *        |true| for starting the init service. |false| for stopping.
   *
   * @return A deferred promise.
   */
  function startStopInitService(aServiceName, aStart) {
    let retryCnt = 5;

    return runEmulatorShellSafe([aStart ? 'start' : 'stop', aServiceName])
      .then(() => isInitServiceRunning(aServiceName))
      .then(function onIsServiceRunningResolved(aIsRunning) {
        if (aStart === aIsRunning) {
          return;
        }

        if (retryCnt-- > 0) {
          log('Failed to ' + (aStart ? 'start ' : 'stop ') + aServiceName +
              '. Retry: ' + retryCnt);

          return waitForTimeout(500)
            .then(() => isInitServiceRunning(aServiceName))
            .then(onIsServiceRunningResolved);
        }

        throw 'Failed to ' + (aStart ? 'start' : 'stop') + ' ' + aServiceName;
      });
  }

  /**
   * Start the stock hostapd.
   *
   * Since the stock hostapd is an init service, use |startStopInitService| to
   * start it. Note that we might fail to start the stock hostapd at the first time
   * for unknown reason so give it the second chance to start again.
   * Resolve when we are eventually successful to start the stock hostapd; Reject
   * otherwise.
   *
   * Fulfill params: (none)
   * Reject params: (none)
   *
   * @return A deferred promise.
   */
  function startStockHostapd() {
    return startStopInitService(STOCK_HOSTAPD_NAME, true)
      .then(null, function onreject() {
        log('Failed to restart goldfish-hostapd at the first time. Try again!');
        return startStopInitService((STOCK_HOSTAPD_NAME), true);
      });
  }

  /**
   * Stop the stock hostapd.
   *
   * Since the stock hostapd is an init service, use |startStopInitService| to
   * stop it.
   *
   * Fulfill params: (none)
   * Reject params: (none)
   *
   * @return A deferred promise.
   */
  function stopStockHostapd() {
    return startStopInitService(STOCK_HOSTAPD_NAME, false);
  }

  /**
   * Get the index of the first matching entry by |ssid|.
   *
   * Find the index of the first entry of |aArray| which property |ssid|
   * is same as |aSsid|.
   *
   * @param aSsid
   *        The ssid that we want to match.
   * @param aArray
   *        An array of objects, each of which should have the property |ssid|.
   *
   * @return The 0-based index of first matching entry if found; -1 otherwise.
   */
  function getFirstIndexBySsid(aSsid, aArray) {
    for (let i = 0; i < aArray.length; i++) {
      if (aArray[i].ssid === aSsid) {
        return i;
      }
    }
    return -1;
  }

  /**
   * Count the number of running process and verify if the count is expected.
   *
   * Return a promise that resolves when the process has expected number
   * of running instances and rejects otherwise.
   *
   * Fulfill params: (none)
   * Reject params: (none)
   *
   * @param aOrigWifiEnabled
   *        Boolean which indicates wifi was originally enabled.
   *
   * @return A deferred promise.
   */
  function verifyNumOfProcesses(aProcessName, aExpectedNum) {
    return getProcessDetail(aProcessName)
      .then(function (detail) {
        if (detail.length === aExpectedNum) {
          return;
        }
        throw 'Unexpected number of running processes:' + aProcessName +
              ', expected: ' + aExpectedNum + ', actual: ' + detail.length;
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
      .then(() => getSettings(SETTINGS_TETHERING_WIFI_IP))
      .then(ip => verifyDefaultRouteAndIp(ip));
  }

  /**
   * Clean up all the allocated resources and running services for the test.
   *
   * After the test no matter success or failure, we should
   * 1) Restore to the wifi original state (enabled or disabled)
   * 2) Wait until all pending emulator shell commands are done.
   *
   * |finsih| will be called in the end.
   *
   * Fulfill params: (none)
   * Reject params: (none)
   *
   * @return A deferred promise.
   */
  function cleanUp() {
    waitFor(function() {
      return ensureWifiEnabled(true)
        .then(forgetAllKnownNetworks)
        .then(() => ensureWifiEnabled(wifiOrigEnabled))
        .then(finish);
    }, function() {
      return pendingEmulatorShellCount === 0;
    });
  }

  /**
   * Init the test environment.
   *
   * Mainly add the required permissions and initialize the wifiManager
   * and the orignal state of wifi. Reject if failing to create
   * window.navigator.mozWifiManager; resolve if all is well.
   *
   * |finsih| will be called in the end.
   *
   * Fulfill params: (none)
   * Reject params: The reject reason.
   *
   * @return A deferred promise.
   */
  function initTestEnvironment() {
    return addRequiredPermissions()
      .then(function() {
        wifiManager = window.navigator.mozWifiManager;
        if (!wifiManager) {
          throw 'window.navigator.mozWifiManager is NULL';
        }
        wifiOrigEnabled = wifiManager.enabled;
      });
  }

  //---------------------------------------------------
  // Public test suite functions
  //---------------------------------------------------
  suite.getWifiManager = (() => wifiManager);
  suite.ensureWifiEnabled = ensureWifiEnabled;
  suite.requestWifiEnabled = requestWifiEnabled;
  suite.startHostapds = startHostapds;
  suite.getProcessDetail = getProcessDetail;
  suite.killAllHostapd = killAllHostapd;
  suite.wrapDomRequestAsPromise = wrapDomRequestAsPromise;
  suite.waitForWifiManagerEventOnce = waitForWifiManagerEventOnce;
  suite.verifyNumOfProcesses = verifyNumOfProcesses;
  suite.testWifiScanWithRetry = testWifiScanWithRetry;
  suite.getFirstIndexBySsid = getFirstIndexBySsid;
  suite.testAssociate = testAssociate;
  suite.getKnownNetworks = getKnownNetworks;
  suite.requestWifiScan = requestWifiScan;
  suite.waitForConnected = waitForConnected;
  suite.forgetNetwork = forgetNetwork;
  suite.waitForTimeout = waitForTimeout;
  suite.waitForRilDataConnected = waitForRilDataConnected;
  suite.requestTetheringEnabled = requestTetheringEnabled;

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
  suite.doTest = function(aTestCaseChain) {
    return initTestEnvironment()
      .then(aTestCaseChain)
      .then(function onresolve() {
        cleanUp();
      }, function onreject(aReason) {
        ok(false, 'Promise rejects during test' + (aReason ? '(' + aReason + ')' : ''));
        cleanUp();
      });
  };

  /**
   * Common test routine without the presence of stock hostapd.
   *
   * Same as doTest except stopping the stock hostapd before test
   * and restarting it after test.
   *
   * Fulfill params: (none)
   * Reject params: (none)
   *
   * @param aTestCaseChain
   *        The test case entry point, which can be a function or a promise.
   *
   * @return A deferred promise.
   */
  suite.doTestWithoutStockAp = function(aTestCaseChain) {
    return suite.doTest(function() {
      return stopStockHostapd()
        .then(aTestCaseChain)
        .then(startStockHostapd);
    });
  };

  /**
   * The common test routine for wifi tethering.
   *
   * Similar as doTest except that it will set 'ril.data.enabled' to true
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
  suite.doTestTethering = function(aTestCaseChain) {

    function verifyInitialState() {
      return getSettings(SETTINGS_RIL_DATA_ENABLED)
        .then(enabled => isOrThrow(enabled, false, SETTINGS_RIL_DATA_ENABLED))
        .then(() => getSettings(SETTINGS_TETHERING_WIFI_ENABLED))
        .then(enabled => isOrThrow(enabled, false, SETTINGS_TETHERING_WIFI_ENABLED));
    }

    function initTetheringTestEnvironment() {
      // Enable ril data.
      return Promise.all([waitForRilDataConnected(true),
                          setSettings1(SETTINGS_RIL_DATA_ENABLED, true)])
        .then(setSettings1(SETTINGS_TETHERING_WIFI_SECURITY, 'open'));
    }

    function restoreToInitialState() {
      return setSettings1(SETTINGS_RIL_DATA_ENABLED, false)
        .then(() => getSettings(SETTINGS_TETHERING_WIFI_ENABLED))
        .then(enabled => is(enabled, false, 'Tethering should be turned off.'));
    }

    return suite.doTest(function() {
      return verifyInitialState()
        .then(initTetheringTestEnvironment)
        .then(aTestCaseChain)
        .then(restoreToInitialState, function onreject(aReason) {
          return restoreToInitialState()
            .then(() => { throw aReason; }); // Re-throw the orignal reject reason.
        });
    });
  };

  return suite;
})();
