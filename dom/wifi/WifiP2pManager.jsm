/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/StateMachine.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/systemlibs.js");

XPCOMUtils.defineLazyServiceGetter(this, "gSysMsgr",
                                   "@mozilla.org/system-message-internal;1",
                                   "nsISystemMessagesInternal");

XPCOMUtils.defineLazyServiceGetter(this, "gNetworkManager",
                                   "@mozilla.org/network/manager;1",
                                   "nsINetworkManager");

this.EXPORTED_SYMBOLS = ["WifiP2pManager"];

const EVENT_IGNORED                      = -1;
const EVENT_UNKNOWN                      = -2;

// Events from supplicant for p2p.
const EVENT_P2P_DEVICE_FOUND             = 0;
const EVENT_P2P_DEVICE_LOST              = 1;
const EVENT_P2P_GROUP_STARTED            = 2;
const EVENT_P2P_GROUP_REMOVED            = 3;
const EVENT_P2P_PROV_DISC_PBC_REQ        = 4;
const EVENT_P2P_PROV_DISC_PBC_RESP       = 5;
const EVENT_P2P_PROV_DISC_SHOW_PIN       = 6;
const EVENT_P2P_PROV_DISC_ENTER_PIN      = 7;
const EVENT_P2P_GO_NEG_REQUEST           = 8;
const EVENT_P2P_GO_NEG_SUCCESS           = 9;
const EVENT_P2P_GO_NEG_FAILURE           = 10;
const EVENT_P2P_GROUP_FORMATION_SUCCESS  = 11;
const EVENT_P2P_GROUP_FORMATION_FAILURE  = 12;
const EVENT_P2P_FIND_STOPPED             = 13;
const EVENT_P2P_INVITATION_RESULT        = 14;
const EVENT_P2P_INVITATION_RECEIVED      = 15;
const EVENT_P2P_PROV_DISC_FAILURE        = 16;

// Events from supplicant but not p2p specific.
const EVENT_AP_STA_DISCONNECTED          = 100;
const EVENT_AP_STA_CONNECTED             = 101;

// Events from DOM.
const EVENT_P2P_SET_PAIRING_CONFIRMATION = 1000;
const EVENT_P2P_CMD_CONNECT              = 1001;
const EVENT_P2P_CMD_DISCONNECT           = 1002;
const EVENT_P2P_CMD_ENABLE               = 1003;
const EVENT_P2P_CMD_DISABLE              = 1004;
const EVENT_P2P_CMD_ENABLE_SCAN          = 1005;
const EVENT_P2P_CMD_DISABLE_SCAN         = 1006;
const EVENT_P2P_CMD_BLOCK_SCAN           = 1007;
const EVENT_P2P_CMD_UNBLOCK_SCAN         = 1008;

// Internal events.
const EVENT_TIMEOUT_PAIRING_CONFIRMATION = 10000;
const EVENT_TIMEOUT_NEG_REQ              = 10001;
const EVENT_TIMEOUT_CONNECTING           = 10002;
const EVENT_P2P_ENABLE_SUCCESS           = 10003;
const EVENT_P2P_ENABLE_FAILED            = 10004;
const EVENT_P2P_DISABLE_SUCCESS          = 10005;

// WPS method string.
const WPS_METHOD_PBC     = "pbc";
const WPS_METHOD_DISPLAY = "display";
const WPS_METHOD_KEYPAD  = "keypad";

// Role string.
const P2P_ROLE_GO     = "GO";
const P2P_ROLE_CLIENT = "client";

// System message for pairing request.
const PAIRING_REQUEST_SYS_MSG = "wifip2p-pairing-request";

// Configuration.
const P2P_INTERFACE_NAME = "p2p0";
const DEFAULT_GO_INTENT = 15;
const DEFAULT_P2P_DEVICE_NAME = "FirefoxPhone";
const P2P_SCAN_TIMEOUT_SEC = 120;
const DEFAULT_P2P_WPS_METHODS = "virtual_push_button physical_display keypad"; // For wpa_supplicant.
const DEFAULT_P2P_DEVICE_TYPE = "10-0050F204-5"; // For wpa_supplicant.

const GO_NETWORK_INTERFACE = {
  ip:         "192.168.2.1",
  maskLength: 24,
  gateway:    "192.168.2.1",
  dns1:       "0.0.0.0",
  dns2:       "0.0.0.0",
  dhcpServer: "192.168.2.1"
};

const GO_DHCP_SERVER_IP_RANGE = {
  startIp: "192.168.2.10",
  endIp:   "192.168.2.30"
};

let gDebug = false;

// Device Capability bitmap
const DEVICE_CAPAB_SERVICE_DISCOVERY         = 1;
const DEVICE_CAPAB_CLIENT_DISCOVERABILITY    = 1<<1;
const DEVICE_CAPAB_CONCURRENT_OPER           = 1<<2;
const DEVICE_CAPAB_INFRA_MANAGED             = 1<<3;
const DEVICE_CAPAB_DEVICE_LIMIT              = 1<<4;
const DEVICE_CAPAB_INVITATION_PROCEDURE      = 1<<5;

// Group Capability bitmap
const GROUP_CAPAB_GROUP_OWNER                = 1;
const GROUP_CAPAB_PERSISTENT_GROUP           = 1<<1;
const GROUP_CAPAB_GROUP_LIMIT                = 1<<2;
const GROUP_CAPAB_INTRA_BSS_DIST             = 1<<3;
const GROUP_CAPAB_CROSS_CONN                 = 1<<4;
const GROUP_CAPAB_PERSISTENT_RECONN          = 1<<5;
const GROUP_CAPAB_GROUP_FORMATION            = 1<<6;

// Constants defined in wpa_supplicants.
const DEV_PW_REGISTRAR_SPECIFIED = 5;
const DEV_PW_USER_SPECIFIED      = 1;
const DEV_PW_PUSHBUTTON          = 4;

