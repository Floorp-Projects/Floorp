/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    openScratchpad(runTests);
  }, true);

  content.location = "data:text/html,<title>foobarBug636725</title>" +
    "<p>test inspect() in Scratchpad";
}

function runTests()
{
  let sp = gScratchpadWindow.Scratchpad;

  sp.setText("document");

  sp.inspect().then(function() {

    let propPanel = document.querySelector(".scratchpad_propertyPanel");
    ok(propPanel, "property panel is open");

    propPanel.addEventListener("popupshown", function onPopupShown() {
      propPanel.removeEventListener("popupshown", onPopupShown, false);

      let tree = propPanel.querySelector("tree");
      ok(tree, "property panel tree found");

      let column = tree.columns[0];
      let found = false;

      for (let i = 0; i < tree.view.rowCount; i++) {
        let cell = tree.view.getCellText(i, column);
        if (cell == 'title: "foobarBug636725"') {
          found = true;
          break;
        }
      }
      ok(found, "found the document.title property");

      executeSoon(function() {
        propPanel.hidePopup();

        finish();
      });
    }, false);
  }, function() {
    notok(true, "document not found");
    finish();
  });
}
