/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_bug_1906132() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {},

    manifest: {
      browser_action: {
        default_popup: "popup.html",
        default_area: "navbar",
        browser_style: false,
      },
    },

    files: {
      "popup.html": `
<!doctype html>
<style>
:root, body, #root {
  margin: 0;
  height: 100%;
  min-height: 100%;
  width: auto;
  min-width: 320px;
}
:root {
  background: red;
}
body {
  background: green;
}
#content {
  height: 200px;
}
</style>
<div id="root">
  <div id="content">
    some content
  </div>
</div>
`,
    },
  });

  await extension.startup();

  clickBrowserAction(extension);
  let browser = await awaitExtensionPanel(extension);

  is(browser.getBoundingClientRect().height, 200, "Should be the right height");

  await closeBrowserAction(extension);
  await extension.unload();
});
