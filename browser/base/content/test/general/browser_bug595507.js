var gInvalidFormPopup = document.getElementById('invalid-form-popup');
ok(gInvalidFormPopup,
   "The browser should have a popup to show when a form is invalid");

/**
 * Make sure that the form validation error message shows even if the form is in an iframe.
 */
function test()
{
  waitForExplicitFinish();

  let uri = "data:text/html,<iframe src=\"data:text/html,<iframe name='t'></iframe><form target='t' action='data:text/html,'><input required id='i'><input id='s' type='submit'></form>\"</iframe>";
  let tab = gBrowser.addTab();

  gInvalidFormPopup.addEventListener("popupshown", function() {
    gInvalidFormPopup.removeEventListener("popupshown", arguments.callee, false);

    let doc = gBrowser.contentDocument.getElementsByTagName('iframe')[0].contentDocument;
    is(doc.activeElement, doc.getElementById('i'),
       "First invalid element should be focused");

    ok(gInvalidFormPopup.state == 'showing' || gInvalidFormPopup.state == 'open',
       "The invalid form popup should be shown");

    // Clean-up and next test.
    gBrowser.removeTab(gBrowser.selectedTab, {animate: false});
    executeSoon(finish);
  }, false);

  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
    executeSoon(function() {
      gBrowser.contentDocument.getElementsByTagName('iframe')[0].contentDocument
        .getElementById('s').click();
    });
  }, true);

  gBrowser.selectedTab = tab;
  gBrowser.selectedTab.linkedBrowser.loadURI(uri);
}
