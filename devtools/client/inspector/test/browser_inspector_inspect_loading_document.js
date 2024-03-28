/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Fix blank inspector bug when opening DevTools on a webpage with a document
// where document.write is called without ever calling document.close.
// Such a document remains forever in the "loading" readyState. See Bug 1765760.

const TEST_URL =
  `data:text/html;charset=utf-8,` +
  encodeURIComponent(`
  <!DOCTYPE html>
  <html lang="en">
    <body>
      <div id=outsideframe></div>
      <script type="text/javascript">
        const iframe = document.createElement("iframe");
        iframe.src = "about:blank";
        iframe.addEventListener('load', () => {
          iframe.contentDocument.write('<div id=inframe>inframe</div>');
        }, true);
        document.body.appendChild(iframe);
      </script>
    </body>
  </html>
`);

add_task(async function testSlowLoadingFrame() {
  const loadingTab = BrowserTestUtils.addTab(gBrowser, TEST_URL);
  gBrowser.selectedTab = loadingTab;

  // Note: we cannot use `await addTab` here because the iframe never finishes
  // loading. But we still want to wait for the frame to reach the loading state
  // and to display a test element so that we can reproduce the initial issue
  // fixed by Bug 1765760.
  info("Wait for the loading iframe to be ready to test");
  await TestUtils.waitForCondition(async () => {
    try {
      return await ContentTask.spawn(gBrowser.selectedBrowser, {}, () => {
        const iframe = content.document.querySelector("iframe");
        return (
          iframe?.contentDocument.readyState === "loading" &&
          iframe.contentDocument.getElementById("inframe")
        );
      });
    } catch (e) {
      return false;
    }
  });

  const { inspector } = await openInspector();

  info("Check the markup view displays the loading iframe successfully");
  await assertMarkupViewAsTree(
    `
    body
      div id="outsideframe"
      script!ignore-children
      iframe
        #document
          html
            head
            body
              div id="inframe"`,
    "body",
    inspector
  );
});

add_task(async function testSlowLoadingDocument() {
  info("Create a test server serving a slow document");
  const httpServer = createTestHTTPServer();
  httpServer.registerContentType("html", "text/html");

  // This promise allows to block serving the complete document from the test.
  let unblockRequest;
  const onRequestUnblocked = new Promise(r => (unblockRequest = r));

  httpServer.registerPathHandler(`/`, async function (request, response) {
    response.processAsync();
    response.setStatusLine(request.httpVersion, 200, "OK");

    // Split the page content in 2 parts:
    // - opening body tag and the "#start" div will be returned immediately
    // - "#end" div and closing body tag are blocked on a promise.
    const page_start = "<body><div id='start'>start</div>";
    const page_end = "<div id='end'>end</div></body>";
    const page = page_start + page_end;

    response.setHeader("Content-Type", "text/html; charset=utf-8", false);
    response.setHeader("Content-Length", page.length + "", false);
    response.write(page_start);

    await onRequestUnblocked;

    response.write(page_end);
    response.finish();
  });

  const port = httpServer.identity.primaryPort;
  const TEST_URL_2 = `http://localhost:${port}/`;

  // Same as in the other task, we cannot wait for the full load.
  info("Open a new tab on TEST_URL_2 and select it");
  const loadingTab = BrowserTestUtils.addTab(gBrowser, TEST_URL_2);
  gBrowser.selectedTab = loadingTab;

  info("Wait for the #start div to be available in the document");
  await TestUtils.waitForCondition(async () => {
    try {
      return await ContentTask.spawn(gBrowser.selectedBrowser, {}, () =>
        content.document.getElementById("start")
      );
    } catch (e) {
      return false;
    }
  });

  const { inspector } = await openInspector();

  info("Check that the inspector is not blank and only shows the #start div");
  await assertMarkupViewAsTree(
    `
    body
      div id="start"`,
    "body",
    inspector
  );

  // Navigate to about:blank to clean the state.
  await navigateTo("about:blank");

  await navigateTo(TEST_URL_2, { waitForLoad: false });
  info("Wait for the #start div to be available as a markupview container");
  await TestUtils.waitForCondition(async () => {
    const nodeFront = await getNodeFront("#start", inspector);
    return nodeFront && getContainerForNodeFront(nodeFront, inspector);
  });

  info("Check that the inspector is not blank and only shows the #start div");
  await assertMarkupViewAsTree(
    `
    body
      div id="start"`,
    "body",
    inspector
  );

  info("Unblock the document request");
  unblockRequest();

  info("Wait for the #end div to be available as a markupview container");
  await TestUtils.waitForCondition(async () => {
    const nodeFront = await getNodeFront("#end", inspector);
    return nodeFront && getContainerForNodeFront(nodeFront, inspector);
  });

  info("Check that the inspector will ultimately show the #end div");
  await assertMarkupViewAsTree(
    `
    body
      div id="start"
      div id="end"`,
    "body",
    inspector
  );
});
