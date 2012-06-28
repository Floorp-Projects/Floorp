/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function getLoadContext() {
  return window.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIWebNavigation)
               .QueryInterface(Ci.nsILoadContext);
}

/*
 * Polls the X11 primary selection buffer waiting for the expected value. A known
 * value different than the expected value is put on the clipboard first (and
 * also polled for) so we can be sure the value we get isn't just the expected
 * value because it was already in the buffer.
 *
 * @param aExpectedStringOrValidatorFn
 *        The string value that is expected to be in the X11 primary selection buffer
 *        or a validator function getting clipboard data and returning a bool.
 * @param aSetupFn
 *        A function responsible for setting the primary selection buffer to the
 *        expected value, called after the known value setting succeeds.
 * @param aSuccessFn
 *        A function called when the expected value is found in the primary
 *        selection buffer.
 * @param aFailureFn
 *        A function called if the expected value isn't found in the primary
 *        selection buffer within 5s. It can also be called if the known value
 *        can't be found.
 * @param aFlavor [optional] The flavor to look for. Defaults to "text/unicode".
 */
function waitForSelection(aExpectedStringOrValidatorFn, aSetupFn,
                          aSuccessFn, aFailureFn, aFlavor) {
    let requestedFlavor = aFlavor || "text/unicode";

    // Build a default validator function for common string input.
    var inputValidatorFn = typeof(aExpectedStringOrValidatorFn) == "string"
        ? function(aData) aData == aExpectedStringOrValidatorFn
        : aExpectedStringOrValidatorFn;

    let clipboard = Cc["@mozilla.org/widget/clipboard;1"].
                    getService(Ci.nsIClipboard);

    // reset for the next use
    function reset() {
      waitForSelection._polls = 0;
    }

    function wait(validatorFn, successFn, failureFn, flavor) {
      if (++waitForSelection._polls > 50) {
        // Log the failure.
        ok(false, "Timed out while polling the X11 primary selection buffer.");
        reset();
        failureFn();
        return;
      }

      let transferable = Cc["@mozilla.org/widget/transferable;1"].
                         createInstance(Ci.nsITransferable);
      transferable.init(getLoadContext());
      transferable.addDataFlavor(requestedFlavor);

      clipboard.getData(transferable, clipboard.kSelectionClipboard);

      let str = {};
      let strLength = {};

      transferable.getTransferData(requestedFlavor, str, strLength);

      let data = null;
      if (str.value) {
        let strValue = str.value.QueryInterface(Ci.nsISupportsString);
        data = strValue.data.substring(0, strLength.value / 2);
      }

      if (validatorFn(data)) {
        // Don't show the success message when waiting for preExpectedVal
        if (preExpectedVal) {
          preExpectedVal = null;
        } else {
          ok(true, "The X11 primary selection buffer has the correct value");
        }
        reset();
        successFn();
      } else {
        setTimeout(function() wait(validatorFn, successFn, failureFn, flavor), 100);
      }
    }

    // First we wait for a known value different from the expected one.
    var preExpectedVal = waitForSelection._monotonicCounter +
                         "-waitForSelection-known-value";

    let clipboardHelper = Cc["@mozilla.org/widget/clipboardhelper;1"].
                          getService(Ci.nsIClipboardHelper);
    clipboardHelper.copyStringToClipboard(preExpectedVal,
                                          document,
                                          Ci.nsIClipboard.kSelectionClipboard);

    wait(function(aData) aData == preExpectedVal,
         function() {
           // Call the original setup fn
           aSetupFn();
           wait(inputValidatorFn, aSuccessFn, aFailureFn, requestedFlavor);
         }, aFailureFn, "text/unicode");
}

waitForSelection._polls = 0;
waitForSelection.__monotonicCounter = 0;
waitForSelection.__defineGetter__("_monotonicCounter", function () {
  return waitForSelection.__monotonicCounter++;
});

