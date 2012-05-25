/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "data:text/html;charset=utf-8,<p>test for bug 642615";

XPCOMUtils.defineLazyServiceGetter(this, "clipboardHelper",
                                   "@mozilla.org/widget/clipboardhelper;1",
                                   "nsIClipboardHelper");

function consoleOpened(HUD) {
  let jsterm = HUD.jsterm;
  let stringToCopy = "foobazbarBug642615";

  jsterm.clearOutput();

  ok(!jsterm.completeNode.value, "no completeNode.value");

  jsterm.setInputValue("doc");

  let completionValue;

  // wait for key "u"
  function onCompletionValue() {
    completionValue = jsterm.completeNode.value;

    // Arguments: expected, setup, success, failure.
    waitForClipboard(
      stringToCopy,
      function() {
        clipboardHelper.copyString(stringToCopy);
      },
      onClipboardCopy,
      finishTest);
  }

  function onClipboardCopy() {
    updateEditUIVisibility();
    goDoCommand("cmd_paste");

    waitForSuccess(waitForPaste);
  }

  let waitForPaste = {
    name: "no completion value after paste",
    validatorFn: function()
    {
      return !jsterm.completeNode.value;
    },
    successFn: onClipboardPaste,
    failureFn: finishTest,
  };

  function onClipboardPaste() {
    goDoCommand("cmd_undo");
    waitForSuccess({
      name: "completion value for 'docu' after undo",
      validatorFn: function()
      {
        return !!jsterm.completeNode.value;
      },
      successFn: onCompletionValueAfterUndo,
      failureFn: finishTest,
    });
  }

  function onCompletionValueAfterUndo() {
    is(jsterm.completeNode.value, completionValue,
       "same completeNode.value after undo");

    EventUtils.synthesizeKey("v", {accelKey: true});
    waitForSuccess({
      name: "no completion after ctrl-v (paste)",
      validatorFn: function()
      {
        return !jsterm.completeNode.value;
      },
      successFn: finishTest,
      failureFn: finishTest,
    });
  }

  EventUtils.synthesizeKey("u", {});

  waitForSuccess({
    name: "completion value for 'docu'",
    validatorFn: function()
    {
      return !!jsterm.completeNode.value;
    },
    successFn: onCompletionValue,
    failureFn: finishTest,
  });
}

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);
}
