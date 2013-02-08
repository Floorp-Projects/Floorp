/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/ctypes.jsm");

const RECOVERYSERVICE_CID = Components.ID("{b3caca5d-0bb0-48c6-912b-6be6cbf08832}");
const RECOVERYSERVICE_CONTRACTID = "@mozilla.org/recovery-service;1";

function log(msg) {
  dump("-*- RecoveryService: " + msg + "\n");
}

#ifdef MOZ_WIDGET_GONK
let librecovery = (function() {
  let library;
  try {
    library = ctypes.open("librecovery.so");
  } catch (e) {
    log("Unable to open librecovery.so");
    throw Cr.NS_ERROR_FAILURE;
  }
  let FotaUpdateStatus = new ctypes.StructType("FotaUpdateStatus", [
                                                { result: ctypes.int },
                                                { updatePath: ctypes.char.ptr }
                                              ]);

  return {
    factoryReset:        library.declare("factoryReset",
                                         ctypes.default_abi,
                                         ctypes.int),
    installFotaUpdate:   library.declare("installFotaUpdate",
                                         ctypes.default_abi,
                                         ctypes.int,
                                         ctypes.char.ptr,
                                         ctypes.int),

    FotaUpdateStatus:    FotaUpdateStatus,
    getFotaUpdateStatus: library.declare("getFotaUpdateStatus",
                                         ctypes.default_abi,
                                         ctypes.int,
                                         FotaUpdateStatus.ptr)
  };
})();
#endif

function RecoveryService() {}

RecoveryService.prototype = {
  classID: RECOVERYSERVICE_CID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIRecoveryService]),
  classInfo: XPCOMUtils.generateCI({
    classID: RECOVERYSERVICE_CID,
    contractID: RECOVERYSERVICE_CONTRACTID,
    interfaces: [Ci.nsIRecoveryService],
    classDescription: "B2G Recovery Service"
  }),

  factoryReset: function RS_factoryReset() {
#ifdef MOZ_WIDGET_GONK
    // If this succeeds, then the device reboots and this never returns
    if (librecovery.factoryReset() != 0) {
      log("Error: Factory reset failed");
    }
#endif
    throw Cr.NS_ERROR_FAILURE;
  },

  installFotaUpdate: function RS_installFotaUpdate(updatePath) {
#ifdef MOZ_WIDGET_GONK
    // If this succeeds, then the device reboots and this never returns
    if (librecovery.installFotaUpdate(updatePath, updatePath.length) != 0) {
      log("Error: FOTA install failed");
    }
#endif
    throw Cr.NS_ERROR_FAILURE;
  },

  getFotaUpdateStatus: function RS_getFotaUpdateStatus() {
    let status =  Ci.nsIRecoveryService.FOTA_UPDATE_UNKNOWN;
#ifdef MOZ_WIDGET_GONK
    let cStatus = librecovery.FotaUpdateStatus();

    if (librecovery.getFotaUpdateStatus(cStatus.address()) == 0) {
      status = cStatus.result;
    }

#endif
    return status;
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([RecoveryService]);
