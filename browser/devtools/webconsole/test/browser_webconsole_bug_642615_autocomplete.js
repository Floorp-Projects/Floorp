/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "data:text/html,<p>test for bug 642615";

XPCOMUtils.defineLazyServiceGetter(this, "clipboardHelper",
                                   "@mozilla.org/widget/clipboardhelper;1",
                                   "nsIClipboardHelper");

function tabLoad(aEvent) {
  browser.removeEventListener(aEvent.type, arguments.callee, true);

  openConsole();

  let hudId = HUDService.getHudIdByWindow(content);
  let HUD = HUDService.hudReferences[hudId];
  let jsterm = HUD.jsterm;
  let stringToCopy = "foobazbarBug642615";

  jsterm.clearOutput();

  ok(!jsterm.completeNode.value, "no completeNode.value");

  jsterm.setInputValue("doc");

  // wait for key "u"
  jsterm.inputNode.addEventListener("keyup", function() {
    jsterm.inputNode.removeEventListener("keyup", arguments.callee, false);

    let completionValue = jsterm.completeNode.value;
    ok(completionValue, "we have a completeNode.value");

    // wait for paste
    jsterm.inputNode.addEventListener("input", function() {
      jsterm.inputNode.removeEventListener("input", arguments.callee, false);

      ok(!jsterm.completeNode.value, "no completeNode.value after clipboard paste");

      // wait for undo
      jsterm.inputNode.addEventListener("input", function() {
        jsterm.inputNode.removeEventListener("input", arguments.callee, false);

        is(jsterm.completeNode.value, completionValue,
           "same completeNode.value after undo");

        // wait for paste (via keyboard event)
        jsterm.inputNode.addEventListener("keyup", function() {
          jsterm.inputNode.removeEventListener("keyup", arguments.callee, false);

          ok(!jsterm.completeNode.value,
             "no completeNode.value after clipboard paste (via keyboard event)");

          executeSoon(finishTest);
        }, false);

        EventUtils.synthesizeKey("v", {accelKey: true});
      }, false);

      goDoCommand("cmd_undo");
    }, false);

    // Arguments: expected, setup, success, failure.
    waitForClipboard(
      stringToCopy,
      function() {
        clipboardHelper.copyString(stringToCopy);
      },
      function() {
        updateEditUIVisibility();
        goDoCommand("cmd_paste");
      },
      finish);
  }, false);

  EventUtils.synthesizeKey("u", {});
}

registerCleanupFunction(function() {
  Services.prefs.clearUserPref("devtools.gcli.enable");
});

function test() {
  Services.prefs.setBoolPref("devtools.gcli.enable", false);
  addTab(TEST_URI);
  browser.addEventListener("load", tabLoad, true);
}
