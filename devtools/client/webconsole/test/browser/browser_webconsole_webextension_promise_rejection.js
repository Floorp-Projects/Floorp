/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that an uncaught promise rejection from a content script
// is reported to the tabs' webconsole.

"use strict";

const TEST_URI =
  "https://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-blank.html";

add_task(async function() {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: [TEST_URI],
          js: ["content-script.js"],
        },
      ],
    },

    files: {
      "content-script.js": function() {
        Promise.reject("abc");
      },
    },
  });

  await extension.startup();

  const hud = await openNewTabAndConsole(TEST_URI);
  await waitFor(() =>
    findMessage(hud, "uncaught exception: abc", ".message.error")
  );

  await extension.unload();
});
