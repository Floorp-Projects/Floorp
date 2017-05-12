"use strict";

var { SocialService } = Cu.import("resource:///modules/SocialService.jsm", {});

let contextMenu;
let hasPocket = Services.prefs.getBoolPref("extensions.pocket.enabled");
let hasContainers = Services.prefs.getBoolPref("privacy.userContext.enabled");

// A social share provider
let manifest = {
  name: "provider 1",
  origin: "https://example.com",
  iconURL: "https://example.com/browser/browser/base/content/test/general/moz.png",
  shareURL: "https://example.com/browser/browser/base/content/test/social/share.html"
};

add_task(async function test_setup() {
  const example_base = "http://example.com/browser/browser/base/content/test/contextMenu/";
  const url = example_base + "subtst_contextmenu_webext.html";
  await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  const chrome_base = "chrome://mochitests/content/browser/browser/base/content/test/general/";
  const contextmenu_common = chrome_base + "contextmenu_common.js";
  /* import-globals-from ../general/contextmenu_common.js */
  Services.scriptloader.loadSubScript(contextmenu_common, this);

  // Enable social sharing functions in the browser, so the context menu item is shown.
  CustomizableUI.addWidgetToArea("social-share-button", CustomizableUI.AREA_NAVBAR);

  await new Promise((resolve) => SocialService.addProvider(manifest, resolve));
  ok(SocialShare.shareButton && !SocialShare.shareButton.disabled, "Sharing is enabled");
});

add_task(async function test_link() {
  // gets hidden for this case.
  await test_contextmenu("#link",
    ["context-openlinkintab", true,
       ...(hasContainers ? ["context-openlinkinusercontext-menu", true] : []),
       // We need a blank entry here because the containers submenu is
       // dynamically generated with no ids.
       ...(hasContainers ? ["", null] : []),
       "context-openlink",      true,
       "context-openlinkprivate", true,
       "---",                   null,
       "context-savelink",      true,
       "context-copylink",      true,
       "context-searchselect",  true]);
});

add_task(async function test_video() {
  await test_contextmenu("#video",
  ["context-media-play",         null,
   "context-media-mute",         null,
   "context-media-playbackrate", null,
       ["context-media-playbackrate-050x", null,
        "context-media-playbackrate-100x", null,
        "context-media-playbackrate-125x", null,
        "context-media-playbackrate-150x", null,
        "context-media-playbackrate-200x", null], null,
   "context-media-loop",         null,
   "context-media-showcontrols", null,
   "context-video-fullscreen",   null,
   "---",                        null,
   "context-viewvideo",          null,
   "context-copyvideourl",       null,
   "---",                        null,
   "context-savevideo",          null,
   "context-sharevideo",         false,
   "context-video-saveimage",    null,
   "context-sendvideo",          null,
   "context-castvideo",          null,
     [], null
  ]);
});

add_task(async function test_cleanup() {
  lastElementSelector = null;
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await new Promise((resolve) => {
    return SocialService.disableProvider(manifest.origin, resolve);
  });
});
