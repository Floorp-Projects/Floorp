/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */

"use strict";

let libnetutils = (function () {
  let library = ctypes.open("libnetutils.so");
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

  let sdkVersion = parseInt(property_get("ro.build.version.sdk"));

  let iface = {
    ifc_enable: library.declare("ifc_enable", ctypes.default_abi, ctypes.int, ctypes.char.ptr),
    ifc_disable: library.declare("ifc_disable", ctypes.default_abi, ctypes.int, ctypes.char.ptr),
    ifc_add_host_route: library.declare("ifc_add_host_route", ctypes.default_abi, ctypes.int, ctypes.char.ptr, ctypes.int),
    ifc_remove_host_routes: library.declare("ifc_remove_host_routes", ctypes.default_abi, ctypes.int, ctypes.char.ptr),
    ifc_set_default_route: library.declare("ifc_set_default_route", ctypes.default_abi, ctypes.int, ctypes.char.ptr, ctypes.int),
    ifc_get_default_route: library.declare("ifc_get_default_route", ctypes.default_abi, ctypes.int, ctypes.char.ptr),
    ifc_remove_default_route: library.declare("ifc_remove_default_route", ctypes.default_abi, ctypes.int, ctypes.char.ptr),
    ifc_reset_connections: library.declare("ifc_reset_connections", ctypes.default_abi, ctypes.int, ctypes.char.ptr),
    ifc_configure: library.declare("ifc_configure", ctypes.default_abi, ctypes.int, ctypes.char.ptr,
                                   ctypes.int, ctypes.int, ctypes.int, ctypes.int, ctypes.int),
    dhcp_stop: library.declare("dhcp_stop", ctypes.default_abi, ctypes.int, ctypes.char.ptr),
    dhcp_release_lease: library.declare("dhcp_release_lease", ctypes.default_abi, ctypes.int, ctypes.char.ptr),
    dhcp_get_errmsg: library.declare("dhcp_get_errmsg", ctypes.default_abi, ctypes.char.ptr),
  };

  if (sdkVersion >= 15) {
    let ipaddrbuf = ctypes.char.array(4096)();
    let gatewaybuf = ctypes.char.array(4096)();
    let prefixLen = ctypes.int();
    let dns1buf = ctypes.char.array(4096)();
    let dns2buf = ctypes.char.array(4096)();
    let serverbuf = ctypes.char.array(4096)();
    let lease = ctypes.int();
    let c_dhcp_do_request =
      library.declare("dhcp_do_request", ctypes.default_abi,
                      ctypes.int,      // [return]
                      ctypes.char.ptr, // ifname
                      ctypes.char.ptr, // ipaddr
                      ctypes.char.ptr, // gateway
                      ctypes.int.ptr,  // prefixlen
                      ctypes.char.ptr, // dns1
                      ctypes.char.ptr, // dns2
                      ctypes.char.ptr, // server
                      ctypes.int.ptr); // lease

    let stringToIp = function (s) {
      if (!s) return 0;
      let comps = s.split(".");
      return ((parseInt(comps[0]) & 0xff) <<  0 |
              (parseInt(comps[1]) & 0xff) <<  8 |
              (parseInt(comps[2]) & 0xff) << 16 |
              (parseInt(comps[3]) & 0xff) << 24);
    }

    let makeMask = function (len) {
      let mask = 0;
      for (let i = 0; i < len; ++i)
        mask |= (1 << i);
      return mask;
    }

    iface.dhcp_do_request = function (ifname) {
      let ret = c_dhcp_do_request(ifname,
                                  ipaddrbuf,
                                  gatewaybuf,
                                  prefixLen.address(),
                                  dns1buf,
                                  dns2buf,
                                  serverbuf,
                                  lease.address());
      return {
          ret: ret |0,
          ipaddr: stringToIp(ipaddrbuf.readString()),
          gateway: stringToIp(gatewaybuf.readString()),
          mask: makeMask(prefixLen),
          dns1: stringToIp(dns1buf.readString()),
          dns2: stringToIp(dns2buf.readString()),
          server: stringToIp(serverbuf.readString()),
          lease: lease |0
      };
    };
    // dhcp_do_request_renew() went away in newer libnetutils.
    iface.dhcp_do_request_renew = iface.dhcp_do_request;
  } else {
    let ints = ctypes.int.array(8)();
    let c_dhcp_do_request =
      library.declare("dhcp_do_request", ctypes.default_abi,
                      ctypes.int,      // [return]
                      ctypes.char.ptr, // ifname
                      ctypes.int.ptr,  // ipaddr
                      ctypes.int.ptr,  // gateway
                      ctypes.int.ptr,  // mask
                      ctypes.int.ptr,  // dns1
                      ctypes.int.ptr,  // dns2
                      ctypes.int.ptr,  // server
                      ctypes.int.ptr); // lease
    let c_dhcp_do_request_renew =
      library.declare("dhcp_do_request_renew", ctypes.default_abi,
                      ctypes.int, // [return]
                      ctypes.char.ptr, // ifname
                      ctypes.int.ptr,  // ipaddr
                      ctypes.int.ptr,  // gateway
                      ctypes.int.ptr,  // mask
                      ctypes.int.ptr,  // dns1
                      ctypes.int.ptr,  // dns2
                      ctypes.int.ptr,  // server
                      ctypes.int.ptr); // lease

    let makeRequestWrapper = function (c_fn) {
      return function (ifname) {
        let ret = c_fn(ifname,
                       ints.addressOfElement(0),
                       ints.addressOfElement(1),
                       ints.addressOfElement(2),
                       ints.addressOfElement(3),
                       ints.addressOfElement(4),
                       ints.addressOfElement(5),
                       ints.addressOfElement(6));
        return { ret: ret |0, ipaddr: ints[0] |0, gateway: ints[1] |0,
                 mask: ints[2] |0, dns1: ints[3] |0, dns2: ints[4] |0,
                 server: ints[5] |0, lease: ints[6] |0 };
      };
    }

    iface.dhcp_do_request = makeRequestWrapper(c_dhcp_do_request);
    iface.dhcp_do_request_renew = makeRequestWrapper(c_dhcp_do_request_renew);
  }

  return iface;
})();
