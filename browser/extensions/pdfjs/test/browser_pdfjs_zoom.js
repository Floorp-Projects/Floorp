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
      keyCode: 61,
      event: "+"
    },
    expectedZoom: 1, // 1 - zoom in
    message: "Zoomed in using the CTRL++ keys"
  },

  {
    action: {
      keyboard: true,
      keyCode: 109,
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

add_task(function* test() {
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

  yield BrowserTestUtils.withNewTab({ gBrowser, url: TESTROOT + "file_pdfjs_test.pdf" },
    function* (newTabBrowser) {
      let initialWidth = 0; // the initial width of the PDF document
      let previousWidth = 0; // the width of the PDF document at previous step/test

      initialWidth = previousWidth =
        yield ContentTask.spawn(newTabBrowser, null, function* () {
          // Runs tests after all 'load' event handlers have fired off
          return yield new Promise((resolve) => {
            content.addEventListener("documentload", function loaded() {
              content.removeEventListener("documentload", loaded);
              resolve(parseInt(content.document.querySelector("div#pageContainer1").style.width));
            }, false, true);
          });
        });

      // Basic tests.
      let [ viewer, pdfjs ] = yield ContentTask.spawn(newTabBrowser, null, function* () {
        // check that PDF is opened with internal viewer
        return [ content.document.querySelector('div#viewer') !== null,
                 'PDFJS' in content.wrappedJSObject ];
      });

      ok(viewer, "document content has viewer UI");
      ok(pdfjs, "window content has PDFJS object");

      for (let test of TESTS) {
        yield ContentTask.spawn(newTabBrowser, test, function* (test) {
          let document = content.document;

          function waitForRender() {
            return new Promise((resolve) => {
              document.addEventListener("pagerendered", function onPageRendered(e) {
                if(e.detail.pageNumber !== 1) {
                  return;
                }

                document.removeEventListener("pagerendered", onPageRendered, true);
                resolve();
              }, true);
            });
          }

          // We zoom using an UI element
          var ev;
          if (test.action.selector) {
            // Get the element and trigger the action for changing the zoom
            var el = document.querySelector(test.action.selector);

            if (test.action.index){
              el.selectedIndex = test.action.index;
            }

            // Dispatch the event for changing the zoom
            ev = new Event(test.action.event);
          }
          // We zoom using keyboard
          else {
            // Simulate key press
            ev = new content.KeyboardEvent("keydown",
                                           { key: test.action.event,
                                             keyCode: test.action.keyCode,
                                             ctrlKey: true });
            el = content;
          }

          el.dispatchEvent(ev);
          yield waitForRender();
        });

        if (test.action.selector) {
          info("Element '" + test.action.selector + "' has been found");
        }

        let { actualWidth, zoomValue } = yield ContentTask.spawn(newTabBrowser, null, function* () {
          var pageZoomScale = content.document.querySelector('select#scaleSelect');

          // The zoom value displayed in the zoom select
          var zoomValue = pageZoomScale.options[pageZoomScale.selectedIndex].innerHTML;

          let pageContainer = content.document.querySelector('div#pageContainer1');
          let actualWidth = parseInt(pageContainer.style.width);

          return { actualWidth, zoomValue };
        });

        // the actual zoom of the PDF document
        let computedZoomValue = parseInt(((actualWidth/initialWidth).toFixed(2))*100) + "%";
        is(computedZoomValue, zoomValue, "Content has correct zoom");

        // Check that document zooms in the expected way (in/out)
        let zoom = (actualWidth - previousWidth) * test.expectedZoom;
        ok(zoom > 0, test.message);

        previousWidth = actualWidth;
      }
    });
});
