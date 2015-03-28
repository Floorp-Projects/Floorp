/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/Promise.jsm", this);

const RELATIVE_DIR = "browser/extensions/pdfjs/test/";
const TESTROOT = "http://example.com/browser/" + RELATIVE_DIR;

const PDF_OUTLINE_ITEMS = 17;
const TESTS = [
  {
    action: {
      selector: "button#next",
      event: "click"
    },
    expectedPage: 2,
    message: "navigated to next page using NEXT button"

  },
  {
    action: {
      selector: "button#previous",
      event: "click"
    },
    expectedPage: 1,
    message: "navigated to previous page using PREV button"
  },
  {
    action: {
      selector: "button#next",
      event: "click"
    },
    expectedPage: 2,
    message: "navigated to next page using NEXT button"
  },
  {
    action: {
      selector: "input#pageNumber",
      value: 1,
      event: "change"
    },
    expectedPage: 1,
    message: "navigated to first page using pagenumber"
  },
  {
    action: {
      selector: "#thumbnailView a:nth-child(4)",
      event: "click"
    },
    expectedPage: 4,
    message: "navigated to 4th page using thumbnail view"
  },
  {
    action: {
      selector: "#thumbnailView a:nth-child(2)",
      event: "click"
    },
    expectedPage: 2,
    message: "navigated to 2nd page using thumbnail view"
  },
  {
    action: {
      selector: "#viewer",
      event: "keydown",
      keyCode: 36
    },
    expectedPage: 1,
    message: "navigated to 1st page using 'home' key"
  },
  {
    action: {
      selector: "#viewer",
      event: "keydown",
      keyCode: 34
    },
    expectedPage: 2,
    message: "navigated to 2nd page using 'Page Down' key"
  },
  {
    action: {
      selector: "#viewer",
      event: "keydown",
      keyCode: 33
    },
    expectedPage: 1,
    message: "navigated to 1st page using 'Page Up' key"
  },
  {
    action: {
      selector: "#viewer",
      event: "keydown",
      keyCode: 39
    },
    expectedPage: 2,
    message: "navigated to 2nd page using 'right' key"
  },
  {
    action: {
      selector: "#viewer",
      event: "keydown",
      keyCode: 37
    },
    expectedPage: 1,
    message: "navigated to 1st page using 'left' key"
  },
  {
    action: {
      selector: "#viewer",
      event: "keydown",
      keyCode: 35
    },
    expectedPage: 5,
    message: "navigated to last page using 'home' key"
  },
  {
    action: {
      selector: ".outlineItem:nth-child(1) a",
      event: "click"
    },
    expectedPage: 1,
    message: "navigated to 1st page using outline view"
  },
  {
    action: {
      selector: ".outlineItem:nth-child(" + PDF_OUTLINE_ITEMS + ") a",
      event: "click"
    },
    expectedPage: 4,
    message: "navigated to 4th page using outline view"
  },
  {
    action: {
      selector: "input#pageNumber",
      value: 5,
      event: "change"
    },
    expectedPage: 5,
    message: "navigated to 5th page using pagenumber"
  }
];

function test() {
  var tab;

  let mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
  let handlerInfo = mimeService.getFromTypeAndExtension('application/pdf', 'pdf');

  // Make sure pdf.js is the default handler.
  is(handlerInfo.alwaysAskBeforeHandling, false, 'pdf handler defaults to always-ask is false');
  is(handlerInfo.preferredAction, Ci.nsIHandlerInfo.handleInternally, 'pdf handler defaults to internal');

  info('Pref action: ' + handlerInfo.preferredAction);

  waitForExplicitFinish();
  registerCleanupFunction(function() {
    gBrowser.removeTab(tab);
  });

  tab = gBrowser.addTab(TESTROOT + "file_pdfjs_test.pdf");
  gBrowser.selectedTab = tab;

  var newTabBrowser = gBrowser.getBrowserForTab(tab);
  newTabBrowser.addEventListener("load", function eventHandler() {
    newTabBrowser.removeEventListener("load", eventHandler, true);

    var document = newTabBrowser.contentDocument,
        window = newTabBrowser.contentWindow;

    // Runs tests after all 'load' event handlers have fired off
    window.addEventListener("documentload", function() {
      runTests(document, window, function () {
        var pageNumber = document.querySelector('input#pageNumber');
        is(pageNumber.value, pageNumber.max, "Document is left on the last page");
        finish();
      });
    }, false, true);
  }, true);
}

