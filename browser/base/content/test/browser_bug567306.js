/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

let Ci = Components.interfaces;
let testWindow = null;

function test() {
  waitForExplicitFinish();

  testWindow = OpenBrowserWindow();
  testWindow.addEventListener("load", function(aEvent) {
    testWindow.removeEventListener("load", arguments.callee, false);
    ok(true, "Load listener called");

    executeSoon(function() {
      let selectedBrowser = testWindow.gBrowser.selectedBrowser;
      selectedBrowser.addEventListener("pageshow", function() {
        selectedBrowser.removeEventListener("pageshow", arguments.callee, true);
        ok(true, "pageshow listener called");
        waitForFocus(onFocus, testWindow.contentWindow);
      }, true);
      testWindow.content.location = "data:text/html,<h1 id='h1'>Select Me</h1>";
    });
  }, false);
}

function selectText() {
  let elt = testWindow.content.document.getElementById("h1");
  let selection = testWindow.content.getSelection();
  let range = testWindow.content.document.createRange();
  range.setStart(elt, 0);
  range.setEnd(elt, 1);
  selection.removeAllRanges();
  selection.addRange(range);
}

function onFocus() {
  ok(!testWindow.gFindBarInitialized, "find bar is not yet initialized");
  let findBar = testWindow.gFindBar;
  selectText();
  findBar.onFindCommand();
  is(findBar._findField.value, "Select Me", "Findbar is initialized with selection");
  findBar.close();
  testWindow.close();
  finish();
}
