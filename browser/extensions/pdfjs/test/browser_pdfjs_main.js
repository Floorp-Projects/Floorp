/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const RELATIVE_DIR = "browser/extensions/pdfjs/test/";
const TESTROOT = "http://example.com/browser/" + RELATIVE_DIR;

add_task(async function test() {
  let mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
  let handlerInfo = mimeService.getFromTypeAndExtension(
    "application/pdf",
    "pdf"
  );

  // Make sure pdf.js is the default handler.
  is(
    handlerInfo.alwaysAskBeforeHandling,
    false,
    "pdf handler defaults to always-ask is false"
  );
  is(
    handlerInfo.preferredAction,
    Ci.nsIHandlerInfo.handleInternally,
    "pdf handler defaults to internal"
  );

  info("Pref action: " + handlerInfo.preferredAction);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function(newTabBrowser) {
      await waitForPdfJS(newTabBrowser, TESTROOT + "file_pdfjs_test.pdf");

      await ContentTask.spawn(newTabBrowser, null, async function() {
        // Overall sanity tests
        Assert.ok(
          content.document.querySelector("div#viewer"),
          "document content has viewer UI"
        );

        // Sidebar: open
        var sidebar = content.document.querySelector("button#sidebarToggle"),
          outerContainer = content.document.querySelector("div#outerContainer");

        sidebar.click();
        Assert.ok(
          outerContainer.classList.contains("sidebarOpen"),
          "sidebar opens on click"
        );

        // Sidebar: close
        sidebar.click();
        Assert.ok(
          !outerContainer.classList.contains("sidebarOpen"),
          "sidebar closes on click"
        );

        // Verify that initial page is 1
        var pgNumber = content.document.querySelector("input#pageNumber").value;
        Assert.equal(parseInt(pgNumber, 10), 1, "initial page is 1");

        // Bookmark button
        var viewBookmark = content.document.querySelector("a#viewBookmark");
        viewBookmark.click();

        Assert.ok(viewBookmark.href.length > 0, "viewBookmark button has href");

        var viewer = content.wrappedJSObject.PDFViewerApplication;
        await viewer.close();
      });
    }
  );
});
