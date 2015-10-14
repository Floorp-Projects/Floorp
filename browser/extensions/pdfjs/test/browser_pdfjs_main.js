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

  yield BrowserTestUtils.withNewTab({ gBrowser: gBrowser, url: TESTROOT + "file_pdfjs_test.pdf" },
    function* (newTabBrowser) {
      ok(gBrowser.isFindBarInitialized(), "Browser FindBar initialized!");

      //
      // Overall sanity tests
      //
      let [ viewer, PDFJS ] = yield ContentTask.spawn(newTabBrowser, null, function() {
        return [ content.document.querySelector('div#viewer') !== null,
                 'PDFJS' in content.wrappedJSObject ];
      });

      ok(viewer, "document content has viewer UI");
      ok(PDFJS, "window content has PDFJS object");

      //
      // Sidebar: open
      //
      let contains = yield ContentTask.spawn(newTabBrowser, null, function() {
        var sidebar = content.document.querySelector('button#sidebarToggle'),
            outerContainer = content.document.querySelector('div#outerContainer');

        sidebar.click();
        return outerContainer.classList.contains('sidebarOpen');
      });

      ok(contains, "sidebar opens on click");

      //
      // Sidebar: close
      //
      contains = yield ContentTask.spawn(newTabBrowser, null, function() {
        var sidebar = content.document.querySelector('button#sidebarToggle'),
            outerContainer = content.document.querySelector('div#outerContainer');

        sidebar.click();
        return outerContainer.classList.contains('sidebarOpen');
      });

      ok(!contains, "sidebar closes on click");

      //
      // Page change from prev/next buttons
      //
      let pageNumber = yield ContentTask.spawn(newTabBrowser, null, function() {
        var prevPage = content.document.querySelector('button#previous'),
            nextPage = content.document.querySelector('button#next');

        return content.document.querySelector('input#pageNumber').value;
      });

      is(parseInt(pageNumber), 1, 'initial page is 1');

      //
      // Bookmark button
      //
      let numBookmarks = yield ContentTask.spawn(newTabBrowser, null, function() {
        var viewBookmark = content.document.querySelector('a#viewBookmark');
        viewBookmark.click();
        return viewBookmark.href.length;
      });

      ok(numBookmarks > 0, "viewBookmark button has href");
    });
});
