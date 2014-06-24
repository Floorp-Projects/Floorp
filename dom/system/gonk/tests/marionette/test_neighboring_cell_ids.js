/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = "head.js";

function testGetNeighboringCellIds() {
  log("Test getting neighboring cell ids");
  let deferred = Promise.defer();

  radioInterface.getNeighboringCellIds({
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
startTestBase(function() {
  // TODO: Bug 1028837 - B2G Emulator: support neighboring cell ids.
  // Currently, emulator does not support RIL_REQUEST_NEIGHBORING_CELL_IDS,
  // so we expect to get a 'RequestNotSupported' error here.
  return testGetNeighboringCellIds()
    .then(function resolve(aResult) {
      ok(false, "getNeighboringCellIds should not success");
    }, function reject(aError) {
      is(aError, "RequestNotSupported", "failed to getNeighboringCellIds");
    });
});
