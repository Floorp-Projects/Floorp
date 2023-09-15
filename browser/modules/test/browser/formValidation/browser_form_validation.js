/**
 * COPIED FROM browser/base/content/test/general/head.js.
 * This function should be removed and replaced with BTU withNewTab calls
 *
 * Waits for a load (or custom) event to finish in a given tab. If provided
 * load an uri into the tab.
 *
 * @param tab
 *        The tab to load into.
 * @param [optional] url
 *        The url to load, or the current url.
 * @return {Promise} resolved when the event is handled.
 * @resolves to the received event
 * @rejects if a valid load event is not received within a meaningful interval
 */
function promiseTabLoadEvent(tab, url) {
  info("Wait tab event: load");

  function handle(loadedUrl) {
    if (loadedUrl === "about:blank" || (url && loadedUrl !== url)) {
      info(`Skipping spurious load event for ${loadedUrl}`);
      return false;
    }

    info("Tab event received: load");
    return true;
  }

  let loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, handle);

  if (url) {
    BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, url);
  }

  return loaded;
}

var gInvalidFormPopup =
  gBrowser.selectedBrowser.browsingContext.currentWindowGlobal
    .getActor("FormValidation")
    ._getAndMaybeCreatePanel(document);
ok(
  gInvalidFormPopup,
  "The browser should have a popup to show when a form is invalid"
);

function isWithinHalfPixel(a, b) {
  return Math.abs(a - b) <= 0.5;
}

function checkPopupShow(anchorRect) {
  ok(
    gInvalidFormPopup.state == "showing" || gInvalidFormPopup.state == "open",
    "[Test " + testId + "] The invalid form popup should be shown"
  );
  // Just check the vertical position, as the horizontal position of an
  // arrow panel will be offset.
  is(
    isWithinHalfPixel(gInvalidFormPopup.screenY),
    isWithinHalfPixel(anchorRect.bottom),
    "popup top"
  );
}

function checkPopupHide() {
  ok(
    gInvalidFormPopup.state != "showing" && gInvalidFormPopup.state != "open",
    "[Test " + testId + "] The invalid form popup should not be shown"
  );
}

var testId = 0;

function incrementTest() {
  testId++;
  info("Starting next part of test");
}

function getDocHeader() {
  return "<html><head><meta charset='utf-8'></head><body>" + getEmptyFrame();
}

function getDocFooter() {
  return "</body></html>";
}

function getEmptyFrame() {
  return (
    "<iframe style='width:100px; height:30px; margin:3px; border:1px solid lightgray;' " +
    "name='t' srcdoc=\"<html><head><meta charset='utf-8'></head><body>form target</body></html>\"></iframe>"
  );
}

async function openNewTab(uri, background) {
  let tab = BrowserTestUtils.addTab(gBrowser);
  let browser = gBrowser.getBrowserForTab(tab);
  if (!background) {
    gBrowser.selectedTab = tab;
  }
  await promiseTabLoadEvent(tab, "data:text/html," + escape(uri));
  return browser;
}

function clickChildElement(browser) {
  return SpecialPowers.spawn(browser, [], async function () {
    let element = content.document.getElementById("s");
    element.click();
    return {
      bottom: content.mozInnerScreenY + element.getBoundingClientRect().bottom,
    };
  });
}

async function blurChildElement(browser) {
  await SpecialPowers.spawn(browser, [], async function () {
    content.document.getElementById("i").blur();
  });
}

async function checkChildFocus(browser, message) {
  await SpecialPowers.spawn(
    browser,
    [[message, testId]],
    async function (args) {
      let [msg, id] = args;
      var focused =
        content.document.activeElement == content.document.getElementById("i");

      var validMsg = true;
      if (msg) {
        validMsg =
          msg == content.document.getElementById("i").validationMessage;
      }

      Assert.equal(
        focused,
        true,
        "Test " + id + " First invalid element should be focused"
      );
      Assert.equal(
        validMsg,
        true,
        "Test " +
          id +
          " The panel should show the message from validationMessage"
      );
    }
  );
}

/**
 * In this test, we check that no popup appears if the form is valid.
 */
