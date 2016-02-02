/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cm = Components.manager;
const Cu = Components.utils;

const CONTRACT_ID = "@mozilla.org/payment/ui-glue;1";

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

var registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
var oldClassID, oldFactory;
var newClassID = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator).generateUUID();
var newFactory = {
  createInstance: function(aOuter, aIID) {
    if (aOuter) {
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    }
    return new MockPaymentsUIGlueInstance().QueryInterface(aIID);
  },
  lockFactory: function(aLock) {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFactory])
};

addMessageListener("MockPaymentsUIGlue.init", function (message) {
  try {
    oldClassID = registrar.contractIDToCID(CONTRACT_ID);
    oldFactory = Cm.getClassObject(oldClassID, Ci.nsIFactory);
  } catch (ex) {
    oldClassID = "";
    oldFactory = null;
    dump("TEST-INFO | can't get payments ui glue registered component, " +
         "assuming there is none\n");
  }
  if (oldFactory) {
    registrar.unregisterFactory(oldClassID, oldFactory);
  }
  registrar.registerFactory(newClassID, "", CONTRACT_ID, newFactory);});

addMessageListener("MockPaymentsUIGlue.cleanup", function (message) {
  if (oldClassID) {
    registrar.registerFactory(oldClassID, "", CONTRACT_ID, null);
  }
});

var payments = new Map();

function MockPaymentsUIGlueInstance() {
};

MockPaymentsUIGlueInstance.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPaymentUIGlue]),

  confirmPaymentRequest: function(aRequestId,
                                  aRequests,
                                  aSuccessCb,
                                  aErrorCb) {
    aSuccessCb.onresult(aRequestId, aRequests[0].type);
  },

  showPaymentFlow: function(aRequestId,
                            aPaymentFlowInfo,
                            aErrorCb) {
    let win = Services.ww.openWindow(null,
                                     null,
                                     "_blank",
                                     "chrome,dialog=no,resizable,scrollbars,centerscreen",
                                     null);

    payments.set(aRequestId, win);
    let docshell = win.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIWebNavigation)
                      .QueryInterface(Ci.nsIDocShell);
    docshell.paymentRequestId = aRequestId;

    win.document.location = aPaymentFlowInfo.uri + aPaymentFlowInfo.jwt;
  },

  closePaymentFlow: function(aRequestId) {
    payments.get(aRequestId).close();
    payments.delete(aRequestId);

    return Promise.resolve();
  },
};
