/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

"use strict";

const DEBUG = true;

importScripts("systemlibs.js");

/**
 * Get network interface properties from the system property table.
 *
 * @param ifname
 *        Name of the network interface.
 */
function getIFProperties(ifname) {
  let gateway_str = libcutils.property_get("net." + ifname + ".gw");
  return {
    ifname:      ifname,
    gateway:     netHelpers.stringToIP(gateway_str),
    gateway_str: gateway_str,
    dns1_str:    libcutils.property_get("net." + ifname + ".dns1"),
    dns2_str:    libcutils.property_get("net." + ifname + ".dns2"),
  };
}

/**
 * Routines accessible to the main thread.
 */

/**
 * Dispatch a message from the main thread to a function.
 */
self.onmessage = function onmessage(event) {
  let message = event.data;
  if (DEBUG) debug("received message: " + JSON.stringify(message));
  let ret = self[message.cmd](message);
  postMessage({id: message.id, ret: ret});
};

/**
 * Set default route and DNS servers for given network interface.
 */
function setDefaultRouteAndDNS(options) {
  if (options.oldIfname) {
    libnetutils.ifc_remove_default_route(options.oldIfname);
  }

  if (!options.gateway || !options.dns1_str) {
    options = getIFProperties(options.ifname);
  }

  libnetutils.ifc_set_default_route(options.ifname, options.gateway);
  libcutils.property_set("net.dns1", options.dns1_str);
  libcutils.property_set("net.dns2", options.dns2_str);

  // Bump the DNS change property.
  let dnschange = libcutils.property_get("net.dnschange", "0");
  libcutils.property_set("net.dnschange", (parseInt(dnschange, 10) + 1).toString());
}

/**
 * Run DHCP and set default route and DNS servers for a given
 * network interface.
 */
function runDHCPAndSetDefaultRouteAndDNS(options) {
  let dhcp = libnetutils.dhcp_do_request(options.ifname);
  dhcp.ifname = options.ifname;
  dhcp.oldIfname = options.oldIfname;

  //TODO this could be race-y... by the time we've finished the DHCP request
  // and are now fudging with the routes, another network interface may have
  // come online that's preferred...
  setDefaultRouteAndDNS(dhcp);
}

if (!this.debug) {
  this.debug = function debug(message) {
    dump("Network Worker: " + message + "\n");
  };
}
