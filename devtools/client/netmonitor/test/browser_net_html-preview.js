/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if different response content types are handled correctly.
 */

const httpServer = createTestHTTPServer();
httpServer.registerContentType("html", "text/html");

const BASE_URL = `http://localhost:${httpServer.identity.primaryPort}/`;

const REDIRECT_URL = BASE_URL + "redirect.html";

// In all content previewed as HTML we ensure using proper html, head and body in order to
// prevent having them added by the <browser> when loaded as a preview.
//
// This first page asserts we can redirect to another URL, even if JS happen to be executed
const FETCH_CONTENT_1 = `<html><head></head><body>Fetch 1<script>window.parent.location.href = "${REDIRECT_URL}";</script></body></html>`;
// This second page asserts that JS is disabled
const FETCH_CONTENT_2 = `<html><head></head><body>Fetch 2<script>document.write("JS activated")</script></body></html>`;
// This last page asserts that links and forms are disabled
const FETCH_CONTENT_3 = `<html><head></head><body>Fetch 3<a href="${REDIRECT_URL}">link</a> -- <form action="${REDIRECT_URL}"><input type="submit"></form></body></html>`;

// Use fetch in order to prevent actually running this code in the test page
const TEST_HTML = `<html><head></head><body>HTML<script>
  fetch("${BASE_URL}fetch-1.html");
  fetch("${BASE_URL}fetch-2.html");
  fetch("${BASE_URL}fetch-3.html");
</script></body></html>`;
const TEST_URL = BASE_URL + "doc-html-preview.html";

httpServer.registerPathHandler(
  "/doc-html-preview.html",
  (request, response) => {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(TEST_HTML);
  }
);
httpServer.registerPathHandler("/fetch-1.html", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(FETCH_CONTENT_1);
});
httpServer.registerPathHandler("/fetch-2.html", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(FETCH_CONTENT_2);
});
httpServer.registerPathHandler("/fetch-3.html", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(FETCH_CONTENT_3);
});
httpServer.registerPathHandler("/redirect.html", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write("Redirected!");
});

add_task(async function() {
  // Enable async events so that clicks on preview iframe's links are correctly
  // going through the parent process which is meant to cancel any mousedown.
  await pushPref("test.events.async.enabled", true);

  const { monitor } = await initNetMonitor(TEST_URL, { requestCount: 3 });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  const onNetworkEvent = waitForNetworkEvents(monitor, 3);
  await reloadBrowser();
  await onNetworkEvent;

  // The new lines are stripped when using outerHTML to retrieve HTML content of the preview iframe
  await selectIndexAndWaitForHtmlView(0, TEST_HTML.replace(/\n/g, ""));
  await selectIndexAndWaitForHtmlView(1, FETCH_CONTENT_1);
  await selectIndexAndWaitForHtmlView(2, FETCH_CONTENT_2);
  const previewIframe = await selectIndexAndWaitForHtmlView(3, FETCH_CONTENT_3);

  info("Try to click on the link and submit the form");
  await SpecialPowers.spawn(previewIframe.browsingContext, [], async function(
    expectedHtml
  ) {
    EventUtils.synthesizeMouseAtCenter(
      content.document.querySelector("a"),
      {},
      content
    );
    EventUtils.synthesizeMouseAtCenter(
      content.document.querySelector("form"),
      {},
      content
    );
  });

  info("Wait for some time to let a chance for the link/form to navigate");
  await wait(1000);

  info("Verify that the content did not change");
  await SpecialPowers.spawn(
    previewIframe.browsingContext,
    [FETCH_CONTENT_3],
    async function(expectedHtml) {
      is(
        content.document.documentElement.outerHTML,
        expectedHtml,
        "Verify that link and form are both disabled and the HTML content stays the same"
      );
    }
  );

  await teardown(monitor);

  async function selectIndexAndWaitForHtmlView(index, expectedHtmlPreview) {
    info(`Select the request #${index}`);
    const onResponseContent = monitor.panelWin.api.once(
      TEST_EVENTS.RECEIVED_RESPONSE_CONTENT
    );
    store.dispatch(Actions.selectRequestByIndex(index));

    info("Open the Response tab");
    document.querySelector("#response-tab").click();

    const [iframe] = await waitForDOM(
      document,
      "#response-panel .html-preview iframe"
    );

    // <xul:iframe type=content remote=true> don't emit "load" event.
    // And SpecialPowsers.spawn throws if kept running during a page load.
    // So poll for the end of the iframe load...
    await waitFor(async () => {
      // Note that if spawn executes early, the iframe may not yet be loading
      // and would throw for the reason mentioned in previous comment.
      try {
        const rv = await SpecialPowers.spawn(iframe.browsingContext, [], () => {
          return content.document.readyState == "complete";
        });
        return rv;
      } catch (e) {
        return false;
      }
    });

    info("Wait for response content to be loaded");
    await onResponseContent;

    is(
      iframe.browsingContext.currentWindowGlobal.isInProcess,
      false,
      "The preview is loaded in a content process"
    );

    await SpecialPowers.spawn(
      iframe.browsingContext,
      [expectedHtmlPreview],
      async function(expectedHtml) {
        is(
          content.document.documentElement.outerHTML,
          expectedHtml,
          "The text shown in the iframe is incorrect for the html request."
        );
      }
    );

    return iframe;
  }
});
