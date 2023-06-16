/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI_ORG = `https://example.org/document-builder.sjs?html=<meta charset=utf8></meta>
<script>
      console.log("early message on org page");
</script><body>`;
const TEST_URI_COM = TEST_URI_ORG.replace(/org/g, "com");

add_task(async function () {
  info("Add a tab and open the console");
  const tab = await addTab("about:robots");
  const hud = await openConsole(tab);

  {
    await navigateTo(TEST_URI_ORG);

    // Wait for some time in order to let a chance to have duplicated message
    // and catch such regression
    await wait(1000);

    info("wait until the ORG message is displayed");
    await checkUniqueMessageExists(
      hud,
      "early message on org page",
      ".console-api"
    );
  }

  {
    await navigateTo(TEST_URI_COM);

    // Wait for some time in order to let a chance to have duplicated message
    // and catch such regression
    await wait(1000);

    info("wait until the COM message is displayed");
    await checkUniqueMessageExists(
      hud,
      "early message on com page",
      ".console-api"
    );
  }
});