function runTests(document, window, finish) {
  // Check if PDF is opened with internal viewer
  ok(document.querySelector('div#viewer'), "document content has viewer UI");
  ok('PDFJS' in window.wrappedJSObject, "window content has PDFJS object");

  // Wait for outline items, the start the navigation actions
  waitForOutlineItems(document).then(function () {
    // The key navigation has to happen in page-fit, otherwise it won't scroll
    // trough a complete page
    setZoomToPageFit(document).then(function () {
      runNextTest(document, window, finish);
    }, function () {
      ok(false, "Current scale has been set to 'page-fit'");
      finish();
    });
  }, function () {
    ok(false, "Outline items have been found");
    finish();
  });
}

/**
 * As the page changes asynchronously, we have to wait for the event after
 * we trigger the action so we will be at the expected page number after each action
 *
 * @param document
 * @param window
 * @param test
 * @param callback
 */
function runNextTest(document, window, endCallback) {
  var test = TESTS.shift(),
      deferred = Promise.defer(),
      pageNumber = document.querySelector('input#pageNumber');

  // Add an event-listener to wait for page to change, afterwards resolve the promise
  var timeout = window.setTimeout(() => deferred.reject(), 5000);
  window.addEventListener('pagechange', function pageChange() {
    if (pageNumber.value == test.expectedPage) {
      window.removeEventListener('pagechange', pageChange);
      window.clearTimeout(timeout);
      deferred.resolve(pageNumber.value);
    }
  });

  // Get the element and trigger the action for changing the page
  var el = document.querySelector(test.action.selector);
  ok(el, "Element '" + test.action.selector + "' has been found");

  // The value option is for input case
  if (test.action.value)
    el.value = test.action.value;

  // Dispatch the event for changing the page
  if (test.action.event == "keydown") {
    var ev = document.createEvent("KeyboardEvent");
        ev.initKeyEvent("keydown", true, true, null, false, false, false, false,
                        test.action.keyCode, 0);
    el.dispatchEvent(ev);
  }
  else {
    var ev = new Event(test.action.event);
  }
  el.dispatchEvent(ev);


  // When the promise gets resolved we call the next test if there are any left
  // or else we call the final callback which will end the test
  deferred.promise.then(function (pgNumber) {
    is(pgNumber, test.expectedPage, test.message);

    if (TESTS.length)
      runNextTest(document, window, endCallback);
    else
      endCallback();
  }, function () {
    ok(false, "Test '" + test.message + "' failed with timeout.");
    endCallback();
  });
}

/**
 * Outline Items gets appended to the document latter on we have to
 * wait for them before we start to navigate though document
 *
 * @param document
 * @returns {deferred.promise|*}
 */
function waitForOutlineItems(document) {
  var deferred = Promise.defer();
  document.addEventListener("outlineloaded", function outlineLoaded(evt) {
    document.removeEventListener("outlineloaded", outlineLoaded);
    var outlineCount = evt.detail.outlineCount;

    if (document.querySelectorAll(".outlineItem").length === outlineCount) {
      deferred.resolve();
    } else {
      deferred.reject();
    }
  });

  return deferred.promise;
}

/**
 * The key navigation has to happen in page-fit, otherwise it won't scroll
 * trough a complete page
 *
 * @param document
 * @returns {deferred.promise|*}
 */
function setZoomToPageFit(document) {
  var deferred = Promise.defer();
  document.addEventListener("pagerendered", function onZoom(e) {
    document.removeEventListener("pagerendered", onZoom);
    document.querySelector("#viewer").click();
    deferred.resolve();
  });

  var select = document.querySelector("select#scaleSelect");
  select.selectedIndex = 2;
  select.dispatchEvent(new Event("change"));

  return deferred.promise;
}

