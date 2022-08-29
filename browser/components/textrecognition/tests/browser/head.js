/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * @param {string} text
 */
function setClipboardText(text) {
  const ClipboardHelper = Cc[
    "@mozilla.org/widget/clipboardhelper;1"
  ].getService(Ci.nsIClipboardHelper);
  ClipboardHelper.copyString(text);
}

/**
 * @returns {string}
 */
function getTextFromClipboard() {
  const transferable = Cc["@mozilla.org/widget/transferable;1"].createInstance(
    Ci.nsITransferable
  );
  transferable.init(window.docShell.QueryInterface(Ci.nsILoadContext));
  transferable.addDataFlavor("text/unicode");
  Services.clipboard.getData(transferable, Services.clipboard.kGlobalClipboard);

  const results = {};
  transferable.getTransferData("text/unicode", results);
  return results.value.QueryInterface(Ci.nsISupportsString)?.data ?? "";
}

/**
 * Returns events specifically for text recognition.
 */
function getTelemetryScalars() {
  const snapshot = Services.telemetry.getSnapshotForKeyedScalars(
    "main",
    true /* clear events */
  );

  if (!snapshot.parent) {
    return {};
  }

  return snapshot.parent;
}

function clearTelemetry() {
  Services.telemetry.clearScalars();
  Services.telemetry
    .getHistogramById("TEXT_RECOGNITION_API_PERFORMANCE")
    .clear();
  Services.telemetry
    .getHistogramById("TEXT_RECOGNITION_INTERACTION_TIMING")
    .clear();
  Services.telemetry.getHistogramById("TEXT_RECOGNITION_TEXT_LENGTH").clear();
}
