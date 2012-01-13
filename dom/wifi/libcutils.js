/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */

"use strict";

let libcutils = (function () {
  let library = ctypes.open("libcutils.so");

  return {
    property_get: library.declare("property_get", ctypes.default_abi, ctypes.int, ctypes.char.ptr, ctypes.char.ptr, ctypes.char.ptr),
    property_set: library.declare("property_set", ctypes.default_abi, ctypes.int, ctypes.char.ptr, ctypes.char.ptr)
  };
})();
