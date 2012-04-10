/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */

"use strict";

let libhardware_legacy = (function () {
  let library = ctypes.open("libhardware_legacy.so");

  return {
    // Load wifi driver, 0 on success, < 0 on failure.
    load_driver: library.declare("wifi_load_driver", ctypes.default_abi, ctypes.int),

    // Unload wifi driver, 0 on success, < 0 on failure.
    unload_driver: library.declare("wifi_unload_driver", ctypes.default_abi, ctypes.int),

    // Start supplicant, 0 on success, < 0 on failure.
    start_supplicant: library.declare("wifi_start_supplicant", ctypes.default_abi, ctypes.int),

    // Stop supplicant, 0 on success, < 0 on failure.
    stop_supplicant: library.declare("wifi_stop_supplicant", ctypes.default_abi, ctypes.int),

    // Open a connection to the supplicant, 0 on success, < 0 on failure.
    connect_to_supplicant: library.declare("wifi_connect_to_supplicant", ctypes.default_abi, ctypes.int),

    // Close connection to connection to the supplicant, 0 on success, < 0 on failure.
    close_supplicant_connection: library.declare("wifi_close_supplicant_connection", ctypes.default_abi, ctypes.void_t),

    // Block until a wifi event is returned, buf is the buffer, len is the max length of the buffer.
    // Return value is number of bytes in buffer, or 0 if no event (no connection for instance), and < 0 on failure.
    wait_for_event: library.declare("wifi_wait_for_event", ctypes.default_abi, ctypes.int, ctypes.char.ptr, ctypes.size_t),

    // Issue a command to the wifi driver. command is the command string, reply will hold the reply, reply_len contains
    // the maximum reply length initially and is updated with the actual length. 0 is returned on success, < 0 on failure.
    command: library.declare("wifi_command", ctypes.default_abi, ctypes.int, ctypes.char.ptr, ctypes.char.ptr, ctypes.size_t.ptr),
  };
})();
