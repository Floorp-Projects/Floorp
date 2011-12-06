/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */

"use strict";

let libnetutils = (function () {
  let library = ctypes.open("/system/lib/libnetutils.so");

  return {
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
    dhcp_do_request: library.declare("dhcp_do_request", ctypes.default_abi, ctypes.int,
                                     ctypes.char.ptr, ctypes.int.ptr, ctypes.int.ptr, ctypes.int.ptr,
                                     ctypes.int.ptr, ctypes.int.ptr, ctypes.int.ptr, ctypes.int.ptr),
    dhcp_stop: library.declare("dhcp_stop", ctypes.default_abi, ctypes.int, ctypes.char.ptr),
    dhcp_release_lease: library.declare("dhcp_release_lease", ctypes.default_abi, ctypes.int, ctypes.char.ptr),
    dhcp_get_errmsg: library.declare("dhcp_get_errmsg", ctypes.default_abi, ctypes.char.ptr),
    dhcp_do_request_renew: library.declare("dhcp_do_request_renew", ctypes.default_abi, ctypes.int,
                                           ctypes.char.ptr, ctypes.int.ptr, ctypes.int.ptr, ctypes.int.ptr,
                                           ctypes.int.ptr, ctypes.int.ptr, ctypes.int.ptr, ctypes.int.ptr)
  };
})();
