/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

let subscriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
                        .getService(Ci.mozIJSSubScriptLoader);

/**
 * Start a new payment module.
 *
 * @param custom_ns
 *        Namespace with symbols to be injected into the new payment module
 *        namespace.
 *
 * @return an object that represents the payment module's namespace.
 */
function newPaymentModule(custom_ns) {
  let payment_ns = {
    importScripts: function fakeImportScripts() {
      Array.slice(arguments).forEach(function (script) {
        subscriptLoader.loadSubScript("resource://gre/modules/" + script, this);
      }, this);
    },
  };

  // Copy the custom definitions over.
  for (let key in custom_ns) {
    payment_ns[key] = custom_ns[key];
  }

  // Load the payment module itself.
  payment_ns.importScripts("Payment.jsm");

  return payment_ns;
}