this.WifiP2pManager = function (aP2pCommand, aNetUtil) {
  function debug(aMsg) {
    if (gDebug) {
      dump('-------------- WifiP2pManager: ' + aMsg);
    }
  }

  let manager = {};

  let _stateMachine = P2pStateMachine(aP2pCommand, aNetUtil);

  // Set debug flag to true or false.
  //
  // @param aDebug Boolean to indicate enabling or disabling the debug flag.
  manager.setDebug = function(aDebug) {
    gDebug = aDebug;
  };

  // Set observer of observing internal state machine events.
  //
  // @param aObserver Used to notify WifiWorker what's happening
  //        in the internal p2p state machine.
  manager.setObserver = function(aObserver) {
    _stateMachine.setObserver(aObserver);
  };

  // Handle wpa_supplicant events.
  //
  // @param aEventString string from wpa_supplicant.
  manager.handleEvent = function(aEventString) {
    let event = parseEventString(aEventString);
    if (EVENT_UNKNOWN === event.id || EVENT_IGNORED === event.id) {
      debug('Unknow or ignored event: ' + aEventString);
      return false;
    }
    return _stateMachine.sendEvent(event);
  };

  // Set the confirmation of pairing request.
  //
  // @param aResult Object of confirmation result which contains:
  //    .accepted: user granted.
  //    .pin: pin code which is displaying or input by user.
  //    .wpsMethod: string of "pbc" or "display" or "keypad".
  manager.setPairingConfirmation = function(aResult) {
    let event = {
      id: EVENT_P2P_SET_PAIRING_CONFIRMATION,
      info: {
        accepted: aResult.accepted,
        pin: aResult.pin
      }
    };
    _stateMachine.sendEvent(event);
  };

  // Connect to a known peer.
  //
  // @param aAddress MAC address of the peer to connect.
  // @param aWpsMethod String of "pbc" or "display" or "keypad".
  // @param aGoIntent Number from 0 to 15.
  // @param aCallback Callback |true| on attempting to connect.
  //                          |false| on failed to connect.
  manager.connect = function(aAddress, aWpsMethod, aGoIntent, aCallback) {
    let event = {
      id: EVENT_P2P_CMD_CONNECT,
      info: {
        wpsMethod: aWpsMethod,
        address: aAddress,
        goIntent: aGoIntent,
        onDoConnect: aCallback
      }
    };
    _stateMachine.sendEvent(event);
  };

  // Disconnect with a known peer.
  //
  // @param aAddress The address the user desires to disconect.
  // @param aCallback Callback |true| on "attempting" to disconnect.
  //                           |false| on failed to disconnect.
  manager.disconnect = function(aAddress, aCallback) {
    let event = {
      id: EVENT_P2P_CMD_DISCONNECT,
      info: {
        address: aAddress,
        onDoDisconnect: aCallback
      }
    };
    _stateMachine.sendEvent(event);
  };

  // Enable/disable wifi p2p.
  //
  // @param aEnabled |true| to enable, |false| to disable.
  // @param aCallbacks object for callbacks:
  //   .onEnabled
  //   .onDisabled
  //   .onSupplicantConnected
  manager.setEnabled = function(aEnabled, aCallbacks) {
    let event = {
      id: (aEnabled ? EVENT_P2P_CMD_ENABLE : EVENT_P2P_CMD_DISABLE),
      info: {
        onEnabled: aCallbacks.onEnabled,
        onDisabled: aCallbacks.onDisabled,
        onSupplicantConnected: aCallbacks.onSupplicantConnected
      }
    };
    _stateMachine.sendEvent(event);
  };

  // Enable/disable the wifi p2p scan.
  //
  // @param aEnabled |true| to enable scan, |false| to disable scan.
  // @param aCallback Callback |true| on success to enable/disable scan.
  //                           |false| on failed to enable/disable scan.
  manager.setScanEnabled = function(aEnabled, aCallback) {
    let event = {
      id: (aEnabled ? EVENT_P2P_CMD_ENABLE_SCAN : EVENT_P2P_CMD_DISABLE_SCAN),
      info: { callback: aCallback }
    };
    _stateMachine.sendEvent(event);
  };

  // Block wifi p2p scan.
  manager.blockScan = function() {
    _stateMachine.sendEvent({ id: EVENT_P2P_CMD_BLOCK_SCAN });
  };

  // Un-block and do the pending scan if any.
  manager.unblockScan = function() {
    _stateMachine.sendEvent({ id: EVENT_P2P_CMD_UNBLOCK_SCAN });
  };

  // Set the p2p device name.
  manager.setDeviceName = function(newDeivceName, callback) {
    aP2pCommand.setDeviceName(newDeivceName, callback);
  };

  // Parse wps_supplicant event string.
  //
  // @param aEventString The raw event string from wpa_supplicant.
  //
  // @return Object:
  //   .id: a number to represent an event.
  //   .info: the additional information carried by this event string.
  function parseEventString(aEventString) {
    if (isIgnoredEvent(aEventString)) {
      return { id: EVENT_IGNORED };
    }

    let match = RegExp("p2p_dev_addr=([0-9a-fA-F:]+) " +
                       "pri_dev_type=([0-9a-zA-Z-]+) " +
                       "name='(.*)' " +
                       "config_methods=0x([0-9a-fA-F]+) " +
                       "dev_capab=0x([0-9a-fA-F]+) " +
                       "group_capab=0x([0-9a-fA-F]+) ").exec(aEventString + ' ');

    let tokens = aEventString.split(" ");

    let id = EVENT_UNKNOWN;

    // general info.
    let info = {};

    if (match) {
      info = {
        address:   match[1] ? match[1] : null,
        type:      match[2] ? match[2] : null,
        name:      match[3] ? match[3] : null,
        wpsFlag:   match[4] ? parseInt(match[4], 16) : null,
        devFlag:   match[5] ? parseInt(match[5], 16) : null,
        groupFlag: match[6] ? parseInt(match[6], 16) : null
      };
    }

    if (0 === aEventString.indexOf("P2P-DEVICE-FOUND")) {
      id = EVENT_P2P_DEVICE_FOUND;
      info.wpsCapabilities = wpsFlagToCapabilities(info.wpsFlag);
      info.isGroupOwner = isPeerGroupOwner(info.groupFlag);
    } else if (0 === aEventString.indexOf("P2P-DEVICE-LOST")) {
      // e.g. "P2P-DEVICE-LOST p2p_dev_addr=5e:0a:5b:15:1f:80".
      id = EVENT_P2P_DEVICE_LOST;
      info.address = /p2p_dev_addr=([0-9a-f:]+)/.exec(aEventString)[1];
    } else if (0 === aEventString.indexOf("P2P-GROUP-STARTED")) {
      // e.g. "P2P-GROUP-STARTED wlan0-p2p-0 GO ssid="DIRECT-3F Testing
      //       passphrase="12345678" go_dev_addr=02:40:61:c2:f3:b7 [PERSISTENT]".

      id = EVENT_P2P_GROUP_STARTED;
      let groupMatch = RegExp('ssid="(.*)" ' +
                              'freq=([0-9]*) ' +
                              '(passphrase|psk)=([^ ]+) ' +
                              'go_dev_addr=([0-9a-f:]+)').exec(aEventString);
      info.ssid = groupMatch[1];
      info.freq = groupMatch[2];
      if ('passphrase' === groupMatch[3]) {
        let s = groupMatch[4]; // e.g. "G7jHkkz9".
        info.passphrase = s.substring(1, s.length-1); // Trim the double quote.
      } else { // psk
        info.psk = groupMatch[4];
      }
      info.goAddress = groupMatch[5];
      info.ifname = tokens[1];
      info.role = tokens[2];
    } else if (0 === aEventString.indexOf("P2P-GROUP-REMOVED")) {
      id = EVENT_P2P_GROUP_REMOVED;
      // e.g. "P2P-GROUP-REMOVED wlan0-p2p-0 GO".
      info.ifname = tokens[1];
      info.role = tokens[2];
    } else if (0 === aEventString.indexOf("P2P-PROV-DISC-PBC-REQ")) {
      id = EVENT_P2P_PROV_DISC_PBC_REQ;
      info.wpsMethod = WPS_METHOD_PBC;
    } else if (0 === aEventString.indexOf("P2P-PROV-DISC-PBC-RESP")) {
      id = EVENT_P2P_PROV_DISC_PBC_RESP;
      // The address is different from the general pattern.
      info.address = aEventString.split(" ")[1];
      info.wpsMethod = WPS_METHOD_PBC;
    } else if (0 === aEventString.indexOf("P2P-PROV-DISC-SHOW-PIN")) {
      id = EVENT_P2P_PROV_DISC_SHOW_PIN;
      // Obtain peer address and pin from tokens.
      info.address = tokens[1];
      info.pin     = tokens[2];
      info.wpsMethod = WPS_METHOD_DISPLAY;
    } else if (0 === aEventString.indexOf("P2P-PROV-DISC-ENTER-PIN")) {
      id = EVENT_P2P_PROV_DISC_ENTER_PIN;
      // Obtain peer address from tokens.
      info.address = tokens[1];
      info.wpsMethod = WPS_METHOD_KEYPAD;
    } else if (0 === aEventString.indexOf("P2P-GO-NEG-REQUEST")) {
      id = EVENT_P2P_GO_NEG_REQUEST;
      info.address = tokens[1];
      switch (parseInt(tokens[2].split("=")[1], 10)) {
        case DEV_PW_REGISTRAR_SPECIFIED: // (5) Peer is display.
          info.wpsMethod = WPS_METHOD_KEYPAD;
          break;
        case DEV_PW_USER_SPECIFIED: // (1) Peer is keypad.
          info.wpsMethod = WPS_METHOD_DISPLAY;
          break;
        case DEV_PW_PUSHBUTTON: // (4) Peer is pbc.
          info.wpsMethod = WPS_METHOD_PBC;
          break;
        default:
          debug('Unknown wps method from event P2P-GO-NEG-REQUEST');
          break;
      }
    } else if (0 === aEventString.indexOf("P2P-GO-NEG-SUCCESS")) {
      id = EVENT_P2P_GO_NEG_SUCCESS;
    } else if (0 === aEventString.indexOf("P2P-GO-NEG-FAILURE")) {
      id = EVENT_P2P_GO_NEG_FAILURE;
    } else if (0 === aEventString.indexOf("P2P-GROUP-FORMATION-FAILURE")) {
      id = EVENT_P2P_GROUP_FORMATION_FAILURE;
    } else if (0 === aEventString.indexOf("P2P-GROUP-FORMATION-SUCCESS")) {
      id = EVENT_P2P_GROUP_FORMATION_SUCCESS;
    } else if (0 === aEventString.indexOf("P2P-FIND-STOPPED")) {
      id = EVENT_P2P_FIND_STOPPED;
    } else if (0 === aEventString.indexOf("P2P-INVITATION-RESULT")) {
      id = EVENT_P2P_INVITATION_RESULT;
      info.status = /status=([0-9]+)/.exec(aEventString)[1];
    } else if (0 === aEventString.indexOf("P2P-INVITATION-RECEIVED")) {
      // e.g. "P2P-INVITATION-RECEIVED sa=32:85:a9:da:e6:1f persistent=7".
      id = EVENT_P2P_INVITATION_RECEIVED;
      info.address = /sa=([0-9a-f:]+)/.exec(aEventString)[1];
      info.netId = /persistent=([0-9]+)/.exec(aEventString)[1];
    } else if (0 === aEventString.indexOf("P2P-PROV-DISC-FAILURE")) {
      id = EVENT_P2P_PROV_DISC_FAILURE;
    } else {
      // Not P2P event but we do receive it. Try to recognize it.
      if (0 === aEventString.indexOf("AP-STA-DISCONNECTED")) {
        id = EVENT_AP_STA_DISCONNECTED;
        info.address = tokens[1];
      } else if (0 === aEventString.indexOf("AP-STA-CONNECTED")) {
        id = EVENT_AP_STA_CONNECTED;
        info.address = tokens[1];
      } else {
        // Neither P2P event nor recognized supplicant event.
        debug('Unknwon event string: ' + aEventString);
      }
    }

    let event = {id: id, info: info};
    debug('Event parsing result: ' + aEventString + ": " + JSON.stringify(event));

    return event;
  }

  function isIgnoredEvent(aEventString) {
    const IGNORED_EVENTS = [
      "CTRL-EVENT-BSS-ADDED",
      "CTRL-EVENT-BSS-REMOVED",
      "CTRL-EVENT-SCAN-RESULTS",
      "CTRL-EVENT-STATE-CHANGE",
      "WPS-AP-AVAILABLE",
      "WPS-ENROLLEE-SEEN"
    ];
    for(let i = 0; i < IGNORED_EVENTS.length; i++) {
      if (0 === aEventString.indexOf(IGNORED_EVENTS[i])) {
        return true;
      }
    }
    return false;
  }

  function isPeerGroupOwner(aGroupFlag) {
    return (aGroupFlag & GROUP_CAPAB_GROUP_OWNER) !== 0;
  }

  // Convert flag to a wps capability array.
  //
  // @param aWpsFlag Number that represents the wps capabilities.
  // @return Array of WPS flag.
  function wpsFlagToCapabilities(aWpsFlag) {
    let wpsCapabilities = [];
    if (aWpsFlag & 0x8) {
      wpsCapabilities.push(WPS_METHOD_DISPLAY);
    }
    if (aWpsFlag & 0x80) {
      wpsCapabilities.push(WPS_METHOD_PBC);
    }
    if (aWpsFlag & 0x100) {
      wpsCapabilities.push(WPS_METHOD_KEYPAD);
    }
    return wpsCapabilities;
  }

  _stateMachine.start();
  return manager;
};

