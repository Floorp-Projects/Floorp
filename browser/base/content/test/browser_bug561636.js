var gInvalidFormPopup = document.getElementById('invalid-form-popup');
ok(gInvalidFormPopup,
   "The browser should have a popup to show when a form is invalid");

function checkPopupShow()
{
  ok(gInvalidFormPopup.state == 'showing' || gInvalidFormPopup.state == 'open',
     "The invalid form popup should be shown");
}

function checkPopupHide()
{
  ok(gInvalidFormPopup.state != 'showing' && gInvalidFormPopup.state != 'open',
     "The invalid form popup should not be shown");
}

function checkPopupMessage(doc)
{
  is(gInvalidFormPopup.firstChild.nodeValue,
     doc.getElementById('i').validationMessage,
     "The panel should show the message from validationMessage");
}

let gObserver = {
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIFormSubmitObserver]),

  notifyInvalidSubmit : function (aFormElement, aInvalidElements)
  {
  }
};

function test()
{
  waitForExplicitFinish();

  test1();
}

/**
 * In this test, we check that no popup appears if the form is valid.
 */
function test1() {
  let uri = "data:text/html,<html><body><iframe name='t'></iframe><form target='t' action='data:text/html,'><input><input id='s' type='submit'></form></body></html>";
  let tab = gBrowser.addTab();

  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
    let doc = gBrowser.contentDocument;

    doc.getElementById('s').click();

    executeSoon(function() {
      checkPopupHide();

      // Clean-up
      gBrowser.removeTab(gBrowser.selectedTab, {animate: false});

      // Next test
      executeSoon(test2);
    });
  }, true);

  gBrowser.selectedTab = tab;
  gBrowser.selectedTab.linkedBrowser.loadURI(uri);
}

/**
 * In this test, we check that, when an invalid form is submitted,
 * the invalid element is focused and a popup appears.
 */
function test2()
{
  let uri = "data:text/html,<iframe name='t'></iframe><form target='t' action='data:text/html,'><input required id='i'><input id='s' type='submit'></form>";
  let tab = gBrowser.addTab();

  gInvalidFormPopup.addEventListener("popupshown", function() {
    gInvalidFormPopup.removeEventListener("popupshown", arguments.callee, false);

    let doc = gBrowser.contentDocument;
    is(doc.activeElement, doc.getElementById('i'),
       "First invalid element should be focused");

    checkPopupShow();
    checkPopupMessage(doc);

    // Clean-up and next test.
    gBrowser.removeTab(gBrowser.selectedTab, {animate: false});
    executeSoon(test3);
  }, false);

  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);

    gBrowser.contentDocument.getElementById('s').click();
  }, true);

  gBrowser.selectedTab = tab;
  gBrowser.selectedTab.linkedBrowser.loadURI(uri);
}

/**
 * In this test, we check that, when an invalid form is submitted,
 * the first invalid element is focused and a popup appears.
 */
function test3()
{
  let uri = "data:text/html,<iframe name='t'></iframe><form target='t' action='data:text/html,'><input><input id='i' required><input required><input id='s' type='submit'></form>";
  let tab = gBrowser.addTab();

  gInvalidFormPopup.addEventListener("popupshown", function() {
    gInvalidFormPopup.removeEventListener("popupshown", arguments.callee, false);

    let doc = gBrowser.contentDocument;
    is(doc.activeElement, doc.getElementById('i'),
       "First invalid element should be focused");

    checkPopupShow();
    checkPopupMessage(doc);

    // Clean-up and next test.
    gBrowser.removeTab(gBrowser.selectedTab, {animate: false});
    executeSoon(test4);
  }, false);

  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);

    gBrowser.contentDocument.getElementById('s').click();
  }, true);

  gBrowser.selectedTab = tab;
  gBrowser.selectedTab.linkedBrowser.loadURI(uri);
}

/**
 * In this test, we check that, we can hide the popup by interacting with the
 * invalid element.
 */
function test4()
{
  let uri = "data:text/html,<iframe name='t'></iframe><form target='t' action='data:text/html,'><input id='i' required><input id='s' type='submit'></form>";
  let tab = gBrowser.addTab();

  gInvalidFormPopup.addEventListener("popupshown", function() {
    gInvalidFormPopup.removeEventListener("popupshown", arguments.callee, false);

    let doc = gBrowser.contentDocument;
    is(doc.activeElement, doc.getElementById('i'),
       "First invalid element should be focused");

    checkPopupShow();
    checkPopupMessage(doc);

    EventUtils.synthesizeKey("a", {});

    executeSoon(function () {
      checkPopupHide();

      // Clean-up and next test.
      gBrowser.removeTab(gBrowser.selectedTab, {animate: false});
      executeSoon(test5);
    });
  }, false);

  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);

    gBrowser.contentDocument.getElementById('s').click();
  }, true);

  gBrowser.selectedTab = tab;
  gBrowser.selectedTab.linkedBrowser.loadURI(uri);
}

/**
 * In this test, we check that we can hide the popup by blurring the invalid
 * element.
 */
