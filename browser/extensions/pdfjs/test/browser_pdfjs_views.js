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
      yield waitForPdfJS(browser);

      yield ContentTask.spawn(browser, null, function* () {
        ok(content.document.querySelector('div#viewer'), "document content has viewer UI");
        ok('PDFJS' in content.wrappedJSObject, "window content has PDFJS object");

        //open sidebar
        var sidebar = content.document.querySelector('button#sidebarToggle');
        var outerContainer = content.document.querySelector('div#outerContainer');

        sidebar.click();
        ok(outerContainer.classList.contains('sidebarOpen'), 'sidebar opens on click');

        // check that thumbnail view is open
        var thumbnailView = content.document.querySelector('div#thumbnailView');
        var outlineView = content.document.querySelector('div#outlineView');

        is(thumbnailView.getAttribute('class'), null, 'Initial view is thumbnail view');
        is(outlineView.getAttribute('class'), 'hidden', 'Outline view is hidden initially');

        //switch to outline view
        var viewOutlineButton = content.document.querySelector('button#viewOutline');
        viewOutlineButton.click();

        is(thumbnailView.getAttribute('class'), 'hidden', 'Thumbnail view is hidden when outline is selected');
        is(outlineView.getAttribute('class'), '', 'Outline view is visible when selected');

        //switch back to thumbnail view
        var viewThumbnailButton = content.document.querySelector('button#viewThumbnail');
        viewThumbnailButton.click();

        is(thumbnailView.getAttribute('class'), '', 'Thumbnail view is visible when selected');
        is(outlineView.getAttribute('class'), 'hidden', 'Outline view is hidden when thumbnail is selected');

        sidebar.click();

        var viewer = content.wrappedJSObject.PDFViewerApplication;
        yield viewer.close();
      });
    });
});
