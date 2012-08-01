/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const DEBUG = false; // set to true to show debug messages

const WIFIWORKER_CONTRACTID = "@mozilla.org/wifi/worker;1";
const WIFIWORKER_CID        = Components.ID("{a14e8977-d259-433a-a88d-58dd44657e5b}");

const WIFIWORKER_WORKER     = "resource://gre/modules/wifi_worker.js";

const kNetworkInterfaceStateChangedTopic = "network-interface-state-changed";

XPCOMUtils.defineLazyServiceGetter(this, "gNetworkManager",
                                   "@mozilla.org/network/manager;1",
                                   "nsINetworkManager");

// A note about errors and error handling in this file:
// The libraries that we use in this file are intended for C code. For
// C code, it is natural to return -1 for errors and 0 for success.
// Therefore, the code that interacts directly with the worker uses this
// convention (note: command functions do get boolean results since the
// command always succeeds and we do a string/boolean check for the
// expected results).
var WifiManager = (function() {
  function getSdkVersionAndDevice() {
    Cu.import("resource://gre/modules/ctypes.jsm");
    try {
      let cutils = ctypes.open("libcutils.so");
      let cbuf = ctypes.char.array(4096)();
      let c_property_get = cutils.declare("property_get", ctypes.default_abi,
                                          ctypes.int,       // return value: length
                                          ctypes.char.ptr,  // key
                                          ctypes.char.ptr,  // value
                                          ctypes.char.ptr); // default
      let property_get = function (key, defaultValue) {
        if (defaultValue === undefined) {
          defaultValue = null;
        }
        c_property_get(key, cbuf, defaultValue);
        return cbuf.readString();
      }
      return { sdkVersion: parseInt(property_get("ro.build.version.sdk")),
               device: property_get("ro.product.device") };
    } catch(e) {
      // Eat it.  Hopefully we're on a non-Gonk system ...
      // 
      // XXX we should check that
      return 0;
    }
  }

  let { sdkVersion, device } = getSdkVersionAndDevice();

  var controlWorker = new ChromeWorker(WIFIWORKER_WORKER);
  var eventWorker = new ChromeWorker(WIFIWORKER_WORKER);

  // Callbacks to invoke when a reply arrives from the controlWorker.
  var controlCallbacks = Object.create(null);
  var idgen = 0;

  function controlMessage(obj, callback) {
    var id = idgen++;
    obj.id = id;
    if (callback)
      controlCallbacks[id] = callback;
    controlWorker.postMessage(obj);
  }

  function onerror(e) {
    // It is very important to call preventDefault on the event here.
    // If an exception is thrown on the worker, it bubbles out to the
    // component that created it. If that component doesn't have an
    // onerror handler, the worker will try to call the error reporter
    // on the context it was created on. However, That doesn't work
    // for component contexts and can result in crashes. This onerror
    // handler has to make sure that it calls preventDefault on the
    // incoming event.
    e.preventDefault();

    var worker = (this === controlWorker) ? "control" : "event";

    debug("Got an error from the " + worker + " worker: " + e.filename +
          ":" + e.lineno + ": " + e.message + "\n");
  }

  controlWorker.onerror = onerror;
  eventWorker.onerror = onerror;

  controlWorker.onmessage = function(e) {
    var data = e.data;
    var id = data.id;
    var callback = controlCallbacks[id];
    if (callback) {
      callback(data);
      delete controlCallbacks[id];
    }
  };

  // Polling the status worker
  var recvErrors = 0;
  eventWorker.onmessage = function(e) {
    // process the event and tell the event worker to listen for more events
    if (handleEvent(e.data.event))
      waitForEvent();
  };

  function waitForEvent() {
    eventWorker.postMessage({ cmd: "wait_for_event" });
  }

  // Commands to the control worker

  function voidControlMessage(cmd, callback) {
    controlMessage({ cmd: cmd }, function (data) {
      callback(data.status);
    });
  }

  var driverLoaded = false;
  function loadDriver(callback) {
    if (driverLoaded) {
      callback(0);
      return;
    }

    voidControlMessage("load_driver", function(status) {
      driverLoaded = (status >= 0);
      callback(status)
    });
  }

  function unloadDriver(callback) {
    // Otoro ICS can't unload and then load the driver, so never unload it.
    if (device === "otoro") {
      callback(0);
      return;
    }

    voidControlMessage("unload_driver", function(status) {
      driverLoaded = (status < 0);
      callback(status);
    });
  }

  function startSupplicant(callback) {
    voidControlMessage("start_supplicant", callback);
  }

  function terminateSupplicant(callback) {
    doBooleanCommand("TERMINATE", "OK", callback);
  }

  function stopSupplicant(callback) {
    voidControlMessage("stop_supplicant", callback);
  }

  function connectToSupplicant(callback) {
    voidControlMessage("connect_to_supplicant", callback);
  }

  function closeSupplicantConnection(callback) {
    voidControlMessage("close_supplicant_connection", callback);
  }

  function doCommand(request, callback) {
    controlMessage({ cmd: "command", request: request }, callback);
  }

  function doIntCommand(request, callback) {
    doCommand(request, function(data) {
      callback(data.status ? -1 : (data.reply|0));
    });
  }

  function doBooleanCommand(request, expected, callback) {
    doCommand(request, function(data) {
      callback(data.status ? false : (data.reply == expected));
    });
  }

  function doStringCommand(request, callback) {
    doCommand(request, function(data) {
      callback(data.status ? null : data.reply);
    });
  }

  function listNetworksCommand(callback) {
    doStringCommand("LIST_NETWORKS", callback);
  }

  function addNetworkCommand(callback) {
    doIntCommand("ADD_NETWORK", callback);
  }

  function setNetworkVariableCommand(netId, name, value, callback) {
    doBooleanCommand("SET_NETWORK " + netId + " " + name + " " + value, "OK", callback);
  }

  function getNetworkVariableCommand(netId, name, callback) {
    doStringCommand("GET_NETWORK " + netId + " " + name, callback);
  }

  function removeNetworkCommand(netId, callback) {
    doBooleanCommand("REMOVE_NETWORK " + netId, "OK", callback);
  }

  function enableNetworkCommand(netId, disableOthers, callback) {
    doBooleanCommand((disableOthers ? "SELECT_NETWORK " : "ENABLE_NETWORK ") + netId, "OK", callback);
  }

  function disableNetworkCommand(netId, callback) {
    doBooleanCommand("DISABLE_NETWORK " + netId, "OK", callback);
  }

  function statusCommand(callback) {
    doStringCommand("STATUS", callback);
  }

  function pingCommand(callback) {
    doBooleanCommand("PING", "PONG", callback);
  }

  function scanResultsCommand(callback) {
    doStringCommand("SCAN_RESULTS", callback);
  }

  function disconnectCommand(callback) {
    doBooleanCommand("DISCONNECT", "OK", callback);
  }

  function reconnectCommand(callback) {
    doBooleanCommand("RECONNECT", "OK", callback);
  }

  function reassociateCommand(callback) {
    doBooleanCommand("REASSOCIATE", "OK", callback);
  }

  var scanModeActive = false;

  function doSetScanModeCommand(setActive, callback) {
    doBooleanCommand(setActive ? "DRIVER SCAN-ACTIVE" : "DRIVER SCAN-PASSIVE", "OK", callback);
  }

  function scanCommand(forceActive, callback) {
    if (forceActive && !scanModeActive) {
      // Note: we ignore errors from doSetScanMode.
      doSetScanModeCommand(true, function(ignore) {
        doBooleanCommand("SCAN", "OK", function(ok) {
          doSetScanModeCommand(false, function(ignore) {
            // The result of scanCommand is the result of the actual SCAN
            // request.
            callback(ok);
          });
        });
      });
      return;
    }
    doBooleanCommand("SCAN", "OK", callback);
  }

  function setScanModeCommand(setActive, callback) {
    scanModeActive = setActive;
    doSetScanModeCommand(setActive, callback);
  }

  function startDriverCommand(callback) {
    doBooleanCommand("DRIVER START", "OK");
  }

  function stopDriverCommand(callback) {
    doBooleanCommand("DRIVER STOP", "OK");
  }

  function startPacketFiltering(callback) {
    doBooleanCommand("DRIVER RXFILTER-ADD 0", "OK", function(ok) {
      ok && doBooleanCommand("DRIVER RXFILTER-ADD 1", "OK", function(ok) {
        ok && doBooleanCommand("DRIVER RXFILTER-ADD 3", "OK", function(ok) {
          ok && doBooleanCommand("DRIVER RXFILTER-START", "OK", callback)
        });
      });
    });
  }

  function stopPacketFiltering(callback) {
    doBooleanCommand("DRIVER RXFILTER-STOP", "OK", function(ok) {
      ok && doBooleanCommand("DRIVER RXFILTER-REMOVE 3", "OK", function(ok) {
        ok && doBooleanCommand("DRIVER RXFILTER-REMOVE 1", "OK", function(ok) {
          ok && doBooleanCommand("DRIVER RXFILTER-REMOVE 0", "OK", callback)
        });
      });
    });
  }

  function doGetRssiCommand(cmd, callback) {
    doCommand(cmd, function(data) {
      var rssi = -200;

      if (!data.status) {
        // If we are associating, the reply is "OK".
        var reply = data.reply;
        if (reply != "OK") {
          // Format is: <SSID> rssi XX". SSID can contain spaces.
          var offset = reply.lastIndexOf("rssi ");
          if (offset !== -1)
            rssi = reply.substr(offset + 5) | 0;
        }
      }
      callback(rssi);
    });
  }

  function getRssiCommand(callback) {
    doGetRssiCommand("DRIVER RSSI", callback);
  }

  function getRssiApproxCommand(callback) {
    doGetRssiCommand("DRIVER RSSI-APPROX", callback);
  }

  function getLinkSpeedCommand(callback) {
    doStringCommand("DRIVER LINKSPEED", function(reply) {
      if (reply)
        reply = reply.split(" ")[1] | 0; // Format: LinkSpeed XX
      callback(reply);
    });
  }

  function getConnectionInfoGB(callback) {
    var rval = {};
    getRssiApproxCommand(function(rssi) {
      rval.rssi = rssi;
      getLinkSpeedCommand(function(linkspeed) {
        rval.linkspeed = linkspeed;
        callback(rval);
      });
    });
  }

  function getConnectionInfoICS(callback) {
    doStringCommand("SIGNAL_POLL", function(reply) {
      if (!reply) {
        callback(null);
        return;
      }

      let rval = {};
      var lines = reply.split("\n");
      for (let i = 0; i < lines.length; ++i) {
        let [key, value] = lines[i].split("=");
        switch (key.toUpperCase()) {
          case "RSSI":
            rval.rssi = value | 0;
            break;
          case "LINKSPEED":
            rval.linkspeed = value | 0;
            break;
          default:
            // Ignore.
        }
      }

      callback(rval);
    });
  }

  function getMacAddressCommand(callback) {
    doStringCommand("DRIVER MACADDR", function(reply) {
      if (reply)
        reply = reply.split(" ")[2]; // Format: Macaddr = XX.XX.XX.XX.XX.XX
      callback(reply);
    });
  }

  function setPowerModeCommand(mode, callback) {
    doBooleanCommand("DRIVER POWERMODE " + mode, "OK", callback);
  }

  function getPowerModeCommand(callback) {
    doStringCommand("DRIVER GETPOWER", function(reply) {
      if (reply)
        reply = (reply.split()[2]|0); // Format: powermode = XX
      callback(reply);
    });
  }

  function setNumAllowedChannelsCommand(numChannels, callback) {
    doBooleanCommand("DRIVER SCAN-CHANNELS " + numChannels, "OK", callback);
  }

  function getNumAllowedChannelsCommand(callback) {
    doStringCommand("DRIVER SCAN-CHANNELS", function(reply) {
      if (reply)
        reply = (reply.split()[2]|0); // Format: Scan-Channels = X
      callback(reply);
    });
  }

  function setBluetoothCoexistenceModeCommand(mode, callback) {
    doBooleanCommand("DRIVER BTCOEXMODE " + mode, "OK", callback);
  }

  function setBluetoothCoexistenceScanModeCommand(mode, callback) {
    doBooleanCommand("DRIVER BTCOEXSCAN-" + (mode ? "START" : "STOP"), "OK", callback);
  }

  function saveConfigCommand(callback) {
    // Make sure we never write out a value for AP_SCAN other than 1
    doBooleanCommand("AP_SCAN 1", "OK", function(ok) {
      doBooleanCommand("SAVE_CONFIG", "OK", callback);
    });
  }

  function reloadConfigCommand(callback) {
    doBooleanCommand("RECONFIGURE", "OK", callback);
  }

  function setScanResultHandlingCommand(mode, callback) {
    doBooleanCommand("AP_SCAN " + mode, "OK", callback);
  }

  function addToBlacklistCommand(bssid, callback) {
    doBooleanCommand("BLACKLIST " + bssid, "OK", callback);
  }

  function clearBlacklistCommand(callback) {
    doBooleanCommand("BLACKLIST clear", "OK", callback);
  }

  function setSuspendOptimizationsCommand(enabled, callback) {
    doBooleanCommand("DRIVER SETSUSPENDOPT " + (enabled ? 0 : 1), "OK", callback);
  }

  function getProperty(key, defaultValue, callback) {
    controlMessage({ cmd: "property_get", key: key, defaultValue: defaultValue }, function(data) {
      callback(data.status < 0 ? null : data.value);
    });
  }

  function setProperty(key, value, callback) {
    controlMessage({ cmd: "property_set", key: key, value: value }, function(data) {
      callback(!data.status);
    });
  }

  function enableInterface(ifname, callback) {
    controlMessage({ cmd: "ifc_enable", ifname: ifname }, function(data) {
      callback(!data.status);
    });
  }

  function disableInterface(ifname, callback) {
    controlMessage({ cmd: "ifc_disable", ifname: ifname }, function(data) {
      callback(!data.status);
    });
  }

  function addHostRoute(ifname, route, callback) {
    controlMessage({ cmd: "ifc_add_host_route", ifname: ifname, route: route }, function(data) {
      callback(!data.status);
    });
  }

  function removeHostRoutes(ifname, callback) {
    controlMessage({ cmd: "ifc_remove_host_routes", ifname: ifname }, function(data) {
      callback(!data.status);
    });
  }

  function setDefaultRoute(ifname, route, callback) {
    controlMessage({ cmd: "ifc_set_default_route", ifname: ifname, route: route }, function(data) {
      callback(!data.status);
    });
  }

  function getDefaultRoute(ifname, callback) {
    controlMessage({ cmd: "ifc_get_default_route", ifname: ifname }, function(data) {
      callback(!data.route);
    });
  }

  function removeDefaultRoute(ifname, callback) {
    controlMessage({ cmd: "ifc_remove_default_route", ifname: ifname }, function(data) {
      callback(!data.status);
    });
  }

  function resetConnections(ifname, callback) {
    controlMessage({ cmd: "ifc_reset_connections", ifname: ifname }, function(data) {
      callback(!data.status);
    });
  }

  var dhcpInfo = null;
  function runDhcp(ifname, callback) {
    controlMessage({ cmd: "dhcp_do_request", ifname: ifname }, function(data) {
      dhcpInfo = data.status ? null : data;
      callback(dhcpInfo);
    });
  }

  function stopDhcp(ifname, callback) {
    controlMessage({ cmd: "dhcp_stop", ifname: ifname }, function(data) {
      dhcpInfo = null;
      notify("dhcplost");
      callback(!data.status);
    });
  }

  function releaseDhcpLease(ifname, callback) {
    controlMessage({ cmd: "dhcp_release_lease", ifname: ifname }, function(data) {
      dhcpInfo = null;
      notify("dhcplost");
      callback(!data.status);
    });
  }

  function getDhcpError(callback) {
    controlMessage({ cmd: "dhcp_get_errmsg" }, function(data) {
      callback(data.error);
    });
  }

  function configureInterface(ifname, ipaddr, mask, gateway, dns1, dns2, callback) {
    controlMessage({ cmd: "ifc_configure", ifname: ifname,
                     ipaddr: ipaddr, mask: mask, gateway: gateway,
                     dns1: dns1, dns2: dns2}, function(data) {
      callback(!data.status);
    });
  }

  function runDhcpRenew(ifname, callback) {
    controlMessage({ cmd: "dhcp_do_request", ifname: ifname }, function(data) {
      if (!data.status)
        dhcpInfo = data;
      callback(data.status ? null : data);
    });
  }

  var manager = {};

  var suppressEvents = false;
  function notify(eventName, eventObject) {
    if (suppressEvents)
      return;
    var handler = manager["on" + eventName];
    if (handler) {
      if (!eventObject)
        eventObject = ({});
      handler.call(eventObject);
    }
  }

  function notifyStateChange(fields) {
    // If we're already in the COMPLETED state, we might receive events from
    // the supplicant that tell us that we're re-authenticating or reminding
    // us that we're associated to a network. In those cases, we don't need to
    // do anything, so just ignore them.
    if (manager.state === "COMPLETED" &&
        fields.state !== "DISCONNECTED" &&
        fields.state !== "INTERFACE_DISABLED" &&
        fields.state !== "INACTIVE" &&
        fields.state !== "SCANNING") {
      return false;
    }
    fields.prevState = manager.state;
    manager.state = fields.state;

    notify("statechange", fields);
    return true;
  }

  function parseStatus(status, reconnected) {
    if (status === null) {
      debug("Unable to get wpa supplicant's status");
      return;
    }

    var ssid;
    var bssid;
    var state;
    var ip_address;
    var id;

    var lines = status.split("\n");
    for (let i = 0; i < lines.length; ++i) {
      let [key, value] = lines[i].split("=");
      switch (key) {
        case "wpa_state":
          state = value;
          break;
        case "ssid":
          ssid = value;
          break;
        case "bssid":
          bssid = value;
          break;
        case "ip_address":
          ip_address = value;
          break;
        case "id":
          id = value;
          break;
      }
    }

    if (bssid && ssid) {
      manager.connectionInfo.bssid = bssid;
      manager.connectionInfo.ssid = ssid;
      manager.connectionInfo.id = id;
    }

    if (ip_address)
      dhcpInfo = { ip_address: ip_address };

    notifyStateChange({ state: state, fromStatus: true });
    if (state === "COMPLETED")
      onconnected(reconnected);
  }

  // try to connect to the supplicant
  var connectTries = 0;
  var retryTimer = null;
  function connectCallback(ok) {
    if (ok === 0) {
      // Tell the event worker to start waiting for events.
      retryTimer = null;
      didConnectSupplicant(false, function(){});
      return;
    }
    if (connectTries++ < 3) {
      // try again in 5 seconds
      if (!retryTimer)
        retryTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

      retryTimer.initWithCallback(function(timer) {
        connectToSupplicant(connectCallback);
      }, 5000, Ci.nsITimer.TYPE_ONE_SHOT);
      return;
    }

    retryTimer = null;
    notify("supplicantfailed");
  }

  manager.connectionDropped = function(callback) {
    // If we got disconnected, kill the DHCP client in preparation for
    // reconnection.
    resetConnections(manager.ifname, function() {
      stopDhcp(manager.ifname, function() {
        callback();
      });
    });
  }

  manager.start = function() {
    debug("detected SDK version " + sdkVersion + " and device " + device);

    // If we reconnected to an already-running supplicant, then manager.state
    // will have already been updated to the supplicant's state. Otherwise, we
    // started the supplicant ourselves and need to connect.
    if (manager.state === "UNINITIALIZED")
      connectToSupplicant(connectCallback);
  }

  function dhcpAfterConnect() {
    // For now we do our own DHCP. In the future, this should be handed
    // off to the Network Manager.
    runDhcp(manager.ifname, function (data) {
      if (!data) {
        debug("DHCP failed to run");
        notify("dhcpconnected", { info: data });
        return;
      }
      setProperty("net." + manager.ifname + ".dns1", ipToString(data.dns1),
                  function(ok) {
        if (!ok) {
          debug("Unable to set net.<ifname>.dns1");
          return;
        }
        setProperty("net." + manager.ifname + ".dns2", ipToString(data.dns2),
                    function(ok) {
          if (!ok) {
            debug("Unable to set net.<ifname>.dns2");
            return;
          }
          setProperty("net." + manager.ifname + ".gw", ipToString(data.gateway),
                      function(ok) {
            if (!ok) {
              debug("Unable to set net.<ifname>.gw");
              return;
            }
            notify("dhcpconnected", { info: data });
          });
        });
      });
    });
  }

  function onconnected(reconnected) {
    if (!reconnected) {
      dhcpAfterConnect();
      return;
    }

    // We're in the process of reconnecting to a pre-existing wpa_supplicant.
    // Check to see if there was already a DHCP process:
    getProperty("init.svc.dhcpcd_" + manager.ifname, "stopped", function(value) {
      if (value === "running") {
        notify("dhcpconnected");
        return;
      }

      // Some phones use a different property name for the dhcpcd daemon.
      getProperty("init.svc.dhcpcd", "stopped", function(value) {
        if (value === "running") {
          notify("dhcpconnected");
          return;
        }

        dhcpAfterConnect();
      });
    });
  }

  var supplicantStatesMap = (sdkVersion >= 15) ?
    ["DISCONNECTED", "INTERFACE_DISABLED", "INACTIVE", "SCANNING",
     "AUTHENTICATING", "ASSOCIATING", "ASSOCIATED", "FOUR_WAY_HANDSHAKE",
     "GROUP_HANDSHAKE", "COMPLETED"]
    :
    ["DISCONNECTED", "INACTIVE", "SCANNING", "ASSOCIATING",
     "ASSOCIATED", "FOUR_WAY_HANDSHAKE", "GROUP_HANDSHAKE",
     "COMPLETED", "DORMANT", "UNINITIALIZED"];

  var driverEventMap = { STOPPED: "driverstopped", STARTED: "driverstarted", HANGED: "driverhung" };

  // handle events sent to us by the event worker
  function handleEvent(event) {
    debug("Event coming in: " + event);
    if (event.indexOf("CTRL-EVENT-") !== 0) {
      if (event.indexOf("WPA:") == 0 &&
          event.indexOf("pre-shared key may be incorrect") != -1) {
        notify("passwordmaybeincorrect");
      }

      // This is ugly, but we need to grab the SSID here. While we're at it,
      // we grab the BSSID as well.
      var match = /Trying to associate with ([^ ]+) \(SSID='([^']+)' freq=\d+ MHz\)/.exec(event);
      if (match) {
        debug("Matched: " + match[1] + " and " + match[2]);
        manager.connectionInfo.bssid = match[1];
        manager.connectionInfo.ssid = match[2];
      }
      return true;
    }

    var space = event.indexOf(" ");
    var eventData = event.substr(0, space + 1);
    if (eventData.indexOf("CTRL-EVENT-STATE-CHANGE") === 0) {
      // Parse the event data
      var fields = {};
      var tokens = event.substr(space + 1).split(" ");
      for (var n = 0; n < tokens.length; ++n) {
        var kv = tokens[n].split("=");
        if (kv.length === 2)
          fields[kv[0]] = kv[1];
      }
      if (!("state" in fields))
        return true;
      fields.state = supplicantStatesMap[fields.state];

      // The BSSID field is only valid in the ASSOCIATING and ASSOCIATED
      // states, except when we "reauth", except this seems to depend on the
      // driver, so simply check to make sure that we don't have a null BSSID.
      if (fields.BSSID !== "00:00:00:00:00:00")
        manager.connectionInfo.bssid = fields.BSSID;

      notifyStateChange(fields);
      return true;
    }
    if (eventData.indexOf("CTRL-EVENT-DRIVER-STATE") === 0) {
      var handlerName = driverEventMap[eventData];
      if (handlerName)
        notify(handlerName);
      return true;
    }
    if (eventData.indexOf("CTRL-EVENT-TERMINATING") === 0) {
      // If the monitor socket is closed, we have already stopped the
      // supplicant and we can stop waiting for more events and
      // simply exit here (we don't have to notify about having lost
      // the connection).
      if (eventData.indexOf("connection closed") !== -1) {
        notify("supplicantlost");
        return false;
      }

      // As long we haven't seen too many recv errors yet, we
      // will keep going for a bit longer
      if (eventData.indexOf("recv error") !== -1 && ++recvErrors < 10)
        return true;

      notifyStateChange({ state: "DISCONNECTED", BSSID: null, id: -1 });
      notify("supplicantlost");
      return false;
    }
    if (eventData.indexOf("CTRL-EVENT-DISCONNECTED") === 0) {
      manager.connectionInfo.bssid = null;
      manager.connectionInfo.ssid = null;
      manager.connectionInfo.id = -1;
      return true;
    }
    if (eventData.indexOf("CTRL-EVENT-CONNECTED") === 0) {
      // Format: CTRL-EVENT-CONNECTED - Connection to 00:1e:58:ec:d5:6d completed (reauth) [id=1 id_str=]
      var bssid = eventData.split(" ")[4];
      var id = eventData.substr(eventData.indexOf("id=")).split(" ")[0];

      // Don't call onconnected if we ignored this state change (since we were
      // already connected).
      if (notifyStateChange({ state: "CONNECTED", BSSID: bssid, id: id }))
        onconnected(false);
      return true;
    }
    if (eventData.indexOf("CTRL-EVENT-SCAN-RESULTS") === 0) {
      debug("Notifying of scan results available");
      notify("scanresultsavailable");
      return true;
    }
    // unknown event
    return true;
  }

  const SUPP_PROP = "init.svc.wpa_supplicant";
  function killSupplicant(callback) {
    // It is interesting to note that this function does exactly what
    // wifi_stop_supplicant does. Unforunately, on the Galaxy S2, Samsung
    // changed that function in a way that means that it doesn't recognize
    // wpa_supplicant as already running. Therefore, we have to roll our own
    // version here.
    var count = 0;
    var timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    function tick() {
      getProperty(SUPP_PROP, "stopped", function (result) {
        if (result === null) {
          callback();
          return;
        }
        if (result === "stopped" || ++count >= 5) {
          // Either we succeeded or ran out of time.
          timer = null;
          callback();
          return;
        }

        // Else it's still running, continue waiting.
        timer.initWithCallback(tick, 1000, Ci.nsITimer.TYPE_ONE_SHOT);
      });
    }

    setProperty("ctl.stop", "wpa_supplicant", tick);
  }

  function didConnectSupplicant(reconnected, callback) {
    waitForEvent();
    notify("supplicantconnection");

    // Load up the supplicant state.
    statusCommand(function(status) {
      parseStatus(status, reconnected);
      callback();
    });
  }

  function prepareForStartup(callback) {
    // First, check to see if there's a wpa_supplicant running that we can
    // connect to.
    getProperty(SUPP_PROP, "stopped", function (value) {
      if (value !== "running") {
        stopDhcp(manager.ifname, function() { callback(false) });
        return;
      }

      // It's running, try to reconnect to it.
      connectToSupplicant(function (retval) {
        if (retval === 0) {
          // Successfully reconnected! Don't do anything else.
          debug("Successfully connected!");

          manager.supplicantStarted = true;

          // It is important that we call parseStatus (in
          // didConnectSupplicant) before calling the callback here.
          // Otherwise, WifiManager.start will reconnect to it.
          didConnectSupplicant(true, function() { callback(true) });
          return;
        }

        debug("Didn't connect, trying other method.");
        suppressEvents = true;
        stopDhcp(manager.ifname, function() {
          // Ignore any errors.
          killSupplicant(function() {
            suppressEvents = false;
            callback(false);
          });
        });
      });
    });
  }

  // Initial state
  manager.state = "UNINITIALIZED";
  manager.enabled = false;
  manager.supplicantStarted = false;
  manager.connectionInfo = { ssid: null, bssid: null, id: -1 };

  // Public interface of the wifi service
  manager.setWifiEnabled = function(enable, callback) {
    if (enable === manager.enabled) {
      callback("no change");
      return;
    }

    if (enable) {
      // Kill any existing connections if necessary.
      getProperty("wifi.interface", "tiwlan0", function (ifname) {
        if (!ifname) {
          callback(-1);
          return;
        }
        manager.ifname = ifname;

        // Register as network interface.
        WifiNetworkInterface.name = ifname;
        if (!WifiNetworkInterface.registered) {
          gNetworkManager.registerNetworkInterface(WifiNetworkInterface);
          WifiNetworkInterface.registered = true;
        }
        WifiNetworkInterface.state = Ci.nsINetworkInterface.NETWORK_STATE_DISCONNECTED;
        Services.obs.notifyObservers(WifiNetworkInterface,
                                     kNetworkInterfaceStateChangedTopic,
                                     null);

        prepareForStartup(function(already_connected) {
          if (already_connected) {
            callback(0);
            return;
          }

          loadDriver(function (status) {
            if (status < 0) {
              callback(status);
              return;
            }

            let timer;
            function doStartSupplicant() {
              timer = null;
              startSupplicant(function (status) {
                if (status < 0) {
                  unloadDriver(function() {
                    callback(status);
                  });
                  return;
                }

                manager.supplicantStarted = true;
                enableInterface(ifname, function (ok) {
                  callback(ok ? 0 : -1);
                });
              });
            }

            // Driver startup on the otoro takes longer than it takes for us
            // to return from loadDriver, so wait 2 seconds before starting
            // the supplicant to give it a chance to start.
            if (device === "otoro") {
              timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
              timer.init(doStartSupplicant, 2000, Ci.nsITimer.TYPE_ONE_SHOT);
            } else {
              doStartSupplicant();
            }
          });
        });
      });
    } else {
      // Note these following calls ignore errors. If we fail to kill the
      // supplicant gracefully, then we need to continue telling it to die
      // until it does.
      terminateSupplicant(function (ok) {
        manager.connectionDropped(function () {
          stopSupplicant(function (status) {
            manager.state = "UNINITIALIZED";
            closeSupplicantConnection(function () {
              disableInterface(manager.ifname, function (ok) {
                unloadDriver(callback);
              });
            });
          });
        });
      });
    }
  }

  manager.disconnect = disconnectCommand;
  manager.reconnect = reconnectCommand;
  manager.reassociate = reassociateCommand;

  var networkConfigurationFields = [
    "ssid", "bssid", "psk", "wep_key0", "wep_key1", "wep_key2", "wep_key3",
    "wep_tx_keyidx", "priority", "key_mgmt", "scan_ssid", "disabled",
    "identity", "password", "auth_alg"
  ];

  manager.getNetworkConfiguration = function(config, callback) {
    var netId = config.netId;
    var done = 0;
    for (var n = 0; n < networkConfigurationFields.length; ++n) {
      let fieldName = networkConfigurationFields[n];
      getNetworkVariableCommand(netId, fieldName, function(value) {
        if (value !== null)
          config[fieldName] = value;
        if (++done == networkConfigurationFields.length)
          callback(config);
      });
    }
  }
  manager.setNetworkConfiguration = function(config, callback) {
    var netId = config.netId;
    var done = 0;
    var errors = 0;
    for (var n = 0; n < networkConfigurationFields.length; ++n) {
      let fieldName = networkConfigurationFields[n];
      if (!(fieldName in config) ||
          // These fields are special: We can't retrieve them from the
          // supplicant, and often we have a star in our config. In that case,
          // we need to avoid overwriting the correct password with a *.
          (fieldName === "password" ||
           fieldName === "wep_key0" ||
           fieldName === "psk") &&
          config[fieldName] === '*') {
        ++done;
      } else {
        setNetworkVariableCommand(netId, fieldName, config[fieldName], function(ok) {
          if (!ok)
            ++errors;
          if (++done == networkConfigurationFields.length)
            callback(errors == 0);
        });
      }
    }
    // If config didn't contain any of the fields we want, don't lose the error callback
    if (done == networkConfigurationFields.length)
      callback(false);
  }
  manager.getConfiguredNetworks = function(callback) {
    listNetworksCommand(function (reply) {
      var networks = Object.create(null);
      var lines = reply.split("\n");
      if (lines.length === 1) {
        // We need to make sure we call the callback even if there are no
        // configured networks.
        callback(networks);
        return;
      }

      var done = 0;
      var errors = 0;
      for (var n = 1; n < lines.length; ++n) {
        var result = lines[n].split("\t");
        var netId = result[0];
        var config = networks[netId] = { netId: netId };
        switch (result[3]) {
        case "[CURRENT]":
          config.status = "CURRENT";
          break;
        case "[DISABLED]":
          config.status = "DISABLED";
          break;
        default:
          config.status = "ENABLED";
          break;
        }
        manager.getNetworkConfiguration(config, function (ok) {
            if (!ok)
              ++errors;
            if (++done == lines.length - 1) {
              if (errors) {
                // If an error occured, delete the new netId
                removeNetworkCommand(netId, function() {
                  callback(null);
                });
              } else {
                callback(networks);
              }
            }
        });
      }
    });
  }
  manager.addNetwork = function(config, callback) {
    addNetworkCommand(function (netId) {
      config.netId = netId;
      manager.setNetworkConfiguration(config, function (ok) {
        if (!ok) {
          removeNetworkCommand(netId, function() { callback(false); });
          return;
        }

        callback(ok);
      });
    });
  }
  manager.updateNetwork = function(config, callback) {
    manager.setNetworkConfiguration(config, callback);
  }
  manager.removeNetwork = function(netId, callback) {
    removeNetworkCommand(netId, callback);
  }

  function ipToString(n) {
    return String((n >>  0) & 0xFF) + "." +
                 ((n >>  8) & 0xFF) + "." +
                 ((n >> 16) & 0xFF) + "." +
                 ((n >> 24) & 0xFF);
  }

  manager.saveConfig = function(callback) {
    saveConfigCommand(callback);
  }
  manager.enableNetwork = function(netId, disableOthers, callback) {
    enableNetworkCommand(netId, disableOthers, callback);
  }
  manager.disableNetwork = function(netId, callback) {
    disableNetworkCommand(netId, callback);
  }
  manager.getMacAddress = getMacAddressCommand;
  manager.getScanResults = scanResultsCommand;
  manager.setScanMode = function(mode, callback) {
    setScanModeCommand(mode === "active", callback);
  }
  manager.scan = scanCommand;
  manager.getRssiApprox = getRssiApproxCommand;
  manager.getLinkSpeed = getLinkSpeedCommand;
  manager.getDhcpInfo = function() { return dhcpInfo; }
  manager.getConnectionInfo = (sdkVersion >= 15)
                              ? getConnectionInfoICS
                              : getConnectionInfoGB;
  return manager;
})();