add_task(async function () {
  incrementTest();
  let uri =
    getDocHeader() +
    "<form target='t' action='data:text/html,'><input><input id='s' type='submit'></form>" +
    getDocFooter();
  let browser = await openNewTab(uri);

  await clickChildElement(browser);

  await new Promise((resolve, reject) => {
    // XXXndeakin This isn't really going to work when the content is another process
    executeSoon(function () {
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
add_task(async function () {
  incrementTest();
  let uri =
    getDocHeader() +
    "<form target='t' action='data:text/html,'><input required id='i'><input id='s' type='submit'></form>" +
    getDocFooter();
  let browser = await openNewTab(uri);

  let popupShownPromise = BrowserTestUtils.waitForEvent(
    gInvalidFormPopup,
    "popupshown"
  );
  let anchorRect = await clickChildElement(browser);
  await popupShownPromise;

  checkPopupShow(anchorRect);

  await checkChildFocus(
    browser,
    gInvalidFormPopup.firstElementChild.textContent
  );

  gBrowser.removeCurrentTab();
});

/**
 * In this test, we check that, when an invalid form is submitted,
 * the first invalid element is focused and a popup appears.
 */
add_task(async function () {
  incrementTest();
  let uri =
    getDocHeader() +
    "<form target='t' action='data:text/html,'><input><input id='i' required><input required><input id='s' type='submit'></form>" +
    getDocFooter();
  let browser = await openNewTab(uri);

  let popupShownPromise = BrowserTestUtils.waitForEvent(
    gInvalidFormPopup,
    "popupshown"
  );
  let anchorRect = await clickChildElement(browser);
  await popupShownPromise;

  checkPopupShow(anchorRect);
  await checkChildFocus(
    browser,
    gInvalidFormPopup.firstElementChild.textContent
  );

  gBrowser.removeCurrentTab();
});

/**
 * In this test, we check that, we hide the popup by interacting with the
 * invalid element if the element becomes valid.
 */
add_task(async function () {
  incrementTest();
  let uri =
    getDocHeader() +
    "<form target='t' action='data:text/html,'><input id='i' required><input id='s' type='submit'></form>" +
    getDocFooter();
  let browser = await openNewTab(uri);

  let popupShownPromise = BrowserTestUtils.waitForEvent(
    gInvalidFormPopup,
    "popupshown"
  );
  let anchorRect = await clickChildElement(browser);
  await popupShownPromise;

  checkPopupShow(anchorRect);
  await checkChildFocus(
    browser,
    gInvalidFormPopup.firstElementChild.textContent
  );

  let popupHiddenPromise = BrowserTestUtils.waitForEvent(
    gInvalidFormPopup,
    "popuphidden"
  );
  EventUtils.sendString("a");
  await popupHiddenPromise;

  gBrowser.removeCurrentTab();
});

/**
 * In this test, we check that, we don't hide the popup by interacting with the
 * invalid element if the element is still invalid.
 */
add_task(async function () {
  incrementTest();
  let uri =
    getDocHeader() +
    "<form target='t' action='data:text/html,'><input type='email' id='i' required><input id='s' type='submit'></form>" +
    getDocFooter();
  let browser = await openNewTab(uri);

  let popupShownPromise = BrowserTestUtils.waitForEvent(
    gInvalidFormPopup,
    "popupshown"
  );
  let anchorRect = await clickChildElement(browser);
  await popupShownPromise;

  checkPopupShow(anchorRect);
  await checkChildFocus(
    browser,
    gInvalidFormPopup.firstElementChild.textContent
  );

  await new Promise((resolve, reject) => {
    EventUtils.sendString("a");
    executeSoon(function () {
      checkPopupShow(anchorRect);
      resolve();
    });
  });

  gBrowser.removeCurrentTab();
});

/**
 * In this test, we check that we can hide the popup by blurring the invalid
 * element.
 */
add_task(async function () {
  incrementTest();
  let uri =
    getDocHeader() +
    "<form target='t' action='data:text/html,'><input id='i' required><input id='s' type='submit'></form>" +
    getDocFooter();
  let browser = await openNewTab(uri);

  let popupShownPromise = BrowserTestUtils.waitForEvent(
    gInvalidFormPopup,
    "popupshown"
  );
  let anchorRect = await clickChildElement(browser);
  await popupShownPromise;

  checkPopupShow(anchorRect);
  await checkChildFocus(
    browser,
    gInvalidFormPopup.firstElementChild.textContent
  );

  let popupHiddenPromise = BrowserTestUtils.waitForEvent(
    gInvalidFormPopup,
    "popuphidden"
  );
  await blurChildElement(browser);
  await popupHiddenPromise;

  gBrowser.removeCurrentTab();
});

/**
 * In this test, we check that we can hide the popup by pressing TAB.
 */
add_task(async function () {
  incrementTest();
  let uri =
    getDocHeader() +
    "<form target='t' action='data:text/html,'><input id='i' required><input id='s' type='submit'></form>" +
    getDocFooter();
  let browser = await openNewTab(uri);

  let popupShownPromise = BrowserTestUtils.waitForEvent(
    gInvalidFormPopup,
    "popupshown"
  );
  let anchorRect = await clickChildElement(browser);
  await popupShownPromise;

  checkPopupShow(anchorRect);
  await checkChildFocus(
    browser,
    gInvalidFormPopup.firstElementChild.textContent
  );

  let popupHiddenPromise = BrowserTestUtils.waitForEvent(
    gInvalidFormPopup,
    "popuphidden"
  );
  EventUtils.synthesizeKey("KEY_Tab");
  await popupHiddenPromise;

  gBrowser.removeCurrentTab();
});

/**
 * In this test, we check that the popup will hide if we move to another tab.
 */
add_task(async function () {
  incrementTest();
  let uri =
    getDocHeader() +
    "<form target='t' action='data:text/html,'><input id='i' required><input id='s' type='submit'></form>" +
    getDocFooter();
  let browser1 = await openNewTab(uri);

  let popupShownPromise = BrowserTestUtils.waitForEvent(
    gInvalidFormPopup,
    "popupshown"
  );
  let anchorRect = await clickChildElement(browser1);
  await popupShownPromise;

  checkPopupShow(anchorRect);
  await checkChildFocus(
    browser1,
    gInvalidFormPopup.firstElementChild.textContent
  );

  let popupHiddenPromise = BrowserTestUtils.waitForEvent(
    gInvalidFormPopup,
    "popuphidden"
  );

  let browser2 = await openNewTab("data:text/html,<html></html>");
  await popupHiddenPromise;

  gBrowser.removeTab(gBrowser.getTabForBrowser(browser1));
  gBrowser.removeTab(gBrowser.getTabForBrowser(browser2));
});

/**
 * In this test, we check that the popup will hide if we navigate to another
 * page.
 */
add_task(async function () {
  incrementTest();
  let uri =
    getDocHeader() +
    "<form target='t' action='data:text/html,'><input id='i' required><input id='s' type='submit'></form>" +
    getDocFooter();
  let browser = await openNewTab(uri);

  let popupShownPromise = BrowserTestUtils.waitForEvent(
    gInvalidFormPopup,
    "popupshown"
  );
  let anchorRect = await clickChildElement(browser);
  await popupShownPromise;

  checkPopupShow(anchorRect);
  await checkChildFocus(
    browser,
    gInvalidFormPopup.firstElementChild.textContent
  );

  let popupHiddenPromise = BrowserTestUtils.waitForEvent(
    gInvalidFormPopup,
    "popuphidden"
  );
  BrowserTestUtils.startLoadingURIString(
    browser,
    "data:text/html,<div>hello!</div>"
  );
  await BrowserTestUtils.browserLoaded(browser);

  await popupHiddenPromise;

  gBrowser.removeCurrentTab();
});

/**
 * In this test, we check that the message is correctly updated when it changes.
 */
add_task(async function () {
  incrementTest();
  let uri =
    getDocHeader() +
    "<form target='t' action='data:text/html,'><input type='email' required id='i'><input id='s' type='submit'></form>" +
    getDocFooter();
  let browser = await openNewTab(uri);

  let popupShownPromise = BrowserTestUtils.waitForEvent(
    gInvalidFormPopup,
    "popupshown"
  );
  let anchorRect = await clickChildElement(browser);
  await popupShownPromise;

  checkPopupShow(anchorRect);
  await checkChildFocus(
    browser,
    gInvalidFormPopup.firstElementChild.textContent
  );

  let inputPromise = BrowserTestUtils.waitForContentEvent(browser, "input");
  EventUtils.sendString("f");
  await inputPromise;

  // Now, the element suffers from another error, the message should have
  // been updated.
  await new Promise((resolve, reject) => {
    // XXXndeakin This isn't really going to work when the content is another process
    executeSoon(function () {
      checkChildFocus(browser, gInvalidFormPopup.firstElementChild.textContent);
      resolve();
    });
  });

  gBrowser.removeCurrentTab();
});

/**
 * In this test, we reload the page while the form validation popup is visible. The validation
 * popup should hide.
 */
add_task(async function () {
  incrementTest();
  let uri =
    getDocHeader() +
    "<form target='t' action='data:text/html,'><input id='i' required><input id='s' type='submit'></form>" +
    getDocFooter();
  let browser = await openNewTab(uri);

  let popupShownPromise = BrowserTestUtils.waitForEvent(
    gInvalidFormPopup,
    "popupshown"
  );
  let anchorRect = await clickChildElement(browser);
  await popupShownPromise;

  checkPopupShow(anchorRect);
  await checkChildFocus(
    browser,
    gInvalidFormPopup.firstElementChild.textContent
  );

  let popupHiddenPromise = BrowserTestUtils.waitForEvent(
    gInvalidFormPopup,
    "popuphidden"
  );
  BrowserReloadSkipCache();
  await popupHiddenPromise;

  gBrowser.removeCurrentTab();
});
