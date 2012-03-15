/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const SYSTEM_PROPERTY_KEY_MAX = 32;
const SYSTEM_PROPERTY_VALUE_MAX = 92;

/**
 * Expose some system-level functions.
 */
let libcutils = (function() {
  let lib;
  try {
    lib = ctypes.open("libcutils.so");
  } catch(ex) {
    // Return a fallback option in case libcutils.so isn't present (e.g.
    // when building Firefox with MOZ_B2G_RIL.
    dump("Could not load libcutils.so. Using fake propdb.\n");
    let fake_propdb = Object.create(null);
    return {
      property_get: function fake_property_get(key, defaultValue) {
        if (key in fake_propdb) {
          return fake_propdb[key];
        }
        return defaultValue === undefined ? null : defaultValue;
      },
      property_set: function fake_property_set(key, value) {
        fake_propdb[key] = value;
      }
    };
  }

  let c_property_get = lib.declare("property_get", ctypes.default_abi,
                                   ctypes.int,       // return value: length
                                   ctypes.char.ptr,  // key
                                   ctypes.char.ptr,  // value
                                   ctypes.char.ptr); // default
  let c_property_set = lib.declare("property_set", ctypes.default_abi,
                                   ctypes.int,       // return value: success
                                   ctypes.char.ptr,  // key
                                   ctypes.char.ptr); // value
  let c_value_buf = ctypes.char.array(SYSTEM_PROPERTY_VALUE_MAX)();

  return {

    /**
     * Get a system property.
     *
     * @param key
     *        Name of the property
     * @param defaultValue [optional]
     *        Default value to return if the property isn't set (default: null)
     */
    property_get: function property_get(key, defaultValue) {
      if (defaultValue === undefined) {
        defaultValue = null;
      }
      c_property_get(key, c_value_buf, defaultValue);
      return c_value_buf.readString();
    },

    /**
     * Set a system property
     *
     * @param key
     *        Name of the property
     * @param value
     *        Value to set the property to.
     */
    property_set: function property_set(key, value) {
      let rv = c_property_set(key, value);
      if (rv) {
        throw Error('libcutils.property_set("' + key + '", "' + value +
                    '") failed with error ' + rv);
      }
    }

  };
})();


/**
 * Network-related functions from libnetutils.
 */
let libnetutils = (function () {
  let library;
  try {
    library = ctypes.open("libnetutils.so");
  } catch(ex) {
    dump("Could not load libnetutils.so!\n");
    // For now we just fake the ctypes library interfacer to return
    // no-op functions when library.declare() is called.
    library = {
      declare: function fake_declare() {
        return function fake_libnetutils_function() {};
      }
    };
  }

  return {
    ifc_enable: library.declare("ifc_enable", ctypes.default_abi,
                                ctypes.int,
                                ctypes.char.ptr),
    ifc_disable: library.declare("ifc_disable", ctypes.default_abi,
                                 ctypes.int,
                                 ctypes.char.ptr),
    ifc_add_host_route: library.declare("ifc_add_host_route",
                                        ctypes.default_abi,
                                        ctypes.int,
                                        ctypes.char.ptr,
                                        ctypes.int),
    ifc_remove_host_routes: library.declare("ifc_remove_host_routes",
                                            ctypes.default_abi,
                                            ctypes.int,
                                            ctypes.char.ptr),
    ifc_set_default_route: library.declare("ifc_set_default_route",
                                           ctypes.default_abi,
                                           ctypes.int,
                                           ctypes.char.ptr,
                                           ctypes.int),
    ifc_get_default_route: library.declare("ifc_get_default_route",
                                           ctypes.default_abi,
                                           ctypes.int,
                                           ctypes.char.ptr),
    ifc_remove_default_route: library.declare("ifc_remove_default_route",
                                              ctypes.default_abi,
                                              ctypes.int,
                                              ctypes.char.ptr),
    ifc_reset_connections: library.declare("ifc_reset_connections",
                                           ctypes.default_abi,
                                           ctypes.int,
                                           ctypes.char.ptr),
    ifc_configure: library.declare("ifc_configure", ctypes.default_abi,
                                   ctypes.int,
                                   ctypes.char.ptr,
                                   ctypes.int,
                                   ctypes.int,
                                   ctypes.int,
                                   ctypes.int,
                                   ctypes.int),
    dhcp_do_request: library.declare("dhcp_do_request", ctypes.default_abi,
                                     ctypes.int,
                                     ctypes.char.ptr,
                                     ctypes.int.ptr,
                                     ctypes.int.ptr,
                                     ctypes.int.ptr,
                                     ctypes.int.ptr,
                                     ctypes.int.ptr,
                                     ctypes.int.ptr,
                                     ctypes.int.ptr),
    dhcp_stop: library.declare("dhcp_stop", ctypes.default_abi,
                               ctypes.int,
                               ctypes.char.ptr),
    dhcp_release_lease: library.declare("dhcp_release_lease", ctypes.default_abi,
                                        ctypes.int,
                                        ctypes.char.ptr),
    dhcp_get_errmsg: library.declare("dhcp_get_errmsg", ctypes.default_abi,
                                     ctypes.char.ptr),
    dhcp_do_request_renew: library.declare("dhcp_do_request_renew",
                                           ctypes.default_abi,
                                           ctypes.int,
                                           ctypes.char.ptr,
                                           ctypes.int.ptr,
                                           ctypes.int.ptr,
                                           ctypes.int.ptr,
                                           ctypes.int.ptr,
                                           ctypes.int.ptr,
                                           ctypes.int.ptr,
                                           ctypes.int.ptr)
  };
})();
