var gInvalidFormPopup = document.getElementById('invalid-form-popup');
ok(gInvalidFormPopup,
   "The browser should have a popup to show when a form is invalid");

function checkPopupShow()
{
  ok(gInvalidFormPopup.state == 'showing' || gInvalidFormPopup.state == 'open',
     "[Test " + testId + "] The invalid form popup should be shown");
}

function checkPopupHide()
{
  ok(gInvalidFormPopup.state != 'showing' && gInvalidFormPopup.state != 'open',
     "[Test " + testId + "] The invalid form popup should not be shown");
}

function checkPopupMessage(doc)
{
  is(gInvalidFormPopup.firstChild.textContent,
     doc.getElementById('i').validationMessage,
     "[Test " + testId + "] The panel should show the message from validationMessage");
}

let gObserver = {
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIFormSubmitObserver]),

  notifyInvalidSubmit : function (aFormElement, aInvalidElements)
  {
  }
};

function getDocHeader()
{
  return "data:text/html,<html><head><meta charset='utf-8'></head><body>" + getEmptyFrame();
}
 
function getDocFooter()
{
  return "</body></html>";
}
 
function getEmptyFrame()
{
  return "<iframe style='width:100px; height:30px; margin:3px; border:1px solid lightgray;' " +
         "name='t' srcdoc=\"<html><head><meta charset='utf-8'></head><body>form target</body></html>\"></iframe>";
}

var testId = -1;

function nextTest()
{
  testId++;
  if (testId >= tests.length) {
    finish();
    return;
  }
  executeSoon(tests[testId]);
}

function test()
{
  waitForExplicitFinish();
  waitForFocus(nextTest);
}

var tests = [

/**
 * In this test, we check that no popup appears if the form is valid.
 */
function()
{
  let uri = getDocHeader() + "<form target='t' action='data:text/html,'><input><input id='s' type='submit'></form>" + getDocFooter();
  let tab = gBrowser.addTab();

  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
    let doc = gBrowser.contentDocument;

    doc.getElementById('s').click();

    executeSoon(function() {
      checkPopupHide();

      // Clean-up
      gBrowser.removeTab(gBrowser.selectedTab);
      nextTest();
    });
  }, true);

  gBrowser.selectedTab = tab;
  gBrowser.selectedTab.linkedBrowser.loadURI(uri);
},

/**
 * In this test, we check that, when an invalid form is submitted,
 * the invalid element is focused and a popup appears.
 */
function()
{
  let uri = getDocHeader() + "<form target='t' action='data:text/html,'><input required id='i'><input id='s' type='submit'></form>" + getDocFooter();
  let tab = gBrowser.addTab();

  gInvalidFormPopup.addEventListener("popupshown", function() {
    gInvalidFormPopup.removeEventListener("popupshown", arguments.callee, false);

    let doc = gBrowser.contentDocument;
    is(doc.activeElement, doc.getElementById('i'),
       "First invalid element should be focused");

    checkPopupShow();
    checkPopupMessage(doc);

    // Clean-up and next test.
    gBrowser.removeTab(gBrowser.selectedTab);
    nextTest();
  }, false);

  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
    executeSoon(function() {
      gBrowser.contentDocument.getElementById('s').click();
    });
  }, true);

  gBrowser.selectedTab = tab;
  gBrowser.selectedTab.linkedBrowser.loadURI(uri);
},

/**
 * In this test, we check that, when an invalid form is submitted,
 * the first invalid element is focused and a popup appears.
 */
function()
{
  let uri = getDocHeader() + "<form target='t' action='data:text/html,'><input><input id='i' required><input required><input id='s' type='submit'></form>" + getDocFooter();
  let tab = gBrowser.addTab();

  gInvalidFormPopup.addEventListener("popupshown", function() {
    gInvalidFormPopup.removeEventListener("popupshown", arguments.callee, false);

    let doc = gBrowser.contentDocument;
    is(doc.activeElement, doc.getElementById('i'),
       "First invalid element should be focused");

    checkPopupShow();
    checkPopupMessage(doc);

    // Clean-up and next test.
    gBrowser.removeTab(gBrowser.selectedTab);
    nextTest();
  }, false);

  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
    executeSoon(function() {
      gBrowser.contentDocument.getElementById('s').click();
    });
  }, true);

  gBrowser.selectedTab = tab;
  gBrowser.selectedTab.linkedBrowser.loadURI(uri);
},

/**
 * In this test, we check that, we hide the popup by interacting with the
 * invalid element if the element becomes valid.
 */
function()
{
  let uri = getDocHeader() + "<form target='t' action='data:text/html,'><input id='i' required><input id='s' type='submit'></form>" + getDocFooter();
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
      gBrowser.removeTab(gBrowser.selectedTab);
      nextTest();
    });
  }, false);

  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
    executeSoon(function() {
      gBrowser.contentDocument.getElementById('s').click();
    });
  }, true);

  gBrowser.selectedTab = tab;
  gBrowser.selectedTab.linkedBrowser.loadURI(uri);
},

/**
 * In this test, we check that, we don't hide the popup by interacting with the
 * invalid element if the element is still invalid.
 */
function()
{
  let uri = getDocHeader() + "<form target='t' action='data:text/html,'><input type='email' id='i' required><input id='s' type='submit'></form>" + getDocFooter();
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
      checkPopupShow();

      // Clean-up and next test.
      gBrowser.removeTab(gBrowser.selectedTab);
      nextTest();
    });
  }, false);

  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
    executeSoon(function() {
      gBrowser.contentDocument.getElementById('s').click();
    });
  }, true);

  gBrowser.selectedTab = tab;
  gBrowser.selectedTab.linkedBrowser.loadURI(uri);
},

/**
 * In this test, we check that we can hide the popup by blurring the invalid
 * element.
 */
