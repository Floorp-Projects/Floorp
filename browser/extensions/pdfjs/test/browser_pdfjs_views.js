/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const RELATIVE_DIR = "browser/extensions/pdfjs/test/";
const TESTROOT = "http://example.com/browser/" + RELATIVE_DIR;

add_task(function* test() {
  let handlerService = Cc["@mozilla.org/uriloader/handler-service;1"].getService(Ci.nsIHandlerService);
  let mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
  let handlerInfo = mimeService.getFromTypeAndExtension('application/pdf', 'pdf');

  // Make sure pdf.js is the default handler.
  is(handlerInfo.alwaysAskBeforeHandling, false, 'pdf handler defaults to always-ask is false');
  is(handlerInfo.preferredAction, Ci.nsIHandlerInfo.handleInternally, 'pdf handler defaults to internal');

  info('Pref action: ' + handlerInfo.preferredAction);

  yield BrowserTestUtils.withNewTab({ gBrowser, url: TESTROOT + "file_pdfjs_test.pdf" },
    function* (browser) {
      // check that PDF is opened with internal viewer
      let [ viewer, pdfjs ] = yield ContentTask.spawn(browser, null, function* () {
        yield new Promise((resolve) => {
          content.addEventListener("documentload", function documentload() {
            content.removeEventListener("documentload", documentload);
            resolve();
          });
        });
        return [ content.document.querySelector('div#viewer') !== null,
                 'PDFJS' in content.wrappedJSObject ]
      });

      ok(viewer, "document content has viewer UI");
      ok(pdfjs, "window content has PDFJS object");

      //open sidebar
      let sidebarOpen = yield ContentTask.spawn(browser, null, function* () {
        var sidebar = content.document.querySelector('button#sidebarToggle');
        var outerContainer = content.document.querySelector('div#outerContainer');

        sidebar.click();
        return outerContainer.classList.contains('sidebarOpen');
      });

      ok(sidebarOpen, 'sidebar opens on click');

      // check that thumbnail view is open
      let [ thumbnailClass, outlineClass ] = yield ContentTask.spawn(browser, null, function* () {
        var thumbnailView = content.document.querySelector('div#thumbnailView');
        var outlineView = content.document.querySelector('div#outlineView');

        return [ thumbnailView.getAttribute('class'),
                 outlineView.getAttribute('class') ]
      });

      is(thumbnailClass, null, 'Initial view is thumbnail view');
      is(outlineClass, 'hidden', 'Outline view is hidden initially');

      //switch to outline view
      [ thumbnailClass, outlineClass ] = yield ContentTask.spawn(browser, null, function* () {
        var viewOutlineButton = content.document.querySelector('button#viewOutline');
        viewOutlineButton.click();

        var thumbnailView = content.document.querySelector('div#thumbnailView');
        var outlineView = content.document.querySelector('div#outlineView');

        return [ thumbnailView.getAttribute('class'),
                 outlineView.getAttribute('class') ];
      });

      is(thumbnailClass, 'hidden', 'Thumbnail view is hidden when outline is selected');
      is(outlineClass, '', 'Outline view is visible when selected');

      //switch back to thumbnail view
      [ thumbnailClass, outlineClass ] = yield ContentTask.spawn(browser, null, function* () {
          var viewThumbnailButton = content.document.querySelector('button#viewThumbnail');
          viewThumbnailButton.click();

          var thumbnailView = content.document.querySelector('div#thumbnailView');
          var outlineView = content.document.querySelector('div#outlineView');

          let rval = [ thumbnailView.getAttribute('class'),
                       outlineView.getAttribute('class') ];

          var sidebar = content.document.querySelector('button#sidebarToggle');
          sidebar.click();

          return rval;
      });

      is(thumbnailClass, '', 'Thumbnail view is visible when selected');
      is(outlineClass, 'hidden', 'Outline view is hidden when thumbnail is selected');
    });
});
