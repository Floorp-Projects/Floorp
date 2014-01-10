/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["WifiCommand"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/systemlibs.js");

const SUPP_PROP = "init.svc.wpa_supplicant";
const WPA_SUPPLICANT = "wpa_supplicant";

this.WifiCommand = function(aControlMessage, aInterface) {
  var command = {};

  //-------------------------------------------------
  // General commands.
  //-------------------------------------------------

  command.loadDriver = function (callback) {
    voidControlMessage("load_driver", function(status) {
      callback(status);
    });
  };

  command.unloadDriver = function (callback) {
    voidControlMessage("unload_driver", function(status) {
      callback(status);
    });
  };

  command.startSupplicant = function (callback) {
    voidControlMessage("start_supplicant", callback);
  };

  command.killSupplicant = function (callback) {
    // It is interesting to note that this function does exactly what
    // wifi_stop_supplicant does. Unforunately, on the Galaxy S2, Samsung
    // changed that function in a way that means that it doesn't recognize
    // wpa_supplicant as already running. Therefore, we have to roll our own
    // version here.
    stopProcess(SUPP_PROP, WPA_SUPPLICANT, callback);
  };

  command.terminateSupplicant = function (callback) {
    doBooleanCommand("TERMINATE", "OK", callback);
  };

  command.stopSupplicant = function (callback) {
    voidControlMessage("stop_supplicant", callback);
  };

  command.listNetworks = function (callback) {
    doStringCommand("LIST_NETWORKS", callback);
  };

  command.addNetwork = function (callback) {
    doIntCommand("ADD_NETWORK", callback);
  };

  command.setNetworkVariable = function (netId, name, value, callback) {
    doBooleanCommand("SET_NETWORK " + netId + " " + name + " " +
                      value, "OK", callback);
  };

  command.getNetworkVariable = function (netId, name, callback) {
    doStringCommand("GET_NETWORK " + netId + " " + name, callback);
  };

  command.removeNetwork = function (netId, callback) {
    doBooleanCommand("REMOVE_NETWORK " + netId, "OK", callback);
  };

  command.enableNetwork = function (netId, disableOthers, callback) {
    doBooleanCommand((disableOthers ? "SELECT_NETWORK " : "ENABLE_NETWORK ") +
                     netId, "OK", callback);
  };

  command.disableNetwork = function (netId, callback) {
    doBooleanCommand("DISABLE_NETWORK " + netId, "OK", callback);
  };

  command.status = function (callback) {
    doStringCommand("STATUS", callback);
  };

  command.ping = function (callback) {
    doBooleanCommand("PING", "PONG", callback);
  };

  command.scanResults = function (callback) {
    doStringCommand("SCAN_RESULTS", callback);
  };

  command.disconnect = function (callback) {
    doBooleanCommand("DISCONNECT", "OK", callback);
  };

  command.reconnect = function (callback) {
    doBooleanCommand("RECONNECT", "OK", callback);
  };

  command.reassociate = function (callback) {
    doBooleanCommand("REASSOCIATE", "OK", callback);
  };

  command.setBackgroundScan = function (enable, callback) {
    doBooleanCommand("SET pno " + (enable ? "1" : "0"),
                     "OK",
                     function(ok) {
                       callback(true, ok);
                     });
  };

  command.doSetScanMode = function (setActive, callback) {
    doBooleanCommand(setActive ?
                     "DRIVER SCAN-ACTIVE" :
                     "DRIVER SCAN-PASSIVE", "OK", callback);
  };

  command.scan = function (callback) {
    doBooleanCommand("SCAN", "OK", callback);
  };

  command.setLogLevel = function (level, callback) {
    doBooleanCommand("LOG_LEVEL " + level, "OK", callback);
  };

  command.getLogLevel = function (callback) {
    doStringCommand("LOG_LEVEL", callback);
  };

  command.wpsPbc = function (callback) {
    doBooleanCommand("WPS_PBC", "OK", callback);
  };

  command.wpsPin = function (detail, callback) {
    doStringCommand("WPS_PIN " +
                    (detail.bssid === undefined ? "any" : detail.bssid) +
                    (detail.pin === undefined ? "" : (" " + detail.pin)),
                    callback);
  };

  command.wpsCancel = function (callback) {
    doBooleanCommand("WPS_CANCEL", "OK", callback);
  };

  command.startDriver = function (callback) {
    doBooleanCommand("DRIVER START", "OK");
  };

  command.stopDriver = function (callback) {
    doBooleanCommand("DRIVER STOP", "OK");
  };

  command.startPacketFiltering = function (callback) {
    var commandChain = ["DRIVER RXFILTER-ADD 0",
                        "DRIVER RXFILTER-ADD 1",
                        "DRIVER RXFILTER-ADD 3",
                        "DRIVER RXFILTER-START"];

    doBooleanCommandChain(commandChain, callback);
  };

  command.stopPacketFiltering = function (callback) {
    var commandChain = ["DRIVER RXFILTER-STOP",
                        "DRIVER RXFILTER-REMOVE 3",
                        "DRIVER RXFILTER-REMOVE 1",
                        "DRIVER RXFILTER-REMOVE 0"];

    doBooleanCommandChain(commandChain, callback);
  };

  command.doGetRssi = function (cmd, callback) {
    doCommand(cmd, function(data) {
      var rssi = -200;

      if (!data.status) {
        // If we are associating, the reply is "OK".
        var reply = data.reply;
        if (reply !== "OK") {
          // Format is: <SSID> rssi XX". SSID can contain spaces.
          var offset = reply.lastIndexOf("rssi ");
          if (offset !== -1) {
            rssi = reply.substr(offset + 5) | 0;
          }
        }
      }
      callback(rssi);
    });
  };

  command.getRssi = function (callback) {
    command.doGetRssi("DRIVER RSSI", callback);
  };

  command.getRssiApprox = function (callback) {
    command.doGetRssi("DRIVER RSSI-APPROX", callback);
  };

  command.getLinkSpeed = function (callback) {
    doStringCommand("DRIVER LINKSPEED", function(reply) {
      if (reply) {
        reply = reply.split(" ")[1] | 0; // Format: LinkSpeed XX
      }
      callback(reply);
    });
  };

  command.getConnectionInfoICS = function (callback) {
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
  };

  command.getMacAddress = function (callback) {
    doStringCommand("DRIVER MACADDR", function(reply) {
      if (reply) {
        reply = reply.split(" ")[2]; // Format: Macaddr = XX.XX.XX.XX.XX.XX
      }
      callback(reply);
    });
  };

  command.setPowerModeICS = function (mode, callback) {
    doBooleanCommand("DRIVER POWERMODE " + (mode === "AUTO" ? 0 : 1), "OK", callback);
  };

  command.setPowerModeJB = function (mode, callback) {
    doBooleanCommand("SET ps " + (mode === "AUTO" ? 1 : 0), "OK", callback);
  };

  command.getPowerMode = function (callback) {
    doStringCommand("DRIVER GETPOWER", function(reply) {
      if (reply) {
        reply = (reply.split()[2]|0); // Format: powermode = XX
      }
      callback(reply);
    });
  };

  command.setNumAllowedChannels = function (numChannels, callback) {
    doBooleanCommand("DRIVER SCAN-CHANNELS " + numChannels, "OK", callback);
  };

  command.getNumAllowedChannels = function (callback) {
    doStringCommand("DRIVER SCAN-CHANNELS", function(reply) {
      if (reply) {
        reply = (reply.split()[2]|0); // Format: Scan-Channels = X
      }
      callback(reply);
    });
  };

  command.setBluetoothCoexistenceMode = function (mode, callback) {
    doBooleanCommand("DRIVER BTCOEXMODE " + mode, "OK", callback);
  };

  command.setBluetoothCoexistenceScanMode = function (mode, callback) {
    doBooleanCommand("DRIVER BTCOEXSCAN-" + (mode ? "START" : "STOP"),
                     "OK", callback);
  };

  command.saveConfig = function (callback) {
    // Make sure we never write out a value for AP_SCAN other than 1.
    doBooleanCommand("AP_SCAN 1", "OK", function(ok) {
      doBooleanCommand("SAVE_CONFIG", "OK", callback);
    });
  };

  command.reloadConfig = function (callback) {
    doBooleanCommand("RECONFIGURE", "OK", callback);
  };

  command.setScanResultHandling = function (mode, callback) {
    doBooleanCommand("AP_SCAN " + mode, "OK", callback);
  };

  command.addToBlacklist = function (bssid, callback) {
    doBooleanCommand("BLACKLIST " + bssid, "OK", callback);
  };

  command.clearBlacklist = function (callback) {
    doBooleanCommand("BLACKLIST clear", "OK", callback);
  };

  command.setSuspendOptimizationsICS = function (enabled, callback) {
    doBooleanCommand("DRIVER SETSUSPENDOPT " + (enabled ? 0 : 1),
                     "OK", callback);
  };

  command.setSuspendOptimizationsJB = function (enabled, callback) {
    doBooleanCommand("DRIVER SETSUSPENDMODE " + (enabled ? 1 : 0),
                     "OK", callback);
  };

  command.connectToSupplicant = function(callback) {
    voidControlMessage("connect_to_supplicant", callback);
  };

  command.closeSupplicantConnection = function(callback) {
    voidControlMessage("close_supplicant_connection", callback);
  };

  command.getMacAddress = function(callback) {
    doStringCommand("DRIVER MACADDR", function(reply) {
      if (reply) {
        reply = reply.split(" ")[2]; // Format: Macaddr = XX.XX.XX.XX.XX.XX
      }
      callback(reply);
    });
  };

  //--------------------------------------------------
  // Helper functions.
  //--------------------------------------------------

  function voidControlMessage(cmd, callback) {
    aControlMessage({ cmd: cmd, iface: aInterface }, function (data) {
      callback(data.status);
    });
  }

  function doCommand(request, callback) {
    var msg = { cmd:     "command",
                request: request,
                iface:   aInterface };

    aControlMessage(msg, callback);
  }

  function doIntCommand(request, callback) {
    doCommand(request, function(data) {
      callback(data.status ? -1 : (data.reply|0));
    });
  }

  function doBooleanCommand(request, expected, callback) {
    doCommand(request, function(data) {
      callback(data.status ? false : (data.reply === expected));
    });
  }

  function doStringCommand(request, callback) {
    doCommand(request, function(data) {
      callback(data.status ? null : data.reply);
    });
  }

  function doBooleanCommandChain(commandChain, callback, i) {
    if (undefined === i) {
      i = 0;
    }

    doBooleanCommand(commandChain[i], "OK", function(ok) {
      if (!ok) {
        return callback(false);
      }
      i++;
      if (i === commandChain.length || !commandChain[i]) {
        // Reach the end or empty command.
        return callback(true);
      }
      doBooleanCommandChain(commandChain, callback, i);
    });
  }

  function stopProcess(service, process, callback) {
    var count = 0;
    var timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    function tick() {
      let result = libcutils.property_get(service);
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
    }

    setProperty("ctl.stop", process, tick);
  }

  // Wrapper around libcutils.property_set that returns true if setting the
  // value was successful.
  // Note that the callback is not called asynchronously.
  function setProperty(key, value, callback) {
    let ok = true;
    try {
      libcutils.property_set(key, value);
    } catch(e) {
      ok = false;
    }
    callback(ok);
  }

  return command;
};
