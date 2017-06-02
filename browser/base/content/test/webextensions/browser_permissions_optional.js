"use strict";
add_task(async function test_request_permissions_without_prompt() {
  async function pageScript() {
    const NO_PROMPT_PERM = "activeTab";
    window.addEventListener("keypress", async () => {
      let permGranted = await browser.permissions.request(
        {permissions: [NO_PROMPT_PERM]});
      browser.test.assertTrue(permGranted,
                              `${NO_PROMPT_PERM} permission was granted.`);
      let perms = await browser.permissions.getAll();
      browser.test.assertTrue(perms.permissions.includes(NO_PROMPT_PERM),
                              `${NO_PROMPT_PERM} permission exists.`)
      browser.test.sendMessage("permsGranted");
      }, {once: true});
    browser.test.sendMessage("pageReady");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.sendMessage("ready", browser.runtime.getURL("page.html"));
    },
    files: {
      "page.html": `<html><head><script src="page.js"></script></head></html>`,
      "page.js": pageScript,
    },
    manifest: {
      "optional_permissions": [
        "activeTab"
      ],
    },
  });
  await extension.startup();

  let url = await extension.awaitMessage("ready");
  let win = window.open(url);
  await extension.awaitMessage("pageReady");

  let winutils = SpecialPowers.getDOMWindowUtils(win);
  winutils.sendKeyEvent("keypress", KeyEvent.DOM_VK_A, 0, 0);
  await extension.awaitMessage("permsGranted");

  await extension.unload();
});
