/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Telephony.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Andreas Gal <gal@mozilla.com>
 *   Blake Kaplan <mrbkap@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const DEBUG = true; // set to false to suppress debug messages

const WIFIWORKER_CONTRACTID = "@mozilla.org/wifi/worker;1";
const WIFIWORKER_CID        = Components.ID("{a14e8977-d259-433a-a88d-58dd44657e5b}");

const WIFIWORKER_WORKER     = "resource://gre/modules/network_worker.js";

// A note about errors and error handling in this file:
// The libraries that we use in this file are intended for C code. For
// C code, it is natural to return -1 for errors and 0 for success.
// Therefore, the code that interacts directly with the worker uses this
// convention (note: command functions do get boolean results since the
// command always succeeds and we do a string/boolean check for the
// expected results).
var WifiManager = (function() {
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

  function loadDriver(callback) {
    voidControlMessage("load_driver", callback);
  }

  function unloadDriver(callback) {
    voidControlMessage("unload_driver", callback);
  }

  function startSupplicant(callback) {
    voidControlMessage("start_supplicant", callback);
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
      doSetScanModeCommand(true, function(ok) {
        ok && doBooleanCommand("SCAN", "OK", function(ok) {
          ok && doSetScanModeCommand(false, callback);
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
          if (offset != -1)
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
        reply = reply.split()[1] | 0; // Format: LinkSpeed XX
      callback(reply);
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
      if (!data.status)
        dhcpInfo = data;
      notify("dhcpconnected", { info: dhcpInfo });
      callback(data.status ? null : data);
    });
  }

  function stopDhcp(ifname, callback) {
    controlMessage({ cmd: "dhcp_stop", ifname: ifname }, function(data) {
      if (!data.status)
        dhcpInfo = null;
      notify("dhcplost");
      callback(!data.status);
    });
  }

  function releaseDhcpLease(ifname, callback) {
    controlMessage({ cmd: "dhcp_release_lease", ifname: ifname }, function(data) {
      if (!data.status)
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
    fields.prevState = manager.state;
    manager.state = fields.state;

    // If we got disconnected, kill the DHCP client in preparation for
    // reconnection.
    if (fields.state === "DISCONNECTED" && dhcpInfo)
      stopDhcp(manager.ifname, function() {});

    notify("statechange", fields);
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
    notify("supplicantlost");
  }

  manager.start = function() {
    // If we reconnected to an already-running supplicant, then manager.state
    // will have already been updated to the supplicant's state. Otherwise, we
    // started the supplicant ourselves and need to connect.
    if (manager.state === "UNINITIALIZED")
      connectToSupplicant(connectCallback);
  }

  function dhcpAfterConnect() {
    runDhcp(manager.ifname, function (data) {
      if (!data) {
        debug("DHCP failed to run");
        return;
      }
      setProperty("net.dns1", ipToString(data.dns1), function(ok) {
        if (!ok) {
          debug("Unable to set net.dns1");
          return;
        }
        setProperty("net.dns2", ipToString(data.dns2), function(ok) {
          if (!ok) {
            debug("Unable to set net.dns2");
            return;
          }
          getProperty("net.dnschange", "0", function(value) {
            if (value === null) {
              debug("Unable to get net.dnschange");
              return;
            }
            setProperty("net.dnschange", String(Number(value) + 1), function(ok) {
              if (!ok)
                debug("Unable to set net.dnschange");
            });
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

  var supplicantStatesMap = ["DISCONNECTED", "INACTIVE", "SCANNING", "ASSOCIATING",
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
      // states.
      if (fields.state === "ASSOCIATING" || fields.state == "ASSOCIATED")
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
      // simply exit here (we don't have to notify).
      if (eventData.indexOf("connection closed") !== -1)
        return false;

      // As long we haven't seen too many recv errors yet, we
      // will keep going for a bit longer
      if (eventData.indexOf("recv error") !== -1 && ++recvErrors < 10)
        return true;

      notify("supplicantlost");
      return false;
    }
    if (eventData.indexOf("CTRL-EVENT-DISCONNECTED") === 0) {
      notifyStateChange({ state: "DISCONNECTED" });
      manager.connectionInfo.bssid = null;
      manager.connectionInfo.ssid = null;
      return true;
    }
    if (eventData.indexOf("CTRL-EVENT-CONNECTED") === 0) {
      // Format: CTRL-EVENT-CONNECTED - Connection to 00:1e:58:ec:d5:6d completed (reauth) [id=1 id_str=]
      var bssid = eventData.split(" ")[4];
      var id = eventData.substr(eventData.indexOf("id=")).split(" ")[0];
      notifyStateChange({ state: "CONNECTED", BSSID: bssid, id: id });
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
  manager.connectionInfo = { ssid: null, bssid: null, id: -1 };
  manager.enabled = true;

  // Public interface of the wifi service
  manager.setWifiEnabled = function(enable, callback) {
    if ((enable && manager.state !== "UNINITIALIZED") ||
        (!enable && manager.state === "UNINITIALIZED")) {
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
            startSupplicant(function (status) {
              if (status < 0) {
                callback(status);
                return;
              }
              enableInterface(ifname, function (ok) {
                callback(ok ? 0 : -1);
              });
            });
          });
        });
      });
    } else {
      stopSupplicant(function (status) {
        if (status < 0) {
          callback(-1);
          return;
        }

        manager.state = "UNINITIALIZED";
        disableInterface(manager.ifname, function (ok) {
          unloadDriver(callback);
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
      if (!(fieldName in config)) {
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
  this.signal = calculateSignal(Number(signal));
}

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

// TODO Make the difference between a DOM-based network object and our
// networks objects much clearer.
let netToDOM;
let netFromDOM;

function nsWifiWorker() {
  var self = this;

  this._mm = Cc["@mozilla.org/parentprocessmessagemanager;1"].getService(Ci.nsIFrameMessageManager);
  const messages = ["WifiManager:setEnabled", "WifiManager:getNetworks",
                    "WifiManager:associate", "WifiManager:getState"];

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

  // Given a connection status network, takes a network from
  // self.configuredNetworks and prepares it for the DOM.
  netToDOM = function(net) {
    var pub = { ssid: dequote(net.ssid) };
    if (net.netId)
      pub.known = true;
    return pub;
  };

  netFromDOM = function(net) {
    // Takes a network from the DOM and makes it suitable for insertion into
    // self.configuredNetworks (that is calling addNetwork will do the right
    // thing).
    // NB: Modifies net in place: safe since we don't share objects between
    // the dom and the chrome code.

    // Things that are useful for the UI but not to us.
    delete net.bssid;
    delete net.signal;

    net.ssid = quote(net.ssid);

    let wep = false;
    if ("keyManagement" in net) {
      if (net.keyManagement === "WEP") {
        wep = true;
        net.keyManagement = "NONE";
      }

      net.key_mgmt = net.keyManagement; // WPA2-PSK, WPA-PSK, etc.
      delete net.keyManagement;
    } else {
      net.key_mgmt = "NONE";
    }

    function checkAssign(name, checkStar) {
      if (name in net) {
        let value = net[name];
        if (checkStar && value === '*')
          delete net[name];
        else
          net[name] = quote(value);
      }
    }

    checkAssign("psk", true);
    checkAssign("identity", false);
    checkAssign("password", true);
    if (wep && net.wep && net.wep != '*') {
      net.wep_key0 = isWepHexKey(net.wep) ? net.wep : quote(net.wep);
      net.auth_alg = "OPEN SHARED";
    }

    return net;
  };

  WifiManager.onsupplicantconnection = function() {
    debug("Connected to supplicant");
    WifiManager.getMacAddress(function (mac) {
      debug("Got mac: " + mac);
    });

    WifiManager.getConfiguredNetworks(function(networks) {
      if (!networks) {
        debug("Unable to get configured networks");
        return;
      }

      // Convert between netId-based and ssid-based indexing.
      for (let net in networks) {
        let network = networks[net];
        if (!network.ssid) {
          delete networks[net]; // TODO support these?
          continue;
        }

        if (network.priority && network.priority > self._highestPriority)
          self._highestPriority = network.priority;
        networks[dequote(network.ssid)] = network;
        delete networks[net];
      }

      self.configuredNetworks = networks;

      // Prime this.networks.
      self.waitForScan(function firstScan() {});
    });
  }
  WifiManager.onsupplicantlost = function() {
    debug("Supplicant died!");
  }

  WifiManager.onstatechange = function() {
    debug("State change: " + this.prevState + " -> " + this.state);

    if (this.state === "DORMANT") {
      // The dormant state is a bad state to be in since we won't
      // automatically connect. Try to knock us out of it. We only
      // hit this state when we've failed to run DHCP, so trying
      // again isn't the worst thing we can do. Eventually, we'll
      // need to detect if we're looping in this state and bail out.
      WifiManager.reconnect(function(){});
    } else if (this.state === "ASSOCIATING") {
      // id has not yet been filled in, so we can only report the ssid and
      // bssid.
      self.currentNetwork =
        { bssid: WifiManager.connectionInfo.bssid,
          ssid: quote(WifiManager.connectionInfo.ssid) };
    } else if (this.state === "ASSOCIATED") {
      self.currentNetwork.netId = this.id;
      WifiManager.getNetworkConfiguration(self.currentNetwork, function (){});
    } else if (this.state === "COMPLETED") {
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
                                known: true }
        WifiManager.getNetworkConfiguration(self.currentNetwork, function(){});
      }

      self._fireEvent("onassociate", netToDOM(self.currentNetwork));
    } else if (this.state === "DISCONNECTED") {
      self._fireEvent("ondisconnect");
      self.currentNetwork = null;
    }
  };

  WifiManager.ondhcpconnected = function() {
    if (this.info)
      self._fireEvent("onconnect", netToDOM(self.currentNetwork));
    else
      WifiManager.disconnect(function(){});
  };

  WifiManager.onscanresultsavailable = function() {
    if (self.wantScanResults.length === 0) {
      debug("Scan results available, but we don't need them");
      return;
    }

    debug("Scan results are available! Asking for them.");
    WifiManager.getScanResults(function(r) {
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
          if (signal > network.signal)
            network.signal = signal;
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

nsWifiWorker.prototype = {
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

  // nsIWifi

  _fireEvent: function(message, data) {
    this._mm.sendAsyncMessage("WifiManager:" + message, { network: data });
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
      case "WifiManager:getState": {
        let net = this.currentNetwork ? netToDOM(this.currentNetwork) : null;
        return { network: net,
                 enabled: WifiManager.state !== "UNINITIALIZED", };
      }
    }
  },

  getNetworks: function(rid, mid) {
    this.waitForScan((function (networks) {
      this._sendMessage("WifiManager:getNetworks:Return",
                        networks !== null, networks, rid, mid);
    }).bind(this));
    WifiManager.scan(true, function() {});
  },

  setWifiEnabled: function(enable, rid, mid) {
    WifiManager.setWifiEnabled(enable, (function (status) {
      if (enable && status === 0)
        WifiManager.start();
      this._sendMessage("WifiManager:setEnabled:Return",
                        (status === 0), enable, rid, mid);
    }).bind(this));
  },

  associate: function(network, rid, mid) {
    const message = "WifiManager:associate:Return";
    let privnet = network;
    let self = this;
    function networkReady() {
      // saveConfig now before we disable most of the other networks.
      WifiManager.saveConfig(function() {
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
      });
    }

    let ssid = privnet.ssid;
    let configured;

    if (ssid in this.configuredNetworks)
      configured = this.configuredNetworks[ssid];

    netFromDOM(privnet);

    // XXX Do we have to worry about overflow/going too high here?
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

  // This is a bit ugly, but works. In particular, this depends on the fact
  // that RadioManager never actually tries to get the worker from us.
  get worker() { throw "Not implemented"; },

  shutdown: function() {
    this.setWifiEnabled(false);
  }
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([nsWifiWorker]);

let debug;
if (DEBUG) {
  debug = function (s) {
    dump("-*- nsWifiWorker component: " + s + "\n");
  };
} else {
  debug = function (s) {};
}
