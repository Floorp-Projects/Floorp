"use strict";

add_task(async function test_generateQI() {
  function checkQI(interfaces, iface) {
    let obj = {
      QueryInterface: ChromeUtils.generateQI(interfaces),
    };
    equal(obj.QueryInterface(iface), obj,
         `Correct return value for query to ${iface}`);
  }

  // Test success scenarios.
  checkQI([], Ci.nsISupports);

  checkQI([Ci.nsIPropertyBag, "nsIPropertyBag2"], Ci.nsIPropertyBag);
  checkQI([Ci.nsIPropertyBag, "nsIPropertyBag2"], Ci.nsIPropertyBag2);

  checkQI([Ci.nsIPropertyBag, "nsIPropertyBag2", "nsINotARealInterface"], Ci.nsIPropertyBag2);

  // Non-IID values get stringified, and don't cause any errors as long
  // as there isn't a non-IID property with the same name on Ci.
  checkQI([Ci.nsIPropertyBag, "nsIPropertyBag2", null, Object], Ci.nsIPropertyBag2);

  ChromeUtils.generateQI([])(Ci.nsISupports);

  // Test failure scenarios.
  Assert.throws(() => ChromeUtils.generateQI(["toString"]),
                e => e.result == Cr.NS_ERROR_INVALID_ARG);

  Assert.throws(() => checkQI([], Ci.nsIPropertyBag),
                e => e.result == Cr.NS_ERROR_NO_INTERFACE);
});
