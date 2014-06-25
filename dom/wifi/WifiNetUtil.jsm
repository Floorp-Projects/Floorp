/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/systemlibs.js");

XPCOMUtils.defineLazyServiceGetter(this, "gNetworkService",
                                   "@mozilla.org/network/service;1",
                                   "nsINetworkService");

this.EXPORTED_SYMBOLS = ["WifiNetUtil"];

const DHCP_PROP = "init.svc.dhcpcd";
const DHCP      = "dhcpcd";
const DEBUG     = false;

this.WifiNetUtil = function(controlMessage) {
  function debug(msg) {
    if (DEBUG) {
      dump('-------------- NetUtil: ' + msg);
    }
  }

  var util = {};

  util.configureInterface = function(cfg, callback) {
    let message = { cmd:     "ifc_configure",
                    ifname:  cfg.ifname,
                    ipaddr:  cfg.ipaddr,
                    mask:    cfg.mask,
                    gateway: cfg.gateway,
                    dns1:    cfg.dns1,
                    dns2:    cfg.dns2 };

    controlMessage(message, function(data) {
      callback(!data.status);
    });
  };

  util.runDhcp = function (ifname, callback) {
    controlMessage({ cmd: "dhcp_do_request", ifname: ifname }, function(data) {
      var dhcpInfo = data.status ? null : data;
      util.runIpConfig(ifname, dhcpInfo, callback);
    });
  };

  util.stopDhcp = function (ifname, callback) {
    // This function does exactly what dhcp_stop does. Unforunately, if we call
    // this function twice before the previous callback is returned. We may block
    // our self waiting for the callback. It slows down the wifi startup procedure.
    // Therefore, we have to roll our own version here.
    let dhcpService = DHCP_PROP + "_" + ifname;
    let suffix = (ifname.substr(0, 3) === "p2p") ? "p2p" : ifname;
    let processName = DHCP + "_" + suffix;
    stopProcess(dhcpService, processName, callback);
  };

  util.enableInterface = function (ifname, callback) {
    controlMessage({ cmd: "ifc_enable", ifname: ifname }, function (data) {
      callback(!data.status);
    });
  };

  util.disableInterface = function (ifname, callback) {
    controlMessage({ cmd: "ifc_disable", ifname: ifname }, function (data) {
      callback(!data.status);
    });
  };

  util.startDhcpServer = function (config, callback) {
    gNetworkService.setDhcpServer(true, config, function (error) {
      callback(!error);
    });
  };

  util.stopDhcpServer = function (callback) {
    gNetworkService.setDhcpServer(false, null, function (error) {
      callback(!error);
    });
  };

  util.addHostRoute = function (ifname, route, callback) {
    controlMessage({ cmd: "ifc_add_host_route", ifname: ifname, route: route }, function(data) {
      callback(!data.status);
    });
  };

  util.removeHostRoutes = function (ifname, callback) {
    controlMessage({ cmd: "ifc_remove_host_routes", ifname: ifname }, function(data) {
      callback(!data.status);
    });
  };

  util.setDefaultRoute = function (ifname, route, callback) {
    controlMessage({ cmd: "ifc_set_default_route", ifname: ifname, route: route }, function(data) {
      callback(!data.status);
    });
  };

  util.getDefaultRoute = function (ifname, callback) {
    controlMessage({ cmd: "ifc_get_default_route", ifname: ifname }, function(data) {
      callback(!data.route);
    });
  };

  util.removeDefaultRoute = function (ifname, callback) {
    controlMessage({ cmd: "ifc_remove_default_route", ifname: ifname }, function(data) {
      callback(!data.status);
    });
  };

  util.resetConnections = function (ifname, callback) {
    controlMessage({ cmd: "ifc_reset_connections", ifname: ifname }, function(data) {
      callback(!data.status);
    });
  };

  util.releaseDhcpLease = function (ifname, callback) {
    controlMessage({ cmd: "dhcp_release_lease", ifname: ifname }, function(data) {
      callback(!data.status);
    });
  };

  util.getDhcpError = function (callback) {
    controlMessage({ cmd: "dhcp_get_errmsg" }, function(data) {
      callback(data.error);
    });
  };

  util.runDhcpRenew = function (ifname, callback) {
    controlMessage({ cmd: "dhcp_do_request", ifname: ifname }, function(data) {
      callback(data.status ? null : data);
    });
  };

  util.runIpConfig = function (name, data, callback) {
    if (!data) {
      debug("IP config failed to run");
      callback({ info: data });
      return;
    }

    setProperty("net." + name + ".dns1", ipToString(data.dns1),
                function(ok) {
      if (!ok) {
        debug("Unable to set net.<ifname>.dns1");
        return;
      }
      setProperty("net." + name + ".dns2", ipToString(data.dns2),
                  function(ok) {
        if (!ok) {
          debug("Unable to set net.<ifname>.dns2");
          return;
        }
        setProperty("net." + name + ".gw", ipToString(data.gateway),
                    function(ok) {
          if (!ok) {
            debug("Unable to set net.<ifname>.gw");
            return;
          }
          callback({ info: data });
        });
      });
    });
  };

  //--------------------------------------------------
  // Helper functions.
  //--------------------------------------------------

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

  function ipToString(n) {
    return String((n >>  0) & 0xFF) + "." +
                 ((n >>  8) & 0xFF) + "." +
                 ((n >> 16) & 0xFF) + "." +
                 ((n >> 24) & 0xFF);
  }

  return util;
};