function getKeyManagement(flags) {
  var types = [];
  if (!flags)
    return types;

  if (/\[WPA2?-PSK/.test(flags))
    types.push("WPA-PSK");
  if (/\[WPA2?-EAP/.test(flags))
    types.push("WPA-EAP");
  if (/\[WEP/.test(flags))
    types.push("WEP");
  return types;
}

// These constants shamelessly ripped from WifiManager.java
// strength is the value returned by scan_results. It is nominally in dB. We
// transform it into a percentage for clients looking to simply show a
// relative indication of the strength of a network.
const MIN_RSSI = -100;
const MAX_RSSI = -55;

function calculateSignal(strength) {
  // Some wifi drivers represent their signal strengths as 8-bit integers, so
  // in order to avoid negative numbers, they add 256 to the actual values.
  // While we don't *know* that this is the case here, we make an educated
  // guess.
  if (strength > 0)
    strength -= 256;

  if (strength <= MIN_RSSI)
    return 0;
  if (strength >= MAX_RSSI)
    return 100;
  return Math.floor(((strength - MIN_RSSI) / (MAX_RSSI - MIN_RSSI)) * 100);
}

function ScanResult(ssid, bssid, flags, signal) {
  this.ssid = ssid;
  this.bssid = bssid;
  this.capabilities = getKeyManagement(flags);
  this.signalStrength = signal;
  this.relSignalStrength = calculateSignal(Number(signal));

  this.__exposedProps__ = ScanResult.api;
}

// XXX This should probably live in the DOM-facing side, but it's hard to do
// there, so we stick this here.
ScanResult.api = {
  ssid: "r",
  bssid: "r",
  capabilities: "r",
  signalStrength: "r",
  relSignalStrength: "r",
  connected: "r",

  keyManagement: "rw",
  psk: "rw",
  identity: "rw",
  password: "rw",
  wep: "rw"
};

function quote(s) {
  return '"' + s + '"';
}

function dequote(s) {
  if (s[0] != '"' || s[s.length - 1] != '"')
    throw "Invalid argument, not a quoted string: " + s;
  return s.substr(1, s.length - 2);
}

function isWepHexKey(s) {
  if (s.length != 10 && s.length != 26 && s.length != 58)
    return false;
  return !/[^a-fA-F0-9]/.test(s);
}


let WifiNetworkInterface = {

  QueryInterface: XPCOMUtils.generateQI([Ci.nsINetworkInterface]),

  registered: false,

  // nsINetworkInterface

  NETWORK_STATE_UNKNOWN:       Ci.nsINetworkInterface.NETWORK_STATE_UNKNOWN,
  NETWORK_STATE_CONNECTING:    Ci.nsINetworkInterface.CONNECTING,
  NETWORK_STATE_CONNECTED:     Ci.nsINetworkInterface.CONNECTED,
  NETWORK_STATE_SUSPENDED:     Ci.nsINetworkInterface.SUSPENDED,
  NETWORK_STATE_DISCONNECTING: Ci.nsINetworkInterface.DISCONNECTING,
  NETWORK_STATE_DISCONNECTED:  Ci.nsINetworkInterface.DISCONNECTED,

  state: Ci.nsINetworkInterface.NETWORK_STATE_UNKNOWN,

  NETWORK_TYPE_WIFI:       Ci.nsINetworkInterface.NETWORK_TYPE_WIFI,
  NETWORK_TYPE_MOBILE:     Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE,
  NETWORK_TYPE_MOBILE_MMS: Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS,

  type: Ci.nsINetworkInterface.NETWORK_TYPE_WIFI,

  name: null,

  // For now we do our own DHCP. In the future this should be handed off
  // to the Network Manager.
  dhcp: false,

  httpProxyHost: null,

  httpProxyPort: null,

};


// TODO Make the difference between a DOM-based network object and our
// networks objects much clearer.
let netToDOM;
let netFromDOM;

function WifiWorker() {
  var self = this;

  this._mm = Cc["@mozilla.org/parentprocessmessagemanager;1"].getService(Ci.nsIFrameMessageManager);
  const messages = ["WifiManager:setEnabled", "WifiManager:getNetworks",
                    "WifiManager:associate", "WifiManager:forget",
                    "WifiManager:getState"];

  messages.forEach((function(msgName) {
    this._mm.addMessageListener(msgName, this);
  }).bind(this));

  this.wantScanResults = [];

  this._needToEnableNetworks = false;
  this._highestPriority = -1;

  // networks is a map from SSID -> a scan result.
  this.networks = Object.create(null);

  // configuredNetworks is a map from SSID -> our view of a network. It only
  // lists networks known to the wpa_supplicant. The SSID field (and other
  // fields) are quoted for ease of use with WifiManager commands.
  // Note that we don't have to worry about escaping embedded quotes since in
  // all cases, the supplicant will take the last quotation that we pass it as
  // the end of the string.
  this.configuredNetworks = Object.create(null);

  this.currentNetwork = null;

  this._lastConnectionInfo = null;
  this._connectionInfoTimer = null;
  this._reconnectOnDisconnect = false;

  // A list of requests to turn wifi on or off.
  this._stateRequests = [];

  // Given a connection status network, takes a network from
  // self.configuredNetworks and prepares it for the DOM.
  netToDOM = function(net) {
    var pub = { ssid: dequote(net.ssid) };
    if (net.netId)
      pub.known = true;
    return pub;
  };

  netFromDOM = function(net, configured) {
    // Takes a network from the DOM and makes it suitable for insertion into
    // self.configuredNetworks (that is calling addNetwork will do the right
    // thing).
    // NB: Modifies net in place: safe since we don't share objects between
    // the dom and the chrome code.

    // Things that are useful for the UI but not to us.
    delete net.bssid;
    delete net.signalStrength;
    delete net.relSignalStrength;
    delete net.capabilities;

    if (!configured)
      configured = {};

    net.ssid = quote(net.ssid);

    let wep = false;
    if ("keyManagement" in net) {
      if (net.keyManagement === "WEP") {
        wep = true;
        net.keyManagement = "NONE";
      }

      configured.key_mgmt = net.key_mgmt = net.keyManagement; // WPA2-PSK, WPA-PSK, etc.
      delete net.keyManagement;
    } else {
      configured.key_mgmt = net.key_mgmt = "NONE";
    }

    function checkAssign(name, checkStar) {
      if (name in net) {
        let value = net[name];
        if (!value || (checkStar && value === '*')) {
          if (name in configured)
            net[name] = configured[name];
          else
            delete net[name];
        } else {
          configured[name] = net[name] = quote(value);
        }
      }
    }

    checkAssign("psk", true);
    checkAssign("identity", false);
    checkAssign("password", true);
    if (wep && net.wep && net.wep != '*') {
      configured.wep_key0 = net.wep_key0 = isWepHexKey(net.wep) ? net.wep : quote(net.wep);
      configured.auth_alg = net.auth_alg = "OPEN SHARED";
    }

    return net;
  };

  WifiManager.onsupplicantconnection = function() {
    debug("Connected to supplicant");
    WifiManager.enabled = true;
    WifiManager.getMacAddress(function (mac) {
      debug("Got mac: " + mac);
    });

    self._reloadConfiguredNetworks(function(ok) {
      // Prime this.networks.
      if (!ok)
        return;
      self.waitForScan(function firstScan() {});
    });

    // Check if we need to fire request replies first:
    if (self._stateRequests.length > 0)
      self._notifyAfterStateChange(true, true);

    // Notify everybody, even if they didn't ask us to come up.
    self._fireEvent("wifiUp", {});
  }
  WifiManager.onsupplicantlost = function() {
    WifiManager.enabled = WifiManager.supplicantStarted = false;
    debug("Supplicant died!");

    // Check if we need to fire request replies first:
    if (self._stateRequests.length > 0)
      self._notifyAfterStateChange(true, false);

    // Notify everybody, even if they didn't ask us to come up.
    self._fireEvent("wifiDown", {});
  }
  WifiManager.onsupplicantfailed = function() {
    WifiManager.enabled = WifiManager.supplicantStarted = false;
    debug("Couldn't connect to supplicant");
    if (self._stateRequests.length > 0)
      self._notifyAfterStateChange(false, false);
  }

  WifiManager.onstatechange = function() {
    debug("State change: " + this.prevState + " -> " + this.state);

    if (self._connectionInfoTimer &&
        this.state !== "CONNECTED" &&
        this.state !== "COMPLETED") {
      self._stopConnectionInfoTimer();
    }

    switch (this.state) {
      case "DORMANT":
        // The dormant state is a bad state to be in since we won't
        // automatically connect. Try to knock us out of it. We only
        // hit this state when we've failed to run DHCP, so trying
        // again isn't the worst thing we can do. Eventually, we'll
        // need to detect if we're looping in this state and bail out.
        WifiManager.reconnect(function(){});
        break;
      case "ASSOCIATING":
        // id has not yet been filled in, so we can only report the ssid and
        // bssid.
        self.currentNetwork =
          { bssid: WifiManager.connectionInfo.bssid,
            ssid: quote(WifiManager.connectionInfo.ssid) };
        self._fireEvent("onconnecting", { network: netToDOM(self.currentNetwork) });
        break;
      case "ASSOCIATED":
        if (!self.currentNetwork) {
          self.currentNetwork =
            { bssid: WifiManager.connectionInfo.bssid,
              ssid: quote(WifiManager.connectionInfo.ssid) };
        }

        self.currentNetwork.netId = this.id;
        WifiManager.getNetworkConfiguration(self.currentNetwork, function (){});
        break;
      case "COMPLETED":
        // Now that we've successfully completed the connection, re-enable the
        // rest of our networks.
        // XXX Need to do this eventually if the user entered an incorrect
        // password. For now, we require user interaction to break the loop and
        // select a better network!
        if (self._needToEnableNetworks) {
          self._enableAllNetworks();
          self._needToEnableNetworks = false;
        }

        // We get the ASSOCIATED event when we've associated but not connected, so
        // wait until the handshake is complete.
        if (this.fromStatus) {
          // In this case, we connected to an already-connected wpa_supplicant,
          // because of that we need to gather information about the current
          // network here.
          self.currentNetwork = { ssid: quote(WifiManager.connectionInfo.ssid),
                                  netId: WifiManager.connectionInfo.id };
          WifiManager.getNetworkConfiguration(self.currentNetwork, function(){});
        }

        self._startConnectionInfoTimer();
        self._fireEvent("onassociate", { network: netToDOM(self.currentNetwork) });
        break;
      case "CONNECTED":
        break;
      case "DISCONNECTED":
        self._fireEvent("ondisconnect", {});
        self.currentNetwork = null;

        WifiManager.connectionDropped(function() {
          // We've disconnected from a network because of a call to forgetNetwork.
          // Reconnect to the next available network (if any).
          if (self._reconnectOnDisconnect) {
            self._reconnectOnDisconnect = false;
            WifiManager.reconnect(function(){});
          }
        });

        WifiNetworkInterface.state =
          Ci.nsINetworkInterface.NETWORK_STATE_DISCONNECTED;
        Services.obs.notifyObservers(WifiNetworkInterface,
                                     kNetworkInterfaceStateChangedTopic,
                                     null);

        break;
    }
  };

  WifiManager.ondhcpconnected = function() {
    if (this.info) {
      WifiNetworkInterface.state =
        Ci.nsINetworkInterface.NETWORK_STATE_CONNECTED;
      Services.obs.notifyObservers(WifiNetworkInterface,
                                   kNetworkInterfaceStateChangedTopic,
                                   null);

      self._fireEvent("onconnect", { network: netToDOM(self.currentNetwork) });
    } else {
      WifiManager.disconnect(function(){});
    }
  };

  WifiManager.onscanresultsavailable = function() {
    if (self.wantScanResults.length === 0) {
      debug("Scan results available, but we don't need them");
      return;
    }

    debug("Scan results are available! Asking for them.");
    WifiManager.getScanResults(function(r) {
      // Failure.
      if (!r) {
        self.wantScanResults.forEach(function(callback) { callback(null) });
        self.wantScanResults = [];
        return;
      }

      // Now that we have scan results, there's no more need to continue
      // scanning. Ignore any errors from this command.
      WifiManager.setScanMode("inactive", function() {});
      let lines = r.split("\n");
      // NB: Skip the header line.
      self.networks = Object.create(null);
      for (let i = 1; i < lines.length; ++i) {
        // bssid / frequency / signal level / flags / ssid
        var match = /([\S]+)\s+([\S]+)\s+([\S]+)\s+(\[[\S]+\])?\s+(.*)/.exec(lines[i]);

        if (match && match[5]) {
          let ssid = match[5];

          // If this is the first time that we've seen this SSID in the scan
          // results, add it to the list along with any other information.
          // Also, we use the highest signal strength that we see.
          let network = self.networks[ssid];
          if (!network) {
            network = self.networks[ssid] =
              new ScanResult(ssid, match[1], match[4], match[3]);

            if (ssid in self.configuredNetworks) {
              let known = self.configuredNetworks[ssid];
              network.known = true;

              if ("identity" in known && known.identity)
                network.identity = dequote(known.identity);

              // Note: we don't hand out passwords here! The * marks that there
              // is a password that we're hiding.
              if (("psk" in known && known.psk) ||
                  ("password" in known && known.password) ||
                  ("wep_key0" in known && known.wep_key0)) {
                network.password = "*";
              }
            }
          }

          if (network.bssid === WifiManager.connectionInfo.bssid)
            network.connected = true;

          let signal = calculateSignal(Number(match[3]));
          if (signal > network.relSignalStrength)
            network.relSignalStrength = signal;
        } else if (!match) {
          debug("Match didn't find anything for: " + lines[i]);
        }
      }

      self.wantScanResults.forEach(function(callback) { callback(self.networks) });
      self.wantScanResults = [];
    });
  }

  WifiManager.setWifiEnabled(true, function (ok) {
    if (ok === 0)
      WifiManager.start();
    else
      debug("Couldn't start Wifi");
  });

  debug("Wifi starting");
}

function translateState(state) {
  switch (state) {
    case "INTERFACE_DISABLED":
    case "INACTIVE":
    case "SCANNING":
    case "DISCONNECTED":
    default:
      return "disconnected";

    case "AUTHENTICATING":
    case "ASSOCIATING":
    case "ASSOCIATED":
    case "FOUR_WAY_HANDSHAKE":
    case "GROUP_HANDSHAKE":
      return "connecting";

    case "COMPLETED":
      return WifiManager.getDhcpInfo() ? "connected" : "associated";
  }
}

WifiWorker.prototype = {
  classID:   WIFIWORKER_CID,
  classInfo: XPCOMUtils.generateCI({classID: WIFIWORKER_CID,
                                    contractID: WIFIWORKER_CONTRACTID,
                                    classDescription: "WifiWorker",
                                    interfaces: [Ci.nsIWorkerHolder,
                                                 Ci.nsIWifi]}),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWorkerHolder,
                                         Ci.nsIWifi]),

  // Internal methods.
  waitForScan: function(callback) {
    this.wantScanResults.push(callback);
  },

  // In order to select a specific network, we disable the rest of the
  // networks known to us. However, in general, we want the supplicant to
  // connect to which ever network it thinks is best, so when we select the
  // proper network (or fail to), we need to re-enable the rest.
  _enableAllNetworks: function() {
    for each (let net in this.configuredNetworks) {
      WifiManager.enableNetwork(net.netId, false, function(ok) {
        net.disabled = ok ? 1 : 0;
      });
    }
  },

  _startConnectionInfoTimer: function() {
    if (this._connectionInfoTimer)
      return;

    var self = this;
    function getConnectionInformation() {
      WifiManager.getConnectionInfo(function(info) {
        // See comments in calculateSignal for information about this.
        if (!info) {
          self._lastConnectionInfo = null;
          return;
        }

        let { rssi, linkspeed } = info;
        if (rssi > 0)
          rssi -= 256;
        if (rssi <= MIN_RSSI)
          rssi = MIN_RSSI;
        else if (rssi >= MAX_RSSI)
          rssi = MAX_RSSI;

        let info = { signalStrength: rssi,
                     relSignalStrength: calculateSignal(rssi),
                     linkSpeed: linkspeed };
        let last = self._lastConnectionInfo;

        // Only fire the event if the link speed changed or the signal
        // strength changed by more than 10%.
        function tensPlace(percent) ((percent / 10) | 0)

        if (last && last.linkSpeed === info.linkSpeed &&
            tensPlace(last.relSignalStrength) === tensPlace(info.relSignalStrength)) {
          return;
        }

        self._lastConnectionInfo = info;
        debug("Firing connectionInfoUpdate: " + uneval(info));
        self._fireEvent("connectionInfoUpdate", info);
      });
    }

    // Prime our _lastConnectionInfo immediately and fire the event at the
    // same time.
    getConnectionInformation();

    // Now, set up the timer for regular updates.
    this._connectionInfoTimer =
      Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this._connectionInfoTimer.init(getConnectionInformation, 5000,
                                   Ci.nsITimer.TYPE_REPEATING_SLACK);
  },

  _stopConnectionInfoTimer: function() {
    if (!this._connectionInfoTimer)
      return;

    this._connectionInfoTimer.cancel();
    this._connectionInfoTimer = null;
    this._lastConnectionInfo = null;
  },

  _reloadConfiguredNetworks: function(callback) {
    WifiManager.getConfiguredNetworks((function(networks) {
      if (!networks) {
        debug("Unable to get configured networks");
        callback(false);
        return;
      }

      this._highestPriority = -1;

      // Convert between netId-based and ssid-based indexing.
      for (let net in networks) {
        let network = networks[net];
        if (!network.ssid) {
          delete networks[net]; // TODO support these?
          continue;
        }

        if (network.priority && network.priority > this._highestPriority)
          this._highestPriority = network.priority;
        networks[dequote(network.ssid)] = network;
        delete networks[net];
      }

      this.configuredNetworks = networks;
      callback(true);
    }).bind(this));
  },

  // Important side effect: calls WifiManager.saveConfig.
  _reprioritizeNetworks: function(callback) {
    // First, sort the networks in orer of their priority.
    var ordered = Object.getOwnPropertyNames(this.configuredNetworks);
    let self = this;
    ordered.sort(function(a, b) {
      var neta = self.configuredNetworks[a],
          netb = self.configuredNetworks[b];

      // Sort unsorted networks to the end of the list.
      if (isNaN(neta.priority))
        return isNaN(netb.priority) ? 0 : 1;
      if (isNaN(netb.priority))
        return -1;
      return netb.priority - neta.priority;
    });

    // Skip unsorted networks.
    let newPriority = 0, i;
    for (i = ordered.length - 1; i >= 0; --i) {
      if (!isNaN(this.configuredNetworks[ordered[i]].priority))
        break;
    }

    // No networks we care about?
    if (i < 0) {
      WifiManager.saveConfig(callback);
      return;
    }

    // Now assign priorities from 0 to length, starting with the smallest
    // priority and heading towards the highest (note the dependency between
    // total and i here).
    let done = 0, errors = 0, total = i + 1;
    for (; i >= 0; --i) {
      let network = this.configuredNetworks[ordered[i]];
      network.priority = newPriority++;

      // Note: networkUpdated declared below since it happens logically after
      // this loop.
      WifiManager.updateNetwork(network, networkUpdated);
    }

    function networkUpdated(ok) {
      if (!ok)
        ++errors;
      if (++done === total) {
        if (errors > 0) {
          callback(false);
          return;
        }

        WifiManager.saveConfig(function(ok) {
          if (!ok) {
            callback(false);
            return;
          }

          self._reloadConfiguredNetworks(function(ok) {
            callback(ok);
          });
        });
      }
    }
  },

  // nsIWifi

  _fireEvent: function(message, data) {
    this._mm.sendAsyncMessage("WifiManager:" + message, data);
  },

  _sendMessage: function(message, success, data, rid, mid) {
    this._mm.sendAsyncMessage(message + (success ? ":OK" : ":NO"),
                              { data: data, rid: rid, mid: mid });
  },

  receiveMessage: function MessageManager_receiveMessage(aMessage) {
    let msg = aMessage.json;
    switch (aMessage.name) {
      case "WifiManager:setEnabled":
        this.setWifiEnabled(msg.data, msg.rid, msg.mid);
        break;
      case "WifiManager:getNetworks":
        this.getNetworks(msg.rid, msg.mid);
        break;
      case "WifiManager:associate":
        this.associate(msg.data, msg.rid, msg.mid);
        break;
      case "WifiManager:forget":
        this.forget(msg.data, msg.rid, msg.mid);
        break;
      case "WifiManager:getState": {
        let net = this.currentNetwork ? netToDOM(this.currentNetwork) : null;
        return { network: net,
                 connectionInfo: this._lastConnectionInfo,
                 enabled: WifiManager.enabled,
                 status: translateState(WifiManager.state) };
      }
    }
  },

  getNetworks: function(rid, mid) {
    const message = "WifiManager:getNetworks:Return";
    if (WifiManager.state === "UNINITIALIZED") {
      this._sendMessage(message, false, "Wifi is disabled", rid, mid);
      return;
    }

    this.waitForScan((function (networks) {
      this._sendMessage(message, networks !== null, networks, rid, mid);
    }).bind(this));
    WifiManager.scan(true, function() {});
  },

  _notifyAfterStateChange: function(success, newState) {
    // First, notify all of the requests that were trying to make this change.
    let state = this._stateRequests[0].enabled;

    // If the new state is not the same as state, then we weren't processing
    // the first request (we were racing somehow) so don't notify.
    if (!success || state === newState) {
      do {
        let req = this._stateRequests.shift();
        this._sendMessage("WifiManager:setEnabled:Return",
                          success, state, req.rid, req.mid);

        // Don't remove more than one request if the previous one failed.
      } while (success &&
               this._stateRequests.length &&
               this._stateRequests[0].enabled === state);
    }

    // If there were requests queued after this one, run them.
    if (this._stateRequests.length > 0) {
      let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      let self = this;
      timer.initWithCallback(function(timer) {
        WifiManager.setWifiEnabled(self._stateRequests[0].enabled,
                                   self._setWifiEnabledCallback.bind(this));
        timer = null;
      }, 1000, Ci.nsITimer.TYPE_ONE_SHOT);
    }
  },

  _setWifiEnabledCallback: function(status) {
    if (status === "no change") {
      this._notifyAfterStateChange(true, this._stateRequests[0].enabled);
      return;
    }

    if (status) {
      // Don't call notifyAndContinue because we don't want to skip another
      // attempt to turn wifi on or off if this one failed.
      this._notifyAfterStateChange(false, this._stateRequests[0].enabled);
      return;
    }

    // If we're enabling ourselves, then wait until we've connected to the
    // supplicant to notify. If we're disabling, we take care of this in
    // supplicantlost.
    if (WifiManager.supplicantStarted)
      WifiManager.start();
  },

  setWifiEnabled: function(enable, rid, mid) {
    // There are two problems that we're trying to solve here:
    //   - If we get multiple requests to turn on and off wifi before the
    //     current request has finished, then we need to queue up the requests
    //     and handle each on/off request in turn.
    //   - Because we can't pass a callback to WifiManager.start, we need to
    //     have a way to communicate with our onsupplicantconnection callback.
    this._stateRequests.push({ enabled: enable, rid: rid, mid: mid });
    if (this._stateRequests.length === 1)
      WifiManager.setWifiEnabled(enable, this._setWifiEnabledCallback.bind(this));
  },

  associate: function(network, rid, mid) {
    const MAX_PRIORITY = 9999;
    const message = "WifiManager:associate:Return";
    if (WifiManager.state === "UNINITIALIZED") {
      this._sendMessage(message, false, "Wifi is disabled", rid, mid);
      return;
    }

    let privnet = network;
    let self = this;
    function networkReady() {
      // saveConfig now before we disable most of the other networks.
      function selectAndConnect() {
        WifiManager.enableNetwork(privnet.netId, true, function (ok) {
          if (ok)
            self._needToEnableNetworks = true;
          if (WifiManager.state === "DISCONNECTED" ||
              WifiManager.state === "SCANNING") {
            WifiManager.reconnect(function (ok) {
              self._sendMessage(message, ok, ok, rid, mid);
            });
          } else {
            self._sendMessage(message, ok, ok, rid, mid);
          }
        });
      }

      if (self._highestPriority >= MAX_PRIORITY)
        self._reprioritizeNetworks(selectAndConnect);
      else
        WifiManager.saveConfig(selectAndConnect);
    }

    let ssid = privnet.ssid;
    let configured;

    if (ssid in this.configuredNetworks)
      configured = this.configuredNetworks[ssid];

    netFromDOM(privnet, configured);

    privnet.priority = ++this._highestPriority;
    if (configured) {
      privnet.netId = configured.netId;
      WifiManager.updateNetwork(privnet, (function(ok) {
        if (!ok) {
          this._sendMessage(message, false, "Network is misconfigured", rid, mid);
          return;
        }

        networkReady();
      }).bind(this));
    } else {
      // networkReady, above, calls saveConfig. We want to remember the new
      // network as being enabled, which isn't the default, so we explicitly
      // set it to being "enabled" before we add it and save the
      // configuration.
      privnet.disabled = 0;
      WifiManager.addNetwork(privnet, (function(ok) {
        if (!ok) {
          this._sendMessage(message, false, "Network is misconfigured", rid, mid);
          return;
        }

        this.configuredNetworks[ssid] = privnet;
        networkReady();
      }).bind(this));
    }
  },

  forget: function(network, rid, mid) {
    const message = "WifiManager:forget:Return";
    if (WifiManager.state === "UNINITIALIZED") {
      this._sendMessage(message, false, "Wifi is disabled", rid, mid);
      return;
    }

    let ssid = network.ssid;
    if (!(ssid in this.configuredNetworks)) {
      this._sendMessage(message, false, "Trying to forget an unknown network", rid, mid);
      return;
    }

    let self = this;
    let configured = this.configuredNetworks[ssid];
    this._reconnectOnDisconnect = (this.currentNetwork &&
                                   (this.currentNetwork.ssid === ssid));
    WifiManager.removeNetwork(configured.netId, function(ok) {
      if (!ok) {
        self._sendMessage(message, false, "Unable to remove the network", rid, mid);
        self._reconnectOnDisconnect = false;
        return;
      }

      WifiManager.saveConfig(function() {
        self._reloadConfiguredNetworks(function() {
          self._sendMessage(message, true, true, rid, mid);
        });
      });
    });
  },

  // This is a bit ugly, but works. In particular, this depends on the fact
  // that RadioManager never actually tries to get the worker from us.
  get worker() { throw "Not implemented"; },

  shutdown: function() {
    debug("shutting down ...");
    this.setWifiEnabled(false);
  }
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([WifiWorker]);

let debug;
if (DEBUG) {
  debug = function (s) {
    dump("-*- WifiWorker component: " + s + "\n");
  };
} else {
  debug = function (s) {};
}
