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
                                          Ci.nsIClipboard.kSelectionClipboard,
                                          document);

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

/**
 * Open a new window with a source editor inside.
 *
 * @param function aCallback
 *        The function you want invoked once the editor is loaded. The function
 *        is given two arguments: editor instance and the window object.
 * @param object [aOptions]
 *        The options object to pass to the SourceEditor.init() method.
 */
function openSourceEditorWindow(aCallback, aOptions) {
  const windowUrl = "data:text/xml;charset=UTF-8,<?xml version='1.0'?>" +
    "<window xmlns='http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul'" +
    " title='Test for Source Editor' width='600' height='500'><box flex='1'/></window>";
  const windowFeatures = "chrome,titlebar,toolbar,centerscreen,resizable,dialog=no";

  let editor = null;
  let testWin = Services.ww.openWindow(null, windowUrl, "_blank",
                                       windowFeatures, null);
  testWin.addEventListener("load", function onWindowLoad() {
    testWin.removeEventListener("load", onWindowLoad, false);
    waitForFocus(initEditor, testWin);
  }, false);

  function initEditor()
  {
    let tempScope = {};
    Cu.import("resource:///modules/source-editor.jsm", tempScope);

    let box = testWin.document.querySelector("box");
    editor = new tempScope.SourceEditor();
    editor.init(box, aOptions || {}, editorLoaded);
  }

  function editorLoaded()
  {
    editor.focus();
    waitForFocus(aCallback.bind(null, editor, testWin), testWin);
  }
}

/**
 * Get text needed to fill the editor view.
 *
 * @param object aEditor
 *        The SourceEditor instance you work with.
 * @param number aPages
 *        The number of pages you want filled with lines.
 * @return string
 *         The string you can insert into the editor so you fill the desired
 *         number of pages.
 */
function fillEditor(aEditor, aPages) {
  let view = aEditor._view;
  let model = aEditor._model;

  let lineHeight = view.getLineHeight();
  let editorHeight = view.getClientArea().height;
  let linesPerPage = Math.floor(editorHeight / lineHeight);
  let totalLines = aPages * linesPerPage;

  let text = "";
  for (let i = 0; i < totalLines; i++) {
    text += "l" + i + " lorem ipsum dolor sit amet. lipsum foobaris bazbaz,\n";
  }

  return text;
}