function()
{
  let uri = getDocHeader() + "<form target='t' action='data:text/html,'><input id='i' required><input id='s' type='submit'></form>" + getDocFooter();
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
      gBrowser.removeTab(gBrowser.selectedTab);
      nextTest();
    });
  }, false);

  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
    executeSoon(function() {
      gBrowser.contentDocument.getElementById('s').click();
    });
  }, true);

  gBrowser.selectedTab = tab;
  gBrowser.selectedTab.linkedBrowser.loadURI(uri);
},

/**
 * In this test, we check that we can hide the popup by pressing TAB.
 */
function()
{
  let uri = getDocHeader() + "<form target='t' action='data:text/html,'><input id='i' required><input id='s' type='submit'></form>" + getDocFooter();
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
      gBrowser.removeTab(gBrowser.selectedTab);
      nextTest();
    });
  }, false);

  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
    executeSoon(function() {
      gBrowser.contentDocument.getElementById('s').click();
    });
  }, true);

  gBrowser.selectedTab = tab;
  gBrowser.selectedTab.linkedBrowser.loadURI(uri);
},

/**
 * In this test, we check that the popup will hide if we move to another tab.
 */
function()
{
  let uri = getDocHeader() + "<form target='t' action='data:text/html,'><input id='i' required><input id='s' type='submit'></form>" + getDocFooter();
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
      gBrowser.removeTab(gBrowser.selectedTab);
      gBrowser.removeTab(gBrowser.selectedTab);
      nextTest();
    });
  }, false);

  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
    executeSoon(function() {
      gBrowser.contentDocument.getElementById('s').click();
    });
  }, true);

  gBrowser.selectedTab = tab;
  gBrowser.selectedTab.linkedBrowser.loadURI(uri);
},

/**
 * In this test, we check that nothing happen if the invalid form is
 * submitted in a background tab.
 */

function()
{
  // Observers don't propagate currently across processes. We may add support for this in the
  // future via the addon compat layer.
  if (gBrowser.isRemoteBrowser) {
    nextTest();
    return;
  }

  let uri = getDocHeader() + "<form target='t' action='data:text/html,'><input id='i' required><input id='s' type='submit'></form>" + getDocFooter();
  let tab = gBrowser.addTab();

  gObserver.notifyInvalidSubmit = function() {
    executeSoon(function() {
      checkPopupHide();

      // Clean-up
      Services.obs.removeObserver(gObserver, "invalidformsubmit");
      gObserver.notifyInvalidSubmit = function () {};
      gBrowser.removeTab(tab);

      nextTest();
    });
  };

  Services.obs.addObserver(gObserver, "invalidformsubmit", false);

  tab.linkedBrowser.addEventListener("load", function(e) {
    // Ignore load events from the iframe.
    if (tab.linkedBrowser.contentDocument == e.target) {
      let browser = e.currentTarget;
      browser.removeEventListener("load", arguments.callee, true);

      isnot(gBrowser.selectedTab.linkedBrowser, browser,
            "This tab should have been loaded in background");
      executeSoon(function() {
        browser.contentDocument.getElementById('s').click();
      });
    }
  }, true);

  tab.linkedBrowser.loadURI(uri);
},

/**
 * In this test, we check that the author defined error message is shown.
 */
function()
{
  let uri = getDocHeader() + "<form target='t' action='data:text/html,'><input x-moz-errormessage='foo' required id='i'><input id='s' type='submit'></form>" + getDocFooter();
  let tab = gBrowser.addTab();

  gInvalidFormPopup.addEventListener("popupshown", function() {
    gInvalidFormPopup.removeEventListener("popupshown", arguments.callee, false);

    let doc = gBrowser.contentDocument;
    is(doc.activeElement, doc.getElementById('i'),
       "First invalid element should be focused");

    checkPopupShow();

    is(gInvalidFormPopup.firstChild.textContent, "foo",
       "The panel should show the author defined error message");

    // Clean-up and next test.
    gBrowser.removeTab(gBrowser.selectedTab);
    nextTest();
  }, false);

  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
    executeSoon(function() {
      gBrowser.contentDocument.getElementById('s').click();
    });
  }, true);

  gBrowser.selectedTab = tab;
  gBrowser.selectedTab.linkedBrowser.loadURI(uri);
},

/**
 * In this test, we check that the message is correctly updated when it changes.
 */
function()
{
  let uri = getDocHeader() + "<form target='t' action='data:text/html,'><input type='email' required id='i'><input id='s' type='submit'></form>" + getDocFooter();
  let tab = gBrowser.addTab();

  gInvalidFormPopup.addEventListener("popupshown", function() {
    gInvalidFormPopup.removeEventListener("popupshown", arguments.callee, false);

    let doc = gBrowser.contentDocument;
    let input = doc.getElementById('i');
    is(doc.activeElement, input, "First invalid element should be focused");

    checkPopupShow();

    is(gInvalidFormPopup.firstChild.textContent, input.validationMessage,
       "The panel should show the current validation message");

    input.addEventListener('input', function() {
      input.removeEventListener('input', arguments.callee, false);

      executeSoon(function() {
        // Now, the element suffers from another error, the message should have
        // been updated.
        is(gInvalidFormPopup.firstChild.textContent, input.validationMessage,
           "The panel should show the current validation message");

        // Clean-up and next test.
        gBrowser.removeTab(gBrowser.selectedTab);
        nextTest();
      });
    }, false);

    EventUtils.synthesizeKey('f', {});
  }, false);

  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
    executeSoon(function() {
      gBrowser.contentDocument.getElementById('s').click();
    });
  }, true);

  gBrowser.selectedTab = tab;
  gBrowser.selectedTab.linkedBrowser.loadURI(uri);
},

];
