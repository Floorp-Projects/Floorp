/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function installAndStartExtension() {
  function contentScript() {
    window.addEventListener(
      "load",
      () => {
        document.documentElement.setAttribute(
          "data-initKeyEvent-in-contentscript",
          typeof window.KeyboardEvent.prototype.initKeyEvent
        );
      },
      { capture: true, once: true }
    );
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          js: ["content_script.js"],
          matches: ["<all_urls>"],
          run_at: "document_start",
        },
      ],
    },
    files: {
      "content_script.js": contentScript,
    },
  });

  await extension.startup();

  return extension;
}

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.keyboardevent.init_key_event.enabled", false],
      ["dom.keyboardevent.init_key_event.enabled_in_addons", true],
    ],
  });

  const extension = await installAndStartExtension();
  await BrowserTestUtils.withNewTab(
    "http://example.com/browser/dom/events/test/file_keyboard_event_init_key_event_enabled_in_contentscript.html",
    async browser => {
      info("Waiting for the test result...");
      await SpecialPowers.spawn(browser, [], () => {
        Assert.equal(
          content.document.documentElement.getAttribute(
            "data-initKeyEvent-before"
          ),
          "undefined",
          "KeyboardEvent.initKeyEvent shouldn't be available in web-content"
        );
        Assert.equal(
          content.document.documentElement.getAttribute(
            "data-initKeyEvent-in-contentscript"
          ),
          "function",
          "KeyboardEvent.initKeyEvent should be available in contentscript"
        );
        Assert.equal(
          content.document.documentElement.getAttribute(
            "data-initKeyEvent-after"
          ),
          "undefined",
          "KeyboardEvent.initKeyEvent shouldn't be available in web-content even after contentscript accesses it"
        );
      });
    }
  );

  await extension.unload();
});
