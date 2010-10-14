/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

let testWindow = null;
function test() {
  waitForExplicitFinish();
  testWindow = OpenBrowserWindow();
  testWindow.addEventListener("load", function() {
    testWindow.removeEventListener("load", arguments.callee, true);
    executeSoon(function() {
      ok(true, "Load listener called");
      testWindow.gBrowser.selectedBrowser.addEventListener("pageshow", function () {
        ok(true, "Pageshow listener called");
        testWindow.gBrowser.selectedBrowser.removeEventListener("pageshow", arguments.callee, false);
        waitForFocus(onFocus, testWindow.content);
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
  selectText();
  testWindow.gFindBar.onFindCommand();
  ok(testWindow.gFindBar._findField.value == "Select Me", "Findbar is initialized with selection");
  testWindow.gFindBar.close();
  testWindow.close();
  finish();
}
