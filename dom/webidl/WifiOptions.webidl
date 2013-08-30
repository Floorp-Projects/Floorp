/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
  * This dictionnary holds the parameters sent to the wifi service.
  */
dictionary WifiCommandOptions
{
  long      id = 0;       // opaque id.
  DOMString cmd = "";     // the command name.
  DOMString request;      // for "command"
  DOMString ifname;       // for "ifc_reset_connections", "ifc_enable",
                          // "ifc_disable", "ifc_remove_host_routes",
                          // "ifc_remove_default_route", "dhcp_stop",
                          // "dhcp_release_lease", "ifc_get_default_route",
                          // "ifc_add_host_route", "ifc_set_default_route",
                          // "ifc_configure", "dhcp_do_request",
                          // "dhcp_do_request_renew".
  long route;             // for "ifc_add_host_route", "ifc_set_default_route".
  long ipaddr;            // for "ifc_configure".
  long mask;              // for "ifc_configure".
  long gateway;           // for "ifc_configure".
  long dns1;              // for "ifc_configure".
  long dns2;              // for "ifc_configure".
  DOMString key;          // for "property_get", "property_set".
  DOMString value;        // for "property_set".
  DOMString defaultValue; // for "property_get".
};

/**
  * This dictionnary holds the parameters sent back to WifiWorker.js
  */
dictionary WifiResultOptions
{
  long      id = 0;             // opaque id.
  long      status = 0;         // the return status of the command.
                                // Used by most commands.
  DOMString reply = "";         // for "command".
  DOMString route = "";         // for "ifc_get_default_route".
  DOMString error = "";         // for "dhcp_get_errmsg".
  DOMString value = "";         // for "property_get".
  DOMString ipaddr_str = "";    // The following are for the result of
                                // dhcp_do_request.
  DOMString gateway_str = "";
  DOMString broadcast_str = "";
  DOMString dns1_str = "";
  DOMString dns2_str = "";
  DOMString mask_str = "";
  DOMString server_str = "";
  DOMString vendor_str = "";
  long      lease = 0;
  long      mask = 0;
  long      ipaddr = 0;
  long      gateway = 0;
  long      dns1 = 0;
  long      dns2 = 0;
  long      server = 0;
};
