/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

let Ci = Components.interfaces;

function test() {
  waitForExplicitFinish();
  let tab = gBrowser.addTab();
  gBrowser.selectedTab = tab;
  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);

    ok(true, "Load listener called");
    waitForFocus(onFocus, content);
  }, true);

  content.location = "data:text/html,<h1 id='h1'>Select Me</h1>";
}

function selectText() {
  let elt = content.document.getElementById("h1");
  let selection = content.getSelection();
  let range = content.document.createRange();
  range.setStart(elt, 0);
  range.setEnd(elt, 1);
  selection.removeAllRanges();
  selection.addRange(range);
}


function onFocus() {
  ok(!gFindBarInitialized, "find bar is not yet initialized");
  selectText();
  gFindBar.onFindCommand();
  ok(gFindBar._findField.value == "Select Me", "Findbar is initialized with selection");
  gFindBar.close();
  gBrowser.removeCurrentTab();
  finish();
}