function test5()
{
  let uri = "data:text/html,<iframe name='t'></iframe><form target='t' action='data:text/html,'><input id='i' required><input id='s' type='submit'></form>";
  let tab = gBrowser.addTab();

  gInvalidFormPopup.addEventListener("popupshown", function() {
    gInvalidFormPopup.removeEventListener("popupshown", arguments.callee, false);

    let doc = gBrowser.contentDocument;
    is(doc.activeElement, doc.getElementById('i'),
       "First invalid element should be focused");

    checkPopupShow();
    checkPopupMessage(doc);

    doc.getElementById('i').blur();

    executeSoon(function () {
      checkPopupHide();

      // Clean-up and next test.
      gBrowser.removeTab(gBrowser.selectedTab, {animate: false});
      executeSoon(test6);
    });
  }, false);

  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);

    gBrowser.contentDocument.getElementById('s').click();
  }, true);

  gBrowser.selectedTab = tab;
  gBrowser.selectedTab.linkedBrowser.loadURI(uri);
}

/**
 * In this test, we check that we can hide the popup by pressing TAB.
 */
function test6()
{
  let uri = "data:text/html,<iframe name='t'></iframe><form target='t' action='data:text/html,'><input id='i' required><input id='s' type='submit'></form>";
  let tab = gBrowser.addTab();

  gInvalidFormPopup.addEventListener("popupshown", function() {
    gInvalidFormPopup.removeEventListener("popupshown", arguments.callee, false);

    let doc = gBrowser.contentDocument;
    is(doc.activeElement, doc.getElementById('i'),
       "First invalid element should be focused");

    checkPopupShow();
    checkPopupMessage(doc);

    EventUtils.synthesizeKey("VK_TAB", {});

    executeSoon(function () {
      checkPopupHide();

      // Clean-up and next test.
      gBrowser.removeTab(gBrowser.selectedTab, {animate: false});
      executeSoon(test7);
    });
  }, false);

  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);

    gBrowser.contentDocument.getElementById('s').click();
  }, true);

  gBrowser.selectedTab = tab;
  gBrowser.selectedTab.linkedBrowser.loadURI(uri);
}

/**
 * In this test, we check that the popup will hide if we move to another tab.
 */
function test7()
{
  let uri = "data:text/html,<iframe name='t'></iframe><form target='t' action='data:text/html,'><input id='i' required><input id='s' type='submit'></form>";
  let tab = gBrowser.addTab();

  gInvalidFormPopup.addEventListener("popupshown", function() {
    gInvalidFormPopup.removeEventListener("popupshown", arguments.callee, false);

    let doc = gBrowser.contentDocument;
    is(doc.activeElement, doc.getElementById('i'),
       "First invalid element should be focused");

    checkPopupShow();
    checkPopupMessage(doc);

    // Create a new tab and move to it.
    gBrowser.selectedTab  = gBrowser.addTab("about:blank", {skipAnimation: true});

    executeSoon(function() {
      checkPopupHide();

      // Clean-up and next test.
      gBrowser.removeTab(gBrowser.selectedTab, {animate: false});
      gBrowser.removeTab(gBrowser.selectedTab, {animate: false});
      executeSoon(test8);
    });
  }, false);

  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);

    gBrowser.contentDocument.getElementById('s').click();
  }, true);

  gBrowser.selectedTab = tab;
  gBrowser.selectedTab.linkedBrowser.loadURI(uri);
}

/**
 * In this test, we check that nothing happen (no focus nor popup) if the
 * invalid form is submitted in another tab than the current focused one
 * (submitted in background).
 */
function test8()
{
  let uri = "data:text/html,<iframe name='t'></iframe><form target='t' action='data:text/html,'><input id='i' required><input id='s' type='submit'></form>";
  let tab = gBrowser.addTab();

  gObserver.notifyInvalidSubmit = function() {
    executeSoon(function() {
      let doc = tab.linkedBrowser.contentDocument;
      isnot(doc.activeElement, doc.getElementById('i'),
            "We should not focus the invalid element when the form is submitted in background");

      checkPopupHide();

      // Clean-up
      Services.obs.removeObserver(gObserver, "invalidformsubmit");
      gObserver.notifyInvalidSubmit = function () {};
      gBrowser.removeTab(tab, {animate: false});

      // Next test
      executeSoon(test9);
    });
  };

  Services.obs.addObserver(gObserver, "invalidformsubmit", false);

  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);

    isnot(gBrowser.selectedTab, tab,
          "This tab should have been loaded in background");

    tab.linkedBrowser.contentDocument.getElementById('s').click();
  }, true);

  tab.linkedBrowser.loadURI(uri);
}

/**
 * In this test, we check that the author defined error message is shown.
 */
function test9()
{
  let uri = "data:text/html,<iframe name='t'></iframe><form target='t' action='data:text/html,'><input x-moz-errormessage='foo' required id='i'><input id='s' type='submit'></form>";
  let tab = gBrowser.addTab();

  gInvalidFormPopup.addEventListener("popupshown", function() {
    gInvalidFormPopup.removeEventListener("popupshown", arguments.callee, false);

    let doc = gBrowser.contentDocument;
    is(doc.activeElement, doc.getElementById('i'),
       "First invalid element should be focused");

    checkPopupShow();

    is(gInvalidFormPopup.firstChild.nodeValue, "foo",
       "The panel should show the author defined error message");

    // Clean-up and next test.
    gBrowser.removeTab(gBrowser.selectedTab, {animate: false});
    executeSoon(finish);
  }, false);

  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);

    gBrowser.contentDocument.getElementById('s').click();
  }, true);

  gBrowser.selectedTab = tab;
  gBrowser.selectedTab.linkedBrowser.loadURI(uri);
}
