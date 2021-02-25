/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Simple test page which writes the value of the cache-control header.
const TEST_URL = URL_ROOT + "sjs_cache_controle_header.sjs";

const { LocalizationHelper } = require("devtools/shared/l10n");
const L10N = new LocalizationHelper(
  "devtools/client/locales/toolbox.properties"
);

// Test that "forceReload" shorcuts send requests with the correct cache-control
// header value: no-cache.
add_task(async function() {
  await addTab(TEST_URL);
  const tab = gBrowser.selectedTab;

  info("Open the toolbox with the inspector selected");
  const toolbox = await gDevTools.showToolboxForTab(tab, {
    toolId: "inspector",
  });

  await testReload("toolbox.reload.key", toolbox, "max-age=0");
  await testReload("toolbox.reload2.key", toolbox, "max-age=0");
  await testReload("toolbox.forceReload.key", toolbox, "no-cache");
  await testReload("toolbox.forceReload2.key", toolbox, "no-cache");
});

async function testReload(shortcut, toolbox, expectedHeader) {
  info(`Reload with ${shortcut}`);
  await sendToolboxReloadShortcut(L10N.getStr(shortcut), toolbox);

  info("Retrieve the text content of the test page");
  const textContent = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    function() {
      return content.document.body.textContent;
    }
  );

  // See sjs_cache_controle_header.sjs
  is(
    textContent,
    "cache-control:" + expectedHeader,
    "cache-control header for the page request had the expected value"
  );
}
