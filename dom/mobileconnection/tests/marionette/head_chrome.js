/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_CONTEXT = "chrome";

var XPCOMUtils = Cu.import("resource://gre/modules/XPCOMUtils.jsm").XPCOMUtils;
var Promise = Cu.import("resource://gre/modules/Promise.jsm").Promise;

var mobileConnectionService =
  Cc["@mozilla.org/mobileconnection/gonkmobileconnectionservice;1"]
  .getService(Ci.nsIMobileConnectionService);
ok(mobileConnectionService,
   "mobileConnectionService.constructor is " + mobileConnectionService.constructor);

var _pendingEmulatorShellCmdCount = 0;

/**
 * Send emulator shell command with safe guard.
 *
 * We should only call |finish()| after all emulator shell command transactions
 * end, so here comes with the pending counter.  Resolve when the emulator
 * shell gives response. Never reject.
 *
 * Fulfill params:
 *   result -- an array of emulator shell response lines.
 *
 * @param aCommands
 *        A string array commands to be passed to emulator through adb shell.
 *
 * @return A deferred promise.
 */
function runEmulatorShellCmdSafe(aCommands) {
  return new Promise(function(aResolve, aReject) {
    ++_pendingEmulatorShellCmdCount;
    runEmulatorShell(aCommands, function(aResult) {
      --_pendingEmulatorShellCmdCount;

      ok(true, "Emulator shell response: " + JSON.stringify(aResult));
      aResolve(aResult);
    });
  });
}

/**
 * Get nsIMobileConnection by clientId
 *
 * @param aClient [optional]
 *        A numeric DSDS client id. Default: 0.
 *
 * @return A nsIMobileConnection.
 */
function getMobileConnection(aClientId = 0) {
  let mobileConnection = mobileConnectionService.getItemByServiceId(0);
  ok(mobileConnection,
     "mobileConnection.constructor is " + mobileConnection.constructor);
  return mobileConnection;
}

/**
 * Get Neighboring Cell Ids.
 *
 * Fulfill params:
 *   An array of nsINeighboringCellInfo.
 * Reject params:
 *   'RadioNotAvailable', 'RequestNotSupported', or 'GenericFailure'
 *
 * @param aClient [optional]
 *        A numeric DSDS client id. Default: 0.
 *
 * @return A deferred promise.
 */
function getNeighboringCellIds(aClientId = 0) {
  let mobileConnection = getMobileConnection(aClientId);
  return new Promise(function(aResolve, aReject) {
    ok(true, "getNeighboringCellIds");
    mobileConnection.getNeighboringCellIds({
      QueryInterface: XPCOMUtils.generateQI([Ci.nsINeighboringCellIdsCallback]),
      notifyGetNeighboringCellIds: function(aCount, aResults) {
        aResolve(aResults);
      },
      notifyGetNeighboringCellIdsFailed: function(aErrorMsg) {
        aReject(aErrorMsg);
      },
    });
  });
}

/**
 * Get Cell Info List.
 *
 * Fulfill params:
 *   An array of nsICellInfo.
 * Reject params:
 *   'RadioNotAvailable', 'RequestNotSupported', or 'GenericFailure'
 *
 * @param aClient [optional]
 *        A numeric DSDS client id. Default: 0.
 *
 * @return A deferred promise.
 */
function getCellInfoList(aClientId = 0) {
  let mobileConnection = getMobileConnection(aClientId);
  return new Promise(function(aResolve, aReject) {
    ok(true, "getCellInfoList");
    mobileConnection.getCellInfoList({
      QueryInterface: XPCOMUtils.generateQI([Ci.nsICellInfoListCallback]),
      notifyGetCellInfoList: function(aCount, aResults) {
        aResolve(aResults);
      },
      notifyGetCellInfoListFailed: function(aErrorMsg) {
        aReject(aErrorMsg);
      },
    });
  });
}

/**
 * Wait for pending emulator transactions and call |finish()|.
 */
function cleanUp() {
  // Use ok here so that we have at least one test run.
  ok(true, ":: CLEANING UP ::");

  waitFor(finish, function() {
    return _pendingEmulatorShellCmdCount === 0;
  });
}

/**
 * Basic test routine helper.
 *
 * This helper does nothing but clean-ups.
 *
 * @param aTestCaseMain
 *        A function that takes no parameter.
 */
function startTestBase(aTestCaseMain) {
  return Promise.resolve()
    .then(aTestCaseMain)
    .catch((aException) => {
      ok(false, "promise rejects during test: " + aException);
    })
    .then(cleanUp);
}
