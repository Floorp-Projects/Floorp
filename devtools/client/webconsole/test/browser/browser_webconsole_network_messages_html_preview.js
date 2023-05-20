/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

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
function addBaseHtmlElements(body) {
  return `<html><head><meta charset="utf8"></head><body>${body}</body></html>`;
}

// This first page asserts we can redirect to another URL, even if JS happen to be executed
const FETCH_CONTENT_1 = addBaseHtmlElements(
  `Fetch 1<script>window.parent.location.href = "${REDIRECT_URL}";</script>`
);
// This second page asserts that JS is disabled
const FETCH_CONTENT_2 = addBaseHtmlElements(
  `Fetch 2<script>document.write("JS activated")</script>`
);
// This third page asserts responses with line breaks
const FETCH_CONTENT_3 = addBaseHtmlElements(`
  <a href="#" id="link1">link1</a>
  <a href="#" id="link2">link2</a>
`);
// This fourth page asserts that links and forms are disabled
const FETCH_CONTENT_4 = addBaseHtmlElements(
  `Fetch 3<a href="${REDIRECT_URL}">link</a> -- <form action="${REDIRECT_URL}"><input type="submit"></form>`
);

// Use fetch in order to prevent actually running this code in the test page
const TEST_HTML = addBaseHtmlElements(`HTML<script>
  fetch("${BASE_URL}fetch-1.html");
  fetch("${BASE_URL}fetch-2.html");
  fetch("${BASE_URL}fetch-3.html");
  fetch("${BASE_URL}fetch-4.html");
</script>`);
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
httpServer.registerPathHandler("/fetch-4.html", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(FETCH_CONTENT_4);
});
httpServer.registerPathHandler("/redirect.html", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write("Redirected!");
});

/**
 * Main test for checking HTTP logs in the Console panel.
 */
add_task(async function task() {
  // Display network requests
  pushPref("devtools.webconsole.filter.net", true);
  pushPref("devtools.webconsole.filter.netxhr", true);
  // Enable async events so that clicks on preview iframe's links are correctly
  // going through the parent process which is meant to cancel any mousedown.
  await pushPref("test.events.async.enabled", true);

  const hud = await openNewTabAndConsole(TEST_URL);
  await reloadBrowser();

  await expandNetworkRequestAndWaitForHtmlView({
    hud,
    url: "doc-html-preview.html",
    expectedHtml: TEST_HTML,
  });
  await expandNetworkRequestAndWaitForHtmlView({
    hud,
    url: "fetch-1.html",
    expectedHtml: FETCH_CONTENT_1,
  });
  await expandNetworkRequestAndWaitForHtmlView({
    hud,
    url: "fetch-2.html",
    expectedHtml: FETCH_CONTENT_2,
  });
  await expandNetworkRequestAndWaitForHtmlView({
    hud,
    url: "fetch-3.html",
    expectedHtml: FETCH_CONTENT_3,
  });

  info("Try to click on the link and submit the form");
  await expandNetworkRequestAndWaitForHtmlView({
    hud,
    url: "fetch-4.html",
    expectedHtml: FETCH_CONTENT_4,
  });
});

async function expandNetworkRequestAndWaitForHtmlView({
  hud,
  url,
  expectedHtml,
}) {
  info(`Wait for ${url} message`);

  const node = await waitFor(() => findMessageByType(hud, url, ".network"));
  ok(node, `Network message found for ${url}`);

  info("Expand the message and open the response tab");
  const onPayloadReady = waitForPayloadReady(hud);
  await expandXhrMessage(node);
  await onPayloadReady;
  node.querySelector("#response-tab").click();

  info("Wait for the iframe to be rendered and loaded");
  const iframe = await waitFor(() =>
    node.querySelector("#response-panel .html-preview iframe")
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

  is(
    iframe.browsingContext.currentWindowGlobal.isInProcess,
    false,
    "The preview is loaded in a content process"
  );

  await SpecialPowers.spawn(
    iframe.browsingContext,
    [expectedHtml],
    async function (_expectedHtml) {
      is(
        content.document.documentElement.outerHTML,
        _expectedHtml,
        "iframe has the expected HTML"
      );
    }
  );

  return iframe;
}

async function waitForPayloadReady(hud) {
  return hud.ui.once("network-request-payload-ready");
}

function expandXhrMessage(node) {
  info(
    "Click on XHR message and wait for the network detail panel to be displayed"
  );
  node.querySelector(".url").click();
  return waitFor(
    () => node.querySelector(".network-info"),
    "Wait for .network-info to be rendered"
  );
}
