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

  yield BrowserTestUtils.withNewTab({ gBrowser: gBrowser, url: "about:blank" },
    function* (newTabBrowser) {
      yield waitForPdfJS(newTabBrowser, TESTROOT + "file_pdfjs_test.pdf");

      ok(gBrowser.isFindBarInitialized(), "Browser FindBar initialized!");

      yield ContentTask.spawn(newTabBrowser, null, function* () {
        //
        // Overall sanity tests
        //
        Assert.ok(content.document.querySelector('div#viewer'), "document content has viewer UI");
        Assert.ok('PDFJS' in content.wrappedJSObject, "window content has PDFJS object");

        //
        // Sidebar: open
        //
        var sidebar = content.document.querySelector('button#sidebarToggle'),
            outerContainer = content.document.querySelector('div#outerContainer');

        sidebar.click();
        Assert.ok(outerContainer.classList.contains('sidebarOpen'), "sidebar opens on click");

        //
        // Sidebar: close
        //
        sidebar.click();
        Assert.ok(!outerContainer.classList.contains('sidebarOpen'), "sidebar closes on click");

        //
        // Page change from prev/next buttons
        //
        var prevPage = content.document.querySelector('button#previous'),
            nextPage = content.document.querySelector('button#next');

        var pgNumber = content.document.querySelector('input#pageNumber').value;
        Assert.equal(parseInt(pgNumber, 10), 1, "initial page is 1");

        //
        // Bookmark button
        //
        var viewBookmark = content.document.querySelector('a#viewBookmark');
        viewBookmark.click();

        Assert.ok(viewBookmark.href.length > 0, "viewBookmark button has href");

        var viewer = content.wrappedJSObject.PDFViewerApplication;
        yield viewer.close();
      });
    });
});
