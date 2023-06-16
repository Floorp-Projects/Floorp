/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_srcUrl_of_redirected_image() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["contextMenus"],
    },
    background() {
      browser.contextMenus.onClicked.addListener(info => {
        browser.test.assertEq(
          "before_redir",
          info.menuItemId,
          "Expected menu item matched for pre-redirect URL"
        );
        browser.test.assertEq("image", info.mediaType, "Expected mediaType");
        browser.test.assertEq(
          "http://mochi.test:8888/browser/browser/components/extensions/test/browser/redirect_to.sjs?ctxmenu-image.png",
          info.srcUrl,
          "Expected srcUrl"
        );
        browser.test.sendMessage("contextMenus_onClicked");
      });
      browser.contextMenus.create({
        id: "before_redir",
        title: "MyMenu",
        targetUrlPatterns: ["*://*/*redirect_to.sjs*"],
      });
      browser.contextMenus.create(
        {
          id: "after_redir",
          title: "MyMenu",
          targetUrlPatterns: ["*://*/*/ctxmenu-image.png*"],
        },
        () => {
          browser.test.sendMessage("menus_setup");
        }
      );
    },
  });
  await extension.startup();
  await extension.awaitMessage("menus_setup");

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context_with_redirect.html",
    },
    async browser => {
      // Verify that the image has been loaded, which implies that the redirect has
      // been followed.
      let imgWidth = await SpecialPowers.spawn(browser, [], () => {
        let img = content.document.getElementById("img_that_redirects");
        return img.naturalWidth;
      });
      is(imgWidth, 100, "Image has been loaded");

      let menu = await openContextMenu("#img_that_redirects");
      let items = menu.getElementsByAttribute("label", "MyMenu");
      is(items.length, 1, "Only one menu item should have been matched");

      await closeExtensionContextMenu(items[0]);
      await extension.awaitMessage("contextMenus_onClicked");
    }
  );

  await extension.unload();
});
