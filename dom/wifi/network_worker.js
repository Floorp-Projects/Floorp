/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */

"use strict";

importScripts("libhardware_legacy.js", "libnetutils.js", "libcutils.js");

var cbuf = ctypes.char.array(4096)();
var hwaddr = ctypes.uint8_t.array(6)();
var len = ctypes.size_t();
var ints = ctypes.int.array(8)();

self.onmessage = function(e) {
  var data = e.data;
  var id = data.id;
  var cmd = data.cmd;

  switch (cmd) {
  case "command":
    len.value = 4096;
    var ret = libhardware_legacy.command(data.request, cbuf, len.address());
    var reply = "";
    if (!ret) {
      var reply_len = len.value;
      var str = cbuf.readString();
      if (str[reply_len-1] == "\n")
        --reply_len;
      reply = str.substr(0, reply_len);
    }
    postMessage({ id: id, status: ret, reply: reply });
    break;
  case "wait_for_event":
    var ret = libhardware_legacy.wait_for_event(cbuf, 4096);
    var event = cbuf.readString().substr(0, ret.value);
    postMessage({ id: id, event: event });
    break;
  case "ifc_enable":
  case "ifc_disable":
  case "ifc_remove_host_routes":
  case "ifc_remove_default_route":
  case "ifc_reset_connections":
  case "dhcp_stop":
  case "dhcp_release_lease":
    var ret = libnetutils[cmd](data.ifname);
    postMessage({ id: id, status: ret });
    break;
  case "ifc_get_default_route":
    var route = libnetutils.ifc_get_default_route(data.ifname);
    postMessage({ id: id, route: route });
    break;
  case "ifc_add_host_route":
  case "ifc_set_default_route":
    var ret = libnetutils[cmd](data.ifname, data.route);
    postMessage({ id: id, status: ret });
    break;
  case "ifc_configure":
    dump("WIFI: data: " + uneval(data) + "\n");
    var ret = libnetutils.ifc_configure(data.ifname, data.ipaddr, data.mask, data.gateway, data.dns1, data.dns2);
    postMessage({ id: id, status: ret });
    break;
  case "dhcp_get_errmsg":
    var error = libnetutils.get_dhcp_get_errmsg();
    postMessage({ id: id, error: error.readString() });
    break;
  case "dhcp_do_request":
  case "dhcp_do_request_renew":
    var ret = libnetutils[cmd](data.ifname,
                               ints.addressOfElement(0),
                               ints.addressOfElement(1),
                               ints.addressOfElement(2),
                               ints.addressOfElement(3),
                               ints.addressOfElement(4),
                               ints.addressOfElement(5),
                               ints.addressOfElement(6));
    postMessage({ id: id, status: ret, ipaddr: ints[0], gateway: ints[1], mask: ints[2],
                  dns1: ints[3], dns2: ints[4], server: ints[5], lease: ints[6]});
    break;
  case "property_get":
    var ret = libcutils.property_get(data.key, cbuf, data.defaultValue);
    postMessage({ id: id, status: ret, value: cbuf.readString() });
    break;
  case "property_set":
    var ret = libcutils.property_set(data.key, data.value);
    postMessage({ id: id, status: ret });
    break;
  default:
    var f = libhardware_legacy[cmd] || libnetutils[cmd];
    var ret = f();
    dump("WIFI: " + cmd + " returned: " + ret);
    postMessage({ id: id, status: ret });
    break;
  }
}