function P2pStateMachine(aP2pCommand, aNetUtil) {
  function debug(aMsg) {
    if (gDebug) {
      dump('-------------- WifiP2pStateMachine: ' + aMsg);
    }
  }

  let p2pSm = {};  // The state machine to return.

  let _sm = StateMachine('WIFIP2P'); // The general purpose state machine.

  // Information we need to keep track across states.
  let _observer;

  let _onEnabled;
  let _onDisabled;
  let _onSupplicantConnected;
  let _savedConfig = {}; // Configuration used to do P2P_CONNECT.
  let _groupInfo = {};   // The information of the group we have formed.
  let _removedGroupInfo = {}; // Used to store the group info we are going to remove.

  let _scanBlocked = false;
  let _scanPostponded = false;

  let _localDevice = {
    address: "",
    deviceName: DEFAULT_P2P_DEVICE_NAME + "_" + libcutils.property_get("ro.build.product"),
    wpsCapabilities: [WPS_METHOD_PBC, WPS_METHOD_KEYPAD, WPS_METHOD_DISPLAY]
  };

  let _p2pNetworkInterface = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsINetworkInterface]),

    state: Ci.nsINetworkInterface.NETWORK_STATE_DISCONNECTED,
    type: Ci.nsINetworkInterface.NETWORK_TYPE_WIFI_P2P,
    name: P2P_INTERFACE_NAME,
    ips: [],
    prefixLengths: [],
    dnses: [],
    gateways: [],
    httpProxyHost: null,
    httpProxyPort: null,

    // help
    registered: false,

    getAddresses: function (ips, prefixLengths) {
      ips.value = this.ips.slice();
      prefixLengths.value = this.prefixLengths.slice();

      return this.ips.length;
    },

    getGateways: function (count) {
      if (count) {
        count.value = this.gateways.length;
      }
      return this.gateways.slice();
    },

    getDnses: function (count) {
      if (count) {
        count.value = this.dnses.length;
      }
      return this.dnses.slice();
    }
  };

  //---------------------------------------------------------
  // State machine APIs.
  //---------------------------------------------------------

  // Register the observer which is implemented in WifiP2pWorkerObserver.jsm.
  //
  // @param aObserver:
  //   .onEnabled
  //   .onDisbaled
  //   .onPeerFound
  //   .onPeerLost
  //   .onConnecting
  //   .onConnected
  //   .onDisconnected
  //   .onLocalDeviceChanged
  p2pSm.setObserver = function(aObserver) {
    _observer = aObserver;
  };

  p2pSm.start = function() {
    _sm.start(stateDisabled);
  };

  p2pSm.sendEvent = function(aEvent) {
    let willBeHandled = isInP2pManagedState(_sm.getCurrentState());
    _sm.sendEvent(aEvent);
    return willBeHandled;
  };

  // Initialize internal state machine _sm.
  _sm.setDefaultEventHandler(handleEventCommon);

  //----------------------------------------------------------
  // State definition.
  //----------------------------------------------------------

  // The initial state.
  var stateDisabled = _sm.makeState("DISABLED", {
    enter: function() {
      _onEnabled = null;
      _onSupplicantConnected = null;
      _savedConfig = null;
      _groupInfo = null;
      _removedGroupInfo = null;
      _scanBlocked = false;
      _scanPostponded = false;

      unregisterP2pNetworkInteface();
    },

    handleEvent: function(aEvent) {
      switch (aEvent.id) {
        case EVENT_P2P_CMD_ENABLE:
          _onEnabled = aEvent.info.onEnabled;
          _onSupplicantConnected = aEvent.info.onSupplicantConnected;
          _sm.gotoState(stateEnabling);
          break;

        default:
          return false;
      } // End of switch.
      return true;
    }
  });

  // The state where we are trying to enable wifi p2p.
  var stateEnabling = _sm.makeState("ENABLING", {
    enter: function() {

      function onFailure()
      {
        _onEnabled(false);
        _sm.gotoState(stateDisabled);
      }

      function onSuccess()
      {
        _onEnabled(true);
        _sm.gotoState(stateInactive);
      }

      _sm.pause();

      // Step 1: Connect to p2p0.
      aP2pCommand.connectToSupplicant(function (status) {
        let detail;

        if (0 !== status) {
          debug('Failed to connect to p2p0');
          onFailure();
          return;
        }

        debug('wpa_supplicant p2p0 connected!');
        _onSupplicantConnected();

        // Step 2: Get MAC address.
        if (!_localDevice.address) {
          aP2pCommand.getMacAddress(function (address) {
            if (!address) {
              debug('Failed to get MAC address....');
              onFailure();
              return;
            }
            debug('Got mac address: ' + address);
            _localDevice.address = address;
            _observer.onLocalDeviceChanged(_localDevice);
          });
        }

        // Step 3: Enable p2p with the device name and wps methods.
        detail = { deviceName: _localDevice.deviceName,
                   deviceType: libcutils.property_get("ro.moz.wifi.p2p_device_type") || DEFAULT_P2P_DEVICE_TYPE,
                   wpsMethods: libcutils.property_get("ro.moz.wifi.p2p_wps_methods") || DEFAULT_P2P_WPS_METHODS };

        aP2pCommand.p2pEnable(detail, function (success) {
          if (!success) {
            debug('Failed to enable p2p');
            onFailure();
            return;
          }

          debug('P2P is enabled! Enabling net interface...');

          // Step 4: Enable p2p0 net interface. wpa_supplicant may have
          //         already done it for us.
          aNetUtil.enableInterface(P2P_INTERFACE_NAME, function (success) {
            onSuccess();
          });
        });
      });
    },

    handleEvent: function(aEvent) {
      // We won't receive any event since all of them will be blocked.
      return true;
    }
  });

  // The state just after enabling wifi direct.
  var stateInactive = _sm.makeState("INACTIVE", {
    enter: function() {
      registerP2pNetworkInteface();

      if (_sm.getPreviousState() !== stateEnabling) {
        _observer.onDisconnected(_savedConfig);
      }

      _savedConfig = null; // Used to connect p2p peer.
      _groupInfo   = null; // The information of the formed group.
    },

    handleEvent: function(aEvent) {
      switch (aEvent.id) {
        // Receiving the following 3 states implies someone is trying to
        // connect to me.
        case EVENT_P2P_PROV_DISC_PBC_REQ:
        case EVENT_P2P_PROV_DISC_SHOW_PIN:
        case EVENT_P2P_PROV_DISC_ENTER_PIN:
          debug('Someone is trying to connect to me: ' + JSON.stringify(aEvent.info));

          _savedConfig = {
            name:      aEvent.info.name,
            address:   aEvent.info.address,
            wpsMethod: aEvent.info.wpsMethod,
            goIntent:  DEFAULT_GO_INTENT,
            pin:       aEvent.info.pin // EVENT_P2P_PROV_DISC_SHOW_PIN only.
          };

          _sm.gotoState(stateWaitingForConfirmation);
          break;

        // Connect to a peer.
        case EVENT_P2P_CMD_CONNECT:
          debug('Trying to connect to peer: ' + JSON.stringify(aEvent.info));

          _savedConfig = {
            address:   aEvent.info.address,
            wpsMethod: aEvent.info.wpsMethod,
            goIntent:  aEvent.info.goIntent
          };

          _sm.gotoState(stateProvisionDiscovery);
          aEvent.info.onDoConnect(true);
          break;

        case EVENT_P2P_INVITATION_RECEIVED:
          _savedConfig = {
            address: aEvent.info.address,
            wpsMethod: WPS_METHOD_PBC,
            goIntent: DEFAULT_GO_INTENT,
            netId: aEvent.info.netId
          };
          _sm.gotoState(stateWaitingForInvitationConfirmation);
          break;

        case EVENT_P2P_GROUP_STARTED:
          // Most likely the peer just reinvoked a peristen group and succeeeded.

          _savedConfig = { address: aEvent.info.goAddress };

          _sm.pause();
          handleGroupStarted(aEvent.info, function (success) {
            _sm.resume();
          });
          break;

        case EVENT_AP_STA_DISCONNECTED:
          // We will hit this case when we used to be a group owner and
          // requested to remove the group we owned.
          break;

        default:
          return false;
      } // End of switch.
      return true;
    },
  });

  // Waiting for user's confirmation.
  var stateWaitingForConfirmation = _sm.makeState("WAITING_FOR_CONFIRMATION", {
    timeoutTimer: null,

    enter: function() {
      gSysMsgr.broadcastMessage(PAIRING_REQUEST_SYS_MSG, _savedConfig);
      this.timeoutTimer = initTimeoutTimer(30000, EVENT_TIMEOUT_PAIRING_CONFIRMATION);
    },

    handleEvent: function(aEvent) {
      switch (aEvent.id) {
        case EVENT_P2P_SET_PAIRING_CONFIRMATION:
          if (!aEvent.info.accepted) {
            debug('User rejected this request');
            _sm.gotoState(stateInactive); // Reset to inactive state.
            break;
          }

          debug('User accepted this request');

          // The only information we may have to grab from user.
          _savedConfig.pin = aEvent.info.pin;

          // The case that user requested to form a group ealier on.
          // Just go to connecting state and do p2p_connect.
          if (_sm.getPreviousState() === stateProvisionDiscovery) {
            _sm.gotoState(stateConnecting);
            break;
          }

          // Otherwise, wait for EVENT_P2P_GO_NEG_REQUEST.
          _sm.gotoState(stateWaitingForNegReq);
          break;

        case EVENT_TIMEOUT_PAIRING_CONFIRMATION:
          debug('Confirmation timeout!');
          _sm.gotoState(stateInactive);
          break;

        case EVENT_P2P_GO_NEG_REQUEST:
          _sm.deferEvent(aEvent);
          break;

        default:
          return false;
      } // End of switch.

      return true;
    },

    exit: function() {
      this.timeoutTimer.cancel();
      this.timeoutTimer = null;
    }
  });

  var stateWaitingForNegReq = _sm.makeState("WAITING_FOR_NEG_REQ", {
    timeoutTimer: null,

    enter: function() {
      debug('Wait for EVENT_P2P_GO_NEG_REQUEST');
      this.timeoutTimer = initTimeoutTimer(30000, EVENT_TIMEOUT_NEG_REQ);
    },

    handleEvent: function(aEvent) {
      switch (aEvent.id) {
        case EVENT_P2P_GO_NEG_REQUEST:
          if (aEvent.info.wpsMethod !== _savedConfig.wpsMethod) {
            debug('Unmatched wps method: ' + aEvent.info.wpsMethod + ", " + _savedConfig.wpsMetho);
          }
          _sm.gotoState(stateConnecting);
          break;

        case EVENT_TIMEOUT_NEG_REQ:
          debug("Waiting for NEG-REQ timeout");
          _sm.gotoState(stateInactive);
          break;

        default:
          return false;
      } // End of switch.
      return true;
    },

    exit: function() {
      this.timeoutTimer.cancel();
      this.timeoutTimer = null;
    }
  });

  // Waiting for user's confirmation for invitation.
  var stateWaitingForInvitationConfirmation = _sm.makeState("WAITING_FOR_INV_CONFIRMATION", {
    timeoutTimer: null,

    enter: function() {
      gSysMsgr.broadcastMessage(PAIRING_REQUEST_SYS_MSG, _savedConfig);
      this.timeoutTimer = initTimeoutTimer(30000, EVENT_TIMEOUT_PAIRING_CONFIRMATION);
    },

    handleEvent: function(aEvent) {
      switch (aEvent.id) {
        case EVENT_P2P_SET_PAIRING_CONFIRMATION:
          if (!aEvent.info.accepted) {
            debug('User rejected this request');
            _sm.gotoState(stateInactive); // Reset to inactive state.
            break;
          }

          debug('User accepted this request');
          _sm.pause();
          aP2pCommand.p2pGetGroupCapab(_savedConfig.address, function (gc) {
            let isPeeGroupOwner = gc & GROUP_CAPAB_GROUP_OWNER;
            _sm.gotoState(isPeeGroupOwner ? stateGroupAdding : stateReinvoking);
          });

          break;

        case EVENT_TIMEOUT_PAIRING_CONFIRMATION:
          debug('Confirmation timeout!');
          _sm.gotoState(stateInactive);
          break;

        default:
          return false;
      } // End of switch.

      return true;
    },

    exit: function() {
      this.timeoutTimer.cancel();
      this.timeoutTimer = null;
    }
  });

  var stateGroupAdding = _sm.makeState("GROUP_ADDING", {
    timeoutTimer: null,

    enter: function() {
      let self = this;

      _observer.onConnecting(_savedConfig);

      _sm.pause();
      aP2pCommand.p2pGroupAdd(_savedConfig.netId, function (success) {
        if (!success) {
          _sm.gotoState(stateInactive);
          return;
        }
        // Waiting for EVENT_P2P_GROUP_STARTED.
        self.timeoutTimer = initTimeoutTimer(60000, EVENT_TIMEOUT_CONNECTING);
        _sm.resume();
      });
    },

    handleEvent: function(aEvent) {
      switch (aEvent.id) {
        case EVENT_P2P_GROUP_STARTED:
          _sm.pause();
          handleGroupStarted(aEvent.info, function (success) {
            _sm.resume();
          });
          break;

        case EVENT_P2P_GO_NEG_FAILURE:
          debug('Negotiation failure. Go back to inactive state');
          _sm.gotoState(stateInactive);
          break;

        case EVENT_TIMEOUT_CONNECTING:
          debug('Connecting timeout! Go back to inactive state');
          _sm.gotoState(stateInactive);
          break;

        case EVENT_P2P_GROUP_FORMATION_SUCCESS:
        case EVENT_P2P_GO_NEG_SUCCESS:
          break;

        case EVENT_P2P_GROUP_FORMATION_FAILURE:
          debug('Group formation failure');
          _sm.gotoState(stateInactive);
          break;

        case EVENT_P2P_GROUP_REMOVED:
          debug('Received P2P-GROUP-REMOVED due to previous failed handleGroupdStarted()');
          _removedGroupInfo = {
            role:   aEvent.info.role,
            ifname: aEvent.info.ifname
          };
          _sm.gotoState(stateDisconnecting);
          break;

        default:
          return false;
      } // End of switch.

      return true;
    },

    exit: function() {
      this.timeoutTimer.cancel();
      this.timeoutTimer = null;
    }
  });

  var stateReinvoking = _sm.makeState("REINVOKING", {
    timeoutTimer: null,

    enter: function() {
      let self = this;

      _observer.onConnecting(_savedConfig);
      _sm.pause();
      aP2pCommand.p2pReinvoke(_savedConfig.netId, _savedConfig.address, function(success) {
        if (!success) {
          _sm.gotoState(stateInactive);
          return;
        }
        // Waiting for EVENT_P2P_GROUP_STARTED.
        self.timeoutTimer = initTimeoutTimer(60000, EVENT_TIMEOUT_CONNECTING);
        _sm.resume();
      });
    },

    handleEvent: function(aEvent) {
      switch (aEvent.id) {
        case EVENT_P2P_GROUP_STARTED:
          _sm.pause();
          handleGroupStarted(aEvent.info, function(success) {
            _sm.resume();
          });
          break;

        case EVENT_P2P_GO_NEG_FAILURE:
          debug('Negotiation failure. Go back to inactive state');
          _sm.gotoState(stateInactive);
          break;

        case EVENT_TIMEOUT_CONNECTING:
          debug('Connecting timeout! Go back to inactive state');
          _sm.gotoState(stateInactive);
          break;

        case EVENT_P2P_GROUP_FORMATION_SUCCESS:
        case EVENT_P2P_GO_NEG_SUCCESS:
          break;

        case EVENT_P2P_GROUP_FORMATION_FAILURE:
          debug('Group formation failure');
          _sm.gotoState(stateInactive);
          break;

        case EVENT_P2P_GROUP_REMOVED:
          debug('Received P2P-GROUP-REMOVED due to previous failed handleGroupdStarted()');
          _removedGroupInfo = {
            role:   aEvent.info.role,
            ifname: aEvent.info.ifname
          };
          _sm.gotoState(stateDisconnecting);
          break;

        default:
          return false;
      } // End of switch.

      return true;
    },

    exit: function() {
      this.timeoutTimer.cancel();
    }
  });

  var stateProvisionDiscovery = _sm.makeState("PROVISION_DISCOVERY", {
    enter: function() {
      function onDiscoveryCommandSent(success) {
        if (!success) {
          _sm.gotoState(stateInactive);
          debug('Failed to send p2p_prov_disc. Go back to inactive state.');
          return;
        }

        debug('p2p_prov_disc has been sent.');

        _sm.resume();
        // Waiting for EVENT_P2P_PROV_DISC_PBC_RESP or
        //             EVENT_P2P_PROV_DISC_SHOW_PIN or
        //             EVENT_P2P_PROV_DISC_ENTER_PIN.
      }

      _sm.pause();
      aP2pCommand.p2pProvDiscovery(_savedConfig.address,
                                   toPeerWpsMethod(_savedConfig.wpsMethod),
                                   onDiscoveryCommandSent);
    },

    handleEvent: function(aEvent) {
      switch (aEvent.id) {
        case EVENT_P2P_PROV_DISC_PBC_RESP:
          _sm.gotoState(stateConnecting); // No need for local user grant.
          break;
        case EVENT_P2P_PROV_DISC_SHOW_PIN:
        case EVENT_P2P_PROV_DISC_ENTER_PIN:
          if (aEvent.info.wpsMethod !== _savedConfig.wpsMethod) {
            debug('Unmatched wps method: ' + aEvent.info.wpsMethod + ":" + _savedConfig.wpsMethod);
          }
          if (EVENT_P2P_PROV_DISC_SHOW_PIN === aEvent.id) {
            _savedConfig.pin = aEvent.info.pin;
          }
          _sm.gotoState(stateWaitingForConfirmation);
          break;

        case EVENT_P2P_PROV_DISC_FAILURE:
          _sm.gotoState(stateInactive);
          break;

        default:
          return false;
      } // End of switch.
      return true;
    }
  });

  // We are going to connect to the peer.
  // |_savedConfig| is supposed to have been filled properly.
  var stateConnecting = _sm.makeState("CONNECTING", {
    timeoutTimer: null,

    enter: function() {
      let self = this;

      if (null === _savedConfig.goIntent) {
        _savedConfig.goIntent = DEFAULT_GO_INTENT;
      }

      _observer.onConnecting(_savedConfig);

      let wpsMethodWithPin;
      if (WPS_METHOD_KEYPAD === _savedConfig.wpsMethod ||
          WPS_METHOD_DISPLAY === _savedConfig.wpsMethod) {
        // e.g. '12345678 display or '12345678 keypad'.
        wpsMethodWithPin = (_savedConfig.pin + ' ' + _savedConfig.wpsMethod);
      } else {
        // e.g. 'pbc'.
        wpsMethodWithPin = _savedConfig.wpsMethod;
      }

      _sm.pause();

      aP2pCommand.p2pGetGroupCapab(_savedConfig.address, function(gc) {
        debug('group capabilities of ' + _savedConfig.address + ': ' + gc);

        let isPeerGroupOwner = gc & GROUP_CAPAB_GROUP_OWNER;
        let config = { address:           _savedConfig.address,
                       wpsMethodWithPin:  wpsMethodWithPin,
                       goIntent:          _savedConfig.goIntent,
                       joinExistingGroup: isPeerGroupOwner };

        aP2pCommand.p2pConnect(config, function (success) {
          if (!success) {
            debug('Failed to send p2p_connect');
            _sm.gotoState(stateInactive);
            return;
          }
          debug('Waiting for EVENT_P2P_GROUP_STARTED.');
          self.timeoutTimer = initTimeoutTimer(60000, EVENT_TIMEOUT_CONNECTING);
          _sm.resume();
        });
      });
    },

    handleEvent: function(aEvent) {
      switch (aEvent.id) {
        case EVENT_P2P_GROUP_STARTED:
          _sm.pause();
          handleGroupStarted(aEvent.info, function (success) {
            _sm.resume();
          });
          break;

        case EVENT_P2P_GO_NEG_FAILURE:
          debug('Negotiation failure. Go back to inactive state');
          _sm.gotoState(stateInactive);
          break;

        case EVENT_TIMEOUT_CONNECTING:
          debug('Connecting timeout! Go back to inactive state');
          _sm.gotoState(stateInactive);
          break;

        case EVENT_P2P_GROUP_FORMATION_SUCCESS:
        case EVENT_P2P_GO_NEG_SUCCESS:
          break;

        case EVENT_P2P_GROUP_FORMATION_FAILURE:
          debug('Group formation failure');
          _sm.gotoState(stateInactive);
          break;

        case EVENT_P2P_GROUP_REMOVED:
          debug('Received P2P-GROUP-REMOVED due to previous failed ' +
                'handleGroupdStarted()');
          _removedGroupInfo = {
            role:   aEvent.info.role,
            ifname: aEvent.info.ifname
          };
          _sm.gotoState(stateDisconnecting);
          break;

        default:
          return false;
      } // End of switch.

      return true;
    },

    exit: function() {
      this.timeoutTimer.cancel();
    }
  });

  var stateConnected = _sm.makeState("CONNECTED", {
    groupOwner: null,

    enter: function() {
      this.groupOwner = {
        macAddress: _groupInfo.goAddress,
        ipAddress:  _groupInfo.networkInterface.gateway,
        passphrase: _groupInfo.passphrase,
        ssid:       _groupInfo.ssid,
        freq:       _groupInfo.freq,
        isLocal:    _groupInfo.isGroupOwner
      };

      if (!_groupInfo.isGroupOwner) {
        _observer.onConnected(this.groupOwner, _savedConfig);
      } else {
        // If I am a group owner, notify onConnected until EVENT_AP_STA_CONNECTED
        // is received.
      }

      _removedGroupInfo = null;
    },

    handleEvent: function(aEvent) {
      switch (aEvent.id) {
        case EVENT_AP_STA_CONNECTED:
          if (_groupInfo.isGroupOwner) {
            _observer.onConnected(this.groupOwner, _savedConfig);
          }
          break;

        case EVENT_P2P_GROUP_REMOVED:
          _removedGroupInfo = {
            role:   aEvent.info.role,
            ifname: aEvent.info.ifname
          };
          _sm.gotoState(stateDisconnecting);
          break;

        case EVENT_AP_STA_DISCONNECTED:
          debug('Client disconnected: ' + aEvent.info.address);

          // Now we suppose it's the only client. Remove my group.
          _sm.pause();
          aP2pCommand.p2pGroupRemove(_groupInfo.ifname, function (success) {
            debug('Requested to remove p2p group. Wait for EVENT_P2P_GROUP_REMOVED.');
            _sm.resume();
          });
          break;

        case EVENT_P2P_CMD_DISCONNECT:
          // Since we only support single connection, we can ignore
          // the given peer address.
          _sm.pause();
          aP2pCommand.p2pGroupRemove(_groupInfo.ifname, function(success) {
            aEvent.info.onDoDisconnect(true);
            _sm.resume();
          });

          debug('Sent disconnect command. Wait for EVENT_P2P_GROUP_REMOVED.');
          break;

        case EVENT_P2P_PROV_DISC_PBC_REQ:
        case EVENT_P2P_PROV_DISC_SHOW_PIN:
        case EVENT_P2P_PROV_DISC_ENTER_PIN:
          debug('Someone is trying to connect to me: ' + JSON.stringify(aEvent.info));

          _savedConfig = {
            name:      aEvent.info.name,
            address:   aEvent.info.address,
            wpsMethod: aEvent.info.wpsMethod,
            pin:       aEvent.info.pin
          };

          _sm.gotoState(stateWaitingForJoiningConfirmation);
          break;

        default:
          return false;
      } // end of switch
      return true;
    }
  });

  var stateWaitingForJoiningConfirmation = _sm.makeState("WAITING_FOR_JOINING_CONFIRMATION", {
    timeoutTimer: null,

    enter: function() {
      gSysMsgr.broadcastMessage(PAIRING_REQUEST_SYS_MSG, _savedConfig);
      this.timeoutTimer = initTimeoutTimer(30000, EVENT_TIMEOUT_PAIRING_CONFIRMATION);
    },

    handleEvent: function (aEvent) {
      switch (aEvent.id) {
        case EVENT_P2P_SET_PAIRING_CONFIRMATION:
          if (!aEvent.info.accepted) {
            debug('User rejected invitation!');
            _sm.gotoState(stateConnected);
            break;
          }

          let onWpsCommandSent = function(success) {
            _observer.onConnecting(_savedConfig);
            _sm.gotoState(stateConnected);
          };

          _sm.pause();
          if (WPS_METHOD_PBC === _savedConfig.wpsMethod) {
            aP2pCommand.wpsPbc(onWpsCommandSent, _groupInfo.ifname);
          } else {
            let detail = { pin: _savedConfig.pin, iface: _groupInfo.ifname };
            aP2pCommand.wpsPin(detail, onWpsCommandSent);
          }
          break;

        case EVENT_TIMEOUT_PAIRING_CONFIRMATION:
          debug('WAITING_FOR_JOINING_CONFIRMATION timeout!');
          _sm.gotoState(stateConnected);
          break;

        default:
          return false;
      } // End of switch.
      return true;
    },

    exit: function() {
      this.timeoutTimer.cancel();
      this.timeoutTimer = null;
    }
  });

  var stateDisconnecting = _sm.makeState("DISCONNECTING", {
    enter: function() {
      _sm.pause();
      handleGroupRemoved(_removedGroupInfo, function (success) {
        if (!success) {
          debug('Failed to handle group removed event. What can I do?');
        }
        _sm.gotoState(stateInactive);
      });
    },

    handleEvent: function(aEvent) {
      return false; // We will not receive any event in this state.
    }
  });

  var stateDisabling = _sm.makeState("DISABLING", {
    enter: function() {
      _sm.pause();
      aNetUtil.stopDhcpServer(function (success) { // Stopping DHCP server is harmless.
        debug('Stop DHCP server result: ' + success);
        aP2pCommand.p2pDisable(function(success) {
          debug('P2P function disabled');
          aP2pCommand.closeSupplicantConnection(function (status) {
            debug('Supplicant connection closed');
            aNetUtil.disableInterface(P2P_INTERFACE_NAME, function (success){
              debug('Disabled interface: ' + P2P_INTERFACE_NAME);
              _onDisabled(true);
              _sm.gotoState(stateDisabled);
            });
          });
        });
      });
    },

    handleEvent: function(aEvent) {
      return false; // We will not receive any event in this state.
    }
  });

  //----------------------------------------------------------
  // Helper functions.
  //----------------------------------------------------------

  // Handle 'P2P_GROUP_STARTED' event. Note that this function
  // will also do the state transitioning and error handling.
  //
  // @param aInfo Information carried by "P2P_GROUP_STARTED" event:
  //   .role: P2P_ROLE_GO or P2P_ROLE_CLIENT
  //   .ssid:
  //   .freq:
  //   .passphrase: Used to connect to GO for legacy device.
  //   .goAddress:
  //   .ifname: e.g. p2p-p2p0
  //
  // @param aCallback Callback function.
  function handleGroupStarted(aInfo, aCallback) {
    debug('handleGroupStarted: ' + JSON.stringify(aInfo));

    function onSuccess()
    {
      _sm.gotoState(stateConnected);
      aCallback(true);
    }

    function onFailure()
    {
      debug('Failed to handleGroupdStarted(). Remove the group...');
      aP2pCommand.p2pGroupRemove(aInfo.ifname, function (success) {
        aCallback(false);

        if (success) {
          return; // Stay in current state and wait for EVENT_P2P_GROUP_REMOVED.
        }

        debug('p2pGroupRemove command error!');
        _sm.gotoState(stateInactive);
      });
    }

    // Save this group information.
    _groupInfo = aInfo;
    _groupInfo.isGroupOwner = (P2P_ROLE_GO === aInfo.role);

    if (_groupInfo.isGroupOwner) {
      debug('Group owner. Start DHCP server');
      let dhcpServerConfig = { ifname: aInfo.ifname,
                               startIp: GO_DHCP_SERVER_IP_RANGE.startIp,
                               endIp: GO_DHCP_SERVER_IP_RANGE.endIp,
                               serverIp: GO_NETWORK_INTERFACE.ip,
                               maskLength: GO_NETWORK_INTERFACE.maskLength };

      aNetUtil.startDhcpServer(dhcpServerConfig, function (success) {
        if (!success) {
          debug('Failed to start DHCP server');
          onFailure();
          return;
        }

        // Update p2p network interface.
        _p2pNetworkInterface.state = Ci.nsINetworkInterface.NETWORK_STATE_CONNECTED;
        _p2pNetworkInterface.ips = [GO_NETWORK_INTERFACE.ip];
        _p2pNetworkInterface.prefixLengths = [GO_NETWORK_INTERFACE.maskLength];
        _p2pNetworkInterface.gateways = [GO_NETWORK_INTERFACE.ip];
        handleP2pNetworkInterfaceStateChanged();

        _groupInfo.networkInterface = _p2pNetworkInterface;

        debug('Everything is done. Happy p2p GO~');
        onSuccess();
      });

      return;
    }

    // We are the client.

    debug("Client. Request IP from DHCP server on interface: " + _groupInfo.ifname);

    aNetUtil.runDhcp(aInfo.ifname, function(dhcpData) {
      if(!dhcpData || !dhcpData.info) {
        debug('Failed to run DHCP client');
        onFailure();
        return;
      }

      // Save network interface.
      debug("DHCP request success: " + JSON.stringify(dhcpData.info));

      // Update p2p network interface.
      let maskLength =
        netHelpers.getMaskLength(netHelpers.stringToIP(dhcpData.info.mask_str));
      if (!maskLength) {
        maskLength = 32; // max prefix for IPv4.
      }
      _p2pNetworkInterface.state = Ci.nsINetworkInterface.NETWORK_STATE_CONNECTED;
      _p2pNetworkInterface.ips = [dhcpData.info.ipaddr_str];
      _p2pNetworkInterface.prefixLengths = [maskLength];
      if (typeof dhcpData.info.dns1_str == "string" &&
          dhcpData.info.dns1_str.length) {
        _p2pNetworkInterface.dnses.push(dhcpData.info.dns1_str);
      }
      if (typeof dhcpData.info.dns2_str == "string" &&
          dhcpData.info.dns2_str.length) {
        _p2pNetworkInterface.dnses.push(dhcpData.info.dns2_str);
      }
      _p2pNetworkInterface.gateways = [dhcpData.info.gateway_str];
      handleP2pNetworkInterfaceStateChanged();

      _groupInfo.networkInterface = _p2pNetworkInterface;

      debug('Happy p2p client~');
      onSuccess();
    });
  }

  function resetP2pNetworkInterface() {
    _p2pNetworkInterface.state = Ci.nsINetworkInterface.NETWORK_STATE_DISCONNECTED;
    _p2pNetworkInterface.ips = [];
    _p2pNetworkInterface.prefixLengths = [];
    _p2pNetworkInterface.dnses = [];
    _p2pNetworkInterface.gateways = [];
  }

  function registerP2pNetworkInteface() {
    if (!_p2pNetworkInterface.registered) {
      resetP2pNetworkInterface();
      gNetworkManager.registerNetworkInterface(_p2pNetworkInterface);
      _p2pNetworkInterface.registered = true;
    }
  }

  function unregisterP2pNetworkInteface() {
    if (_p2pNetworkInterface.registered) {
      resetP2pNetworkInterface();
      gNetworkManager.unregisterNetworkInterface(_p2pNetworkInterface);
      _p2pNetworkInterface.registered = false;
    }
  }

  function handleP2pNetworkInterfaceStateChanged() {
    gNetworkManager.updateNetworkInterface(_p2pNetworkInterface);
  }

  // Handle 'P2P_GROUP_STARTED' event.
  //
  // @param aInfo information carried by "P2P_GROUP_REMOVED" event:
  //   .ifname
  //   .role: "GO" or "client".
  //
  // @param aCallback Callback function.
  function handleGroupRemoved(aInfo, aCallback) {
    if (!_groupInfo) {
      debug('No group info. Why?');
      aCallback(true);
      return;
    }
    if (_groupInfo.ifname !== aInfo.ifname ||
        _groupInfo.role   !== aInfo.role) {
      debug('Unmatched group info: ' + JSON.stringify(_groupInfo) +
            ' v.s. ' + JSON.stringify(aInfo));
    }

    // Update p2p network interface.
    _p2pNetworkInterface.state = Ci.nsINetworkInterface.NETWORK_STATE_DISCONNECTED;
    handleP2pNetworkInterfaceStateChanged();

    if (P2P_ROLE_GO === aInfo.role) {
      aNetUtil.stopDhcpServer(function(success) {
        debug('Stop DHCP server result: ' + success);
        aCallback(true);
      });
    } else {
      aNetUtil.stopDhcp(aInfo.ifname, function() {
        aCallback(true);
      });
    }
  }

  // Non state-specific event handler.
  function handleEventCommon(aEvent) {
    switch (aEvent.id) {
      case EVENT_P2P_DEVICE_FOUND:
        _observer.onPeerFound(aEvent.info);
        break;

      case EVENT_P2P_DEVICE_LOST:
        _observer.onPeerLost(aEvent.info);
        break;

      case EVENT_P2P_CMD_DISABLE:
        _onDisabled = aEvent.info.onDisabled;
        _sm.gotoState(stateDisabling);
        break;

      case EVENT_P2P_CMD_ENABLE_SCAN:
        if (_scanBlocked) {
          _scanPostponded = true;
          aEvent.info.callback(true);
          break;
        }
        aP2pCommand.p2pEnableScan(P2P_SCAN_TIMEOUT_SEC, aEvent.info.callback);
        break;

      case EVENT_P2P_CMD_DISABLE_SCAN:
        aP2pCommand.p2pDisableScan(aEvent.info.callback);
        break;

      case EVENT_P2P_FIND_STOPPED:
        break;

      case EVENT_P2P_CMD_BLOCK_SCAN:
        _scanBlocked = true;
        aP2pCommand.p2pDisableScan(function(success) {});
        break;

      case EVENT_P2P_CMD_UNBLOCK_SCAN:
        _scanBlocked = false;
        if (_scanPostponded) {
          aP2pCommand.p2pEnableScan(P2P_SCAN_TIMEOUT_SEC, function(success) {});
        }
        break;

      case EVENT_P2P_CMD_CONNECT:
      case EVENT_P2P_CMD_DISCONNECT:
        debug("The current state couldn't handle connect/disconnect request. Ignore it.");
        break;

      default:
        return false;
    } // End of switch.
    return true;
  }

  function isInP2pManagedState(aState) {
    let p2pManagedStates = [stateWaitingForConfirmation,
                            stateWaitingForNegReq,
                            stateProvisionDiscovery,
                            stateWaitingForInvitationConfirmation,
                            stateGroupAdding,
                            stateReinvoking,
                            stateConnecting,
                            stateConnected,
                            stateDisconnecting];

    for (let i = 0; i < p2pManagedStates.length; i++) {
      if (aState === p2pManagedStates[i]) {
        return true;
      }
    }

    return false;
  }

  function initTimeoutTimer(aTimeoutMs, aTimeoutEvent) {
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    function onTimerFired() {
      _sm.sendEvent({ id: aTimeoutEvent });
      timer = null;
    }
    timer.initWithCallback(onTimerFired.bind(this), aTimeoutMs,
                           Ci.nsITimer.TYPE_ONE_SHOT);
    return timer;
  }

  // Converts local WPS method to peer WPS method.
  function toPeerWpsMethod(aLocalWpsMethod) {
    switch (aLocalWpsMethod) {
      case WPS_METHOD_DISPLAY:
        return WPS_METHOD_KEYPAD;
      case WPS_METHOD_KEYPAD:
        return WPS_METHOD_DISPLAY;
      case WPS_METHOD_PBC:
        return WPS_METHOD_PBC;
      default:
        return WPS_METHOD_PBC; // Use "push button" as the default method.
    }
  }

  return p2pSm;
}

this.WifiP2pManager.INTERFACE_NAME = P2P_INTERFACE_NAME;
