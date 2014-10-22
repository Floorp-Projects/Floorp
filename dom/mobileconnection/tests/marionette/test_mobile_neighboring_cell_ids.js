/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
// This test must run in chrome context.
MARIONETTE_CONTEXT = "chrome";

let Promise = Cu.import("resource://gre/modules/Promise.jsm").Promise;

let service = Cc["@mozilla.org/mobileconnection/mobileconnectionservice;1"]
              .getService(Ci.nsIMobileConnectionService);
ok(service, "service.constructor is " + service.constructor);

let mobileConnection = service.getItemByServiceId(0);
ok(mobileConnection, "mobileConnection.constructor is " + mobileConnection.constrctor);

function testGetNeighboringCellIds() {
  log("Test getting mobile neighboring cell ids");
  let deferred = Promise.defer();

  mobileConnection.getNeighboringCellIds({
    notifyGetNeighboringCellIds: function(aResult) {
      deferred.resolve(aResult);
    },
    notifyGetNeighboringCellIdsFailed: function(aError) {
      deferred.reject(aError);
    }
  });
  return deferred.promise;
}

// Start tests
testGetNeighboringCellIds()
  .then(function resolve(aResult) {
    ok(false, "getNeighboringCellIds should not success");
  }, function reject(aError) {
    is(aError, "RequestNotSupported", "failed to getNeighboringCellIds");
  }).then(finish);