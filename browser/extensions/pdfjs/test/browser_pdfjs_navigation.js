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

add_task(function* test() {
  let mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
  let handlerInfo = mimeService.getFromTypeAndExtension('application/pdf', 'pdf');

  // Make sure pdf.js is the default handler.
  is(handlerInfo.alwaysAskBeforeHandling, false, 'pdf handler defaults to always-ask is false');
  is(handlerInfo.preferredAction, Ci.nsIHandlerInfo.handleInternally, 'pdf handler defaults to internal');

  info('Pref action: ' + handlerInfo.preferredAction);

  yield BrowserTestUtils.withNewTab({ gBrowser, url: TESTROOT + "file_pdfjs_test.pdf" },
    function* (newTabBrowser) {
      yield waitForPdfJS(newTabBrowser);

      yield ContentTask.spawn(newTabBrowser, null, function* () {
        // Check if PDF is opened with internal viewer
        ok(content.document.querySelector('div#viewer'), "document content has viewer UI");
        ok('PDFJS' in content.wrappedJSObject, "window content has PDFJS object");
      });

      yield ContentTask.spawn(newTabBrowser, null, contentSetUp);

      yield Task.spawn(runTests(newTabBrowser));

      yield ContentTask.spawn(newTabBrowser, null, function*() {
        let pageNumber = content.document.querySelector('input#pageNumber');
        is(pageNumber.value, pageNumber.max, "Document is left on the last page");
      });
    });
});

function* contentSetUp() {
  /**
   * Outline Items gets appended to the document later on we have to
   * wait for them before we start to navigate though document
   *
   * @param document
   * @returns {deferred.promise|*}
   */
  function waitForOutlineItems(document) {
    return new Promise((resolve, reject) => {
      document.addEventListener("outlineloaded", function outlineLoaded(evt) {
        document.removeEventListener("outlineloaded", outlineLoaded);
        var outlineCount = evt.detail.outlineCount;

        if (document.querySelectorAll(".outlineItem").length === outlineCount) {
          resolve();
        } else {
          reject();
        }
      });
    });
  }

  /**
   * The key navigation has to happen in page-fit, otherwise it won't scroll
   * through a complete page
   *
   * @param document
   * @returns {deferred.promise|*}
   */
  function setZoomToPageFit(document) {
    return new Promise((resolve) => {
      document.addEventListener("pagerendered", function onZoom(e) {
        document.removeEventListener("pagerendered", onZoom);
        document.querySelector("#viewer").click();
        resolve();
      });

      var select = document.querySelector("select#scaleSelect");
      select.selectedIndex = 2;
      select.dispatchEvent(new Event("change"));
    });
  }

  yield waitForOutlineItems(content.document);
  yield setZoomToPageFit(content.document);
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
function* runTests(browser) {
  yield ContentTask.spawn(browser, TESTS, function* (TESTS) {
    let window = content;
    let document = window.document;

    for (let test of TESTS) {
      let deferred = {};
      deferred.promise = new Promise((resolve, reject) => {
        deferred.resolve = resolve;
        deferred.reject = reject;
      });

      let pageNumber = document.querySelector('input#pageNumber');

      // Add an event-listener to wait for page to change, afterwards resolve the promise
      let timeout = window.setTimeout(() => deferred.reject(), 5000);
      window.addEventListener('pagechange', function pageChange() {
        if (pageNumber.value == test.expectedPage) {
          window.removeEventListener('pagechange', pageChange);
          window.clearTimeout(timeout);
          deferred.resolve(+pageNumber.value);
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

      let pgNumber = yield deferred.promise;
      is(pgNumber, test.expectedPage, test.message);
    }

    var viewer = content.wrappedJSObject.PDFViewerApplication;
    yield viewer.close();
  });
}
