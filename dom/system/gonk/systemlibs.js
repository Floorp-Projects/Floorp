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

if (!this.ctypes) {
  // We're likely being loaded as a JSM.
  this.EXPORTED_SYMBOLS = [ "libcutils", "libnetutils", "netHelpers" ];
  Components.utils.import("resource://gre/modules/ctypes.jsm");
}

const SYSTEM_PROPERTY_KEY_MAX = 32;
const SYSTEM_PROPERTY_VALUE_MAX = 92;

// We leave this as 'undefined' instead of setting it to 'false'. That
// way a file that includes us can have it defined already without us
// overriding the value here.
let DEBUG;

/**
 * Expose some system-level functions.
 */
this.libcutils = (function() {
  let lib;
  try {
    lib = ctypes.open("libcutils.so");
  } catch(ex) {
    // Return a fallback option in case libcutils.so isn't present (e.g.
    // when building Firefox with MOZ_B2G_RIL.
    if (DEBUG) {
      dump("Could not load libcutils.so. Using fake propdb.\n");
    }
    let fake_propdb = Object.create(null);
    return {
      property_get: function(key, defaultValue) {
        if (key in fake_propdb) {
          return fake_propdb[key];
        }
        return defaultValue === undefined ? null : defaultValue;
      },
      property_set: function(key, value) {
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
    property_get: function(key, defaultValue) {
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
    property_set: function(key, value) {
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
this.libnetutils = (function() {
  let library;
  try {
    library = ctypes.open("libnetutils.so");
  } catch(ex) {
    if (DEBUG) {
      dump("Could not load libnetutils.so!\n");
    }
    // For now we just fake the ctypes library interfacer to return
    // no-op functions when library.declare() is called.
    library = {
      declare: function() {
        return function fake_libnetutils_function() {};
      }
    };
  }

  let iface = {
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
    ifc_configure: library.declare("ifc_configure", ctypes.default_abi,
                                   ctypes.int,
                                   ctypes.char.ptr,
                                   ctypes.int,
                                   ctypes.int,
                                   ctypes.int,
                                   ctypes.int,
                                   ctypes.int),
    ifc_add_route: library.declare("ifc_add_route", ctypes.default_abi,
                                   ctypes.int, // return value
                                   ctypes.char.ptr, // ifname
                                   ctypes.char.ptr, // dst
                                   ctypes.int, // prefix_length
                                   ctypes.char.ptr), // gw
    ifc_remove_route: library.declare("ifc_remove_route", ctypes.default_abi,
                                      ctypes.int, // return value
                                      ctypes.char.ptr, // ifname
                                      ctypes.char.ptr, // dst
                                      ctypes.int, // prefix_length
                                      ctypes.char.ptr), // gw
    dhcp_stop: library.declare("dhcp_stop", ctypes.default_abi,
                               ctypes.int,
                               ctypes.char.ptr),
    dhcp_release_lease: library.declare("dhcp_release_lease", ctypes.default_abi,
                                        ctypes.int,
                                        ctypes.char.ptr),
    dhcp_get_errmsg: library.declare("dhcp_get_errmsg", ctypes.default_abi,
                                     ctypes.char.ptr),

    // Constants for ifc_reset_connections.
    // NOTE: Ignored in versions before ICS.
    RESET_IPV4_ADDRESSES: 0x01,
    RESET_IPV6_ADDRESSES: 0x02,
  };

  iface.RESET_ALL_ADDRESSES = iface.RESET_IPV4_ADDRESSES |
                              iface.RESET_IPV6_ADDRESSES;

  return iface;
})();

/**
 * Helpers for conversions.
 */
this.netHelpers = {

  /**
   * Swap byte orders for 32-bit value
   */
  swap32: function(n) {
    return (((n >> 24) & 0xFF) <<  0) |
           (((n >> 16) & 0xFF) <<  8) |
           (((n >>  8) & 0xFF) << 16) |
           (((n >>  0) & 0xFF) << 24);
  },

  /**
   * Convert network byte order to host byte order
   * Note: Assume that the system is little endian
   */
  ntohl: function(n) {
    return this.swap32(n);
  },

  /**
   * Convert host byte order to network byte order
   * Note: Assume that the system is little endian
   */
  htonl: function(n) {
    return this.swap32(n);
  },

  /**
   * Convert integer representation of an IP address to the string
   * representation.
   *
   * @param ip
   *        IP address in number format.
   */
  ipToString: function(ip) {
    return ((ip >>  0) & 0xFF) + "." +
           ((ip >>  8) & 0xFF) + "." +
           ((ip >> 16) & 0xFF) + "." +
           ((ip >> 24) & 0xFF);
  },

  /**
   * Convert string representation of an IP address to the integer
   * representation (network byte order).
   *
   * @param string
   *        String containing the IP address.
   */
  stringToIP: function(string) {
    if (!string) {
      return null;
    }
    let ip = 0;
    let start, end = -1;
    for (let i = 0; i < 4; i++) {
      start = end + 1;
      end = string.indexOf(".", start);
      if (end == -1) {
        end = string.length;
      }
      let num = parseInt(string.slice(start, end), 10);
      if (isNaN(num)) {
        return null;
      }
      ip |= num << (i * 8);
    }
    return ip;
  },

  /**
   * Make a subnet mask.
   */
  makeMask: function(len) {
    let mask = 0;
    for (let i = 0; i < len; ++i) {
      mask |= (0x80000000 >> i);
    }
    return this.ntohl(mask);
  },

  /**
   * Get Mask length from given mask address
   */
  getMaskLength: function(mask) {
    let len = 0;
    let netmask = this.ntohl(mask);
    while (netmask & 0x80000000) {
        len++;
        netmask = netmask << 1;
    }
    return len;
  }
};
