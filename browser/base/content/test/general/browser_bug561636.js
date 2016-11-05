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

var gObserver = {
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIFormSubmitObserver]),

  notifyInvalidSubmit : function (aFormElement, aInvalidElements)
  {
  }
};

var testId = 0;

function incrementTest()
{
  testId++;
  info("Starting next part of test");
}

function getDocHeader()
{
  return "<html><head><meta charset='utf-8'></head><body>" + getEmptyFrame();
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

function* openNewTab(uri, background)
{
  let tab = gBrowser.addTab();
  let browser = gBrowser.getBrowserForTab(tab);
  if (!background) {
    gBrowser.selectedTab = tab;
  }
  yield promiseTabLoadEvent(tab, "data:text/html," + escape(uri));
  return browser;
}

function* clickChildElement(browser)
{
  yield ContentTask.spawn(browser, {}, function* () {
    content.document.getElementById('s').click();
  });
}

function* blurChildElement(browser)
{
  yield ContentTask.spawn(browser, {}, function* () {
    content.document.getElementById('i').blur();
  });
}

function* checkChildFocus(browser, message)
{
  yield ContentTask.spawn(browser, [message, testId], function* (args) {
    let [msg, id] = args;
    var focused = content.document.activeElement == content.document.getElementById('i');

    var validMsg = true;
    if (msg) {
      validMsg = (msg == content.document.getElementById('i').validationMessage);
    }

    Assert.equal(focused, true, "Test " + id + " First invalid element should be focused");
    Assert.equal(validMsg, true, "Test " + id + " The panel should show the message from validationMessage");
  });
}

/**
 * In this test, we check that no popup appears if the form is valid.
 */
add_task(function* ()
{
  incrementTest();
  let uri = getDocHeader() + "<form target='t' action='data:text/html,'><input><input id='s' type='submit'></form>" + getDocFooter();
  let browser = yield openNewTab(uri);

  yield clickChildElement(browser);

  yield new Promise((resolve, reject) => {
    // XXXndeakin This isn't really going to work when the content is another process
    executeSoon(function() {
      checkPopupHide();
      resolve();
    });
  });

  gBrowser.removeCurrentTab();
});

/**
 * In this test, we check that, when an invalid form is submitted,
 * the invalid element is focused and a popup appears.
 */
add_task(function* ()
{
  incrementTest();
  let uri = getDocHeader() + "<form target='t' action='data:text/html,'><input required id='i'><input id='s' type='submit'></form>" + getDocFooter();
  let browser = yield openNewTab(uri);

  let popupShownPromise = promiseWaitForEvent(gInvalidFormPopup, "popupshown");
  yield clickChildElement(browser);
  yield popupShownPromise;

  checkPopupShow();
  yield checkChildFocus(browser, gInvalidFormPopup.firstChild.textContent);

  gBrowser.removeCurrentTab();
});

/**
 * In this test, we check that, when an invalid form is submitted,
 * the first invalid element is focused and a popup appears.
 */
add_task(function* ()
{
  incrementTest();
  let uri = getDocHeader() + "<form target='t' action='data:text/html,'><input><input id='i' required><input required><input id='s' type='submit'></form>" + getDocFooter();
  let browser = yield openNewTab(uri);

  let popupShownPromise = promiseWaitForEvent(gInvalidFormPopup, "popupshown");
  yield clickChildElement(browser);
  yield popupShownPromise;

  checkPopupShow();
  yield checkChildFocus(browser, gInvalidFormPopup.firstChild.textContent);

  gBrowser.removeCurrentTab();
});

/**
 * In this test, we check that, we hide the popup by interacting with the
 * invalid element if the element becomes valid.
 */
add_task(function* ()
{
  incrementTest();
  let uri = getDocHeader() + "<form target='t' action='data:text/html,'><input id='i' required><input id='s' type='submit'></form>" + getDocFooter();
  let browser = yield openNewTab(uri);

  let popupShownPromise = promiseWaitForEvent(gInvalidFormPopup, "popupshown");
  yield clickChildElement(browser);
  yield popupShownPromise;

  checkPopupShow();
  yield checkChildFocus(browser, gInvalidFormPopup.firstChild.textContent);

  let popupHiddenPromise = promiseWaitForEvent(gInvalidFormPopup, "popuphidden");
  EventUtils.synthesizeKey("a", {});
  yield popupHiddenPromise;

  gBrowser.removeCurrentTab();
});

/**
 * In this test, we check that, we don't hide the popup by interacting with the
 * invalid element if the element is still invalid.
 */
add_task(function* ()
{
  incrementTest();
  let uri = getDocHeader() + "<form target='t' action='data:text/html,'><input type='email' id='i' required><input id='s' type='submit'></form>" + getDocFooter();
  let browser = yield openNewTab(uri);

  let popupShownPromise = promiseWaitForEvent(gInvalidFormPopup, "popupshown");
  yield clickChildElement(browser);
  yield popupShownPromise;

  checkPopupShow();
  yield checkChildFocus(browser, gInvalidFormPopup.firstChild.textContent);

  yield new Promise((resolve, reject) => {
    EventUtils.synthesizeKey("a", {});
    executeSoon(function() {
      checkPopupShow();
      resolve();
    })
  });

  gBrowser.removeCurrentTab();
});

/**
 * In this test, we check that we can hide the popup by blurring the invalid
 * element.
 */
add_task(function* ()
{
  incrementTest();
  let uri = getDocHeader() + "<form target='t' action='data:text/html,'><input id='i' required><input id='s' type='submit'></form>" + getDocFooter();
  let browser = yield openNewTab(uri);

  let popupShownPromise = promiseWaitForEvent(gInvalidFormPopup, "popupshown");
  yield clickChildElement(browser);
  yield popupShownPromise;

  checkPopupShow();
  yield checkChildFocus(browser, gInvalidFormPopup.firstChild.textContent);

  let popupHiddenPromise = promiseWaitForEvent(gInvalidFormPopup, "popuphidden");
  yield blurChildElement(browser);
  yield popupHiddenPromise;

  gBrowser.removeCurrentTab();
});

/**
 * In this test, we check that we can hide the popup by pressing TAB.
 */
add_task(function* ()
{
  incrementTest();
  let uri = getDocHeader() + "<form target='t' action='data:text/html,'><input id='i' required><input id='s' type='submit'></form>" + getDocFooter();
  let browser = yield openNewTab(uri);

  let popupShownPromise = promiseWaitForEvent(gInvalidFormPopup, "popupshown");
  yield clickChildElement(browser);
  yield popupShownPromise;

  checkPopupShow();
  yield checkChildFocus(browser, gInvalidFormPopup.firstChild.textContent);

  let popupHiddenPromise = promiseWaitForEvent(gInvalidFormPopup, "popuphidden");
  EventUtils.synthesizeKey("VK_TAB", {});
  yield popupHiddenPromise;

  gBrowser.removeCurrentTab();
});

/**
 * In this test, we check that the popup will hide if we move to another tab.
 */
add_task(function* ()
{
  incrementTest();
  let uri = getDocHeader() + "<form target='t' action='data:text/html,'><input id='i' required><input id='s' type='submit'></form>" + getDocFooter();
  let browser1 = yield openNewTab(uri);

  let popupShownPromise = promiseWaitForEvent(gInvalidFormPopup, "popupshown");
  yield clickChildElement(browser1);
  yield popupShownPromise;

  checkPopupShow();
  yield checkChildFocus(browser1, gInvalidFormPopup.firstChild.textContent);

  let popupHiddenPromise = promiseWaitForEvent(gInvalidFormPopup, "popuphidden");

  let browser2 = yield openNewTab("data:text/html,<html></html>");
  yield popupHiddenPromise;

  gBrowser.removeTab(gBrowser.getTabForBrowser(browser1));
  gBrowser.removeTab(gBrowser.getTabForBrowser(browser2));
});

/**
 * In this test, we check that nothing happen if the invalid form is
 * submitted in a background tab.
 */
add_task(function* ()
{
  // Observers don't propagate currently across processes. We may add support for this in the
  // future via the addon compat layer.
  if (gMultiProcessBrowser) {
    return;
  }

  incrementTest();
  let uri = getDocHeader() + "<form target='t' action='data:text/html,'><input id='i' required><input id='s' type='submit'></form>" + getDocFooter();
  let browser = yield openNewTab(uri, true);
  isnot(gBrowser.selectedBrowser, browser, "This tab should have been loaded in background");

  let notifierPromise = new Promise((resolve, reject) => {
    gObserver.notifyInvalidSubmit = function() {
      executeSoon(function() {
        checkPopupHide();

        // Clean-up
        Services.obs.removeObserver(gObserver, "invalidformsubmit");
        gObserver.notifyInvalidSubmit = function () {};
        resolve();
      });
    };

    Services.obs.addObserver(gObserver, "invalidformsubmit", false);

    executeSoon(function () {
      browser.contentDocument.getElementById('s').click();
    });
  });

  yield notifierPromise;

  gBrowser.removeTab(gBrowser.getTabForBrowser(browser));
});

/**
 * In this test, we check that the author defined error message is shown.
 */
add_task(function* ()
{
  incrementTest();
  let uri = getDocHeader() + "<form target='t' action='data:text/html,'><input x-moz-errormessage='foo' required id='i'><input id='s' type='submit'></form>" + getDocFooter();
  let browser = yield openNewTab(uri);

  let popupShownPromise = promiseWaitForEvent(gInvalidFormPopup, "popupshown");
  yield clickChildElement(browser);
  yield popupShownPromise;

  checkPopupShow();
  yield checkChildFocus(browser, gInvalidFormPopup.firstChild.textContent);

  is(gInvalidFormPopup.firstChild.textContent, "foo",
     "The panel should show the author defined error message");

  gBrowser.removeCurrentTab();
});

/**
 * In this test, we check that the message is correctly updated when it changes.
 */
add_task(function* ()
{
  incrementTest();
  let uri = getDocHeader() + "<form target='t' action='data:text/html,'><input type='email' required id='i'><input id='s' type='submit'></form>" + getDocFooter();
  let browser = yield openNewTab(uri);

  let popupShownPromise = promiseWaitForEvent(gInvalidFormPopup, "popupshown");
  yield clickChildElement(browser);
  yield popupShownPromise;

  checkPopupShow();
  yield checkChildFocus(browser, gInvalidFormPopup.firstChild.textContent);

  let inputPromise = promiseWaitForEvent(gBrowser.contentDocument.getElementById('i'), "input");
  EventUtils.synthesizeKey('f', {});
  yield inputPromise;

  // Now, the element suffers from another error, the message should have
  // been updated.
  yield new Promise((resolve, reject) => {
    // XXXndeakin This isn't really going to work when the content is another process
    executeSoon(function() {
      checkChildFocus(browser, gInvalidFormPopup.firstChild.textContent);
      resolve();
    });
  });

  gBrowser.removeCurrentTab();
});
