/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/Promise.jsm", this);

const RELATIVE_DIR = "browser/extensions/pdfjs/test/";
const TESTROOT = "http://example.com/browser/" + RELATIVE_DIR;

const TESTS = [
  {
    action: {
      selector: "button#zoomIn",
      event: "click"
    },
    expectedZoom: 1, // 1 - zoom in
    message: "Zoomed in using the '+' (zoom in) button"
  },

  {
    action: {
      selector: "button#zoomOut",
      event: "click"
    },
    expectedZoom: -1, // -1 - zoom out
    message: "Zoomed out using the '-' (zoom out) button"
  },

  {
    action: {
      keyboard: true,
      event: "+"
    },
    expectedZoom: 1, // 1 - zoom in
    message: "Zoomed in using the CTRL++ keys"
  },

  {
    action: {
      keyboard: true,
      event: "-"
    },
    expectedZoom: -1, // -1 - zoom out
    message: "Zoomed out using the CTRL+- keys"
  },

  {
    action: {
      selector: "select#scaleSelect",
      index: 5,
      event: "change"
    },
    expectedZoom: -1, // -1 - zoom out
    message: "Zoomed using the zoom picker"
  }
];

let initialWidth; // the initial width of the PDF document
var previousWidth; // the width of the PDF document at previous step/test

function test() {
  var tab;
  let handlerService = Cc["@mozilla.org/uriloader/handler-service;1"]
                       .getService(Ci.nsIHandlerService);
  let mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
  let handlerInfo = mimeService.getFromTypeAndExtension('application/pdf', 'pdf');

  // Make sure pdf.js is the default handler.
  is(handlerInfo.alwaysAskBeforeHandling, false,
     'pdf handler defaults to always-ask is false');
  is(handlerInfo.preferredAction, Ci.nsIHandlerInfo.handleInternally,
    'pdf handler defaults to internal');

  info('Pref action: ' + handlerInfo.preferredAction);

  waitForExplicitFinish();
  registerCleanupFunction(function() {
    gBrowser.removeTab(tab);
  });

  tab = gBrowser.selectedTab = gBrowser.addTab(TESTROOT + "file_pdfjs_test.pdf");
  var newTabBrowser = gBrowser.getBrowserForTab(tab);

  newTabBrowser.addEventListener("load", function eventHandler() {
    newTabBrowser.removeEventListener("load", eventHandler, true);

    var document = newTabBrowser.contentDocument,
        window = newTabBrowser.contentWindow;

    // Runs tests after all 'load' event handlers have fired off
    window.addEventListener("documentload", function() {
      initialWidth = parseInt(document.querySelector("div#pageContainer1").style.width);
      previousWidth = initialWidth;
      runTests(document, window, finish);
    }, false, true);
  }, true);
}

function runTests(document, window, callback) {
  // check that PDF is opened with internal viewer
  ok(document.querySelector('div#viewer'), "document content has viewer UI");
  ok('PDFJS' in window.wrappedJSObject, "window content has PDFJS object");

  // Start the zooming tests after the document is loaded
  waitForDocumentLoad(document).then(function () {
    zoomPDF(document, window, TESTS.shift(), finish);
  });
}

function waitForDocumentLoad(document) {
  var deferred = Promise.defer();
  var interval = setInterval(function () {
    if (document.querySelector("div#pageContainer1") != null){
      clearInterval(interval);
      deferred.resolve();
    }
  }, 500);

  return deferred.promise;
}

function zoomPDF(document, window, test, endCallback) {
  var renderedPage;

  document.addEventListener("pagerendered", function onPageRendered(e) {
    if(e.detail.pageNumber !== 1) {
      return;
    }

    document.removeEventListener("pagerendered", onPageRendered, true);

    var pageZoomScale = document.querySelector('select#scaleSelect');

    // The zoom value displayed in the zoom select
    var zoomValue = pageZoomScale.options[pageZoomScale.selectedIndex].innerHTML;

    let pageContainer = document.querySelector('div#pageContainer1');
    let actualWidth  = parseInt(pageContainer.style.width);

    // the actual zoom of the PDF document
    let computedZoomValue = parseInt(((actualWidth/initialWidth).toFixed(2))*100) + "%";
    is(computedZoomValue, zoomValue, "Content has correct zoom");

    // Check that document zooms in the expected way (in/out)
    let zoom = (actualWidth - previousWidth) * test.expectedZoom;
    ok(zoom > 0, test.message);

    // Go to next test (if there is any) or finish
    var nextTest = TESTS.shift();
    if (nextTest) {
      previousWidth = actualWidth;
      zoomPDF(document, window, nextTest, endCallback);
    }
    else
      endCallback();
  }, true);

  // We zoom using an UI element
  if (test.action.selector) {
    // Get the element and trigger the action for changing the zoom
    var el = document.querySelector(test.action.selector);
    ok(el, "Element '" + test.action.selector + "' has been found");

    if (test.action.index){
      el.selectedIndex = test.action.index;
    }

    // Dispatch the event for changing the zoom
    el.dispatchEvent(new Event(test.action.event));
  }
  // We zoom using keyboard
  else {
    // Simulate key press
    EventUtils.synthesizeKey(test.action.event, { ctrlKey: true });
  }
}
