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

add_task(async function() {
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
