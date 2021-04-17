"use strict";

let contextMenu;
let LOGIN_FILL_ITEMS = ["---", null, "manage-saved-logins", true];
let NAVIGATION_ITEMS =
  AppConstants.platform == "macosx"
    ? [
        "context-back",
        false,
        "context-forward",
        false,
        "context-reload",
        true,
        "---",
        null,
        "context-bookmarkpage",
        true,
      ]
    : [
        "context-navigation",
        null,
        [
          "context-back",
          false,
          "context-forward",
          false,
          "context-reload",
          true,
          "context-bookmarkpage",
          true,
        ],
        null,
        "---",
        null,
      ];
let hasPocket = Services.prefs.getBoolPref("extensions.pocket.enabled");
let hasContainers =
  Services.prefs.getBoolPref("privacy.userContext.enabled") &&
  ContextualIdentityService.getPublicIdentities().length;

const example_base =
  "http://example.com/browser/browser/base/content/test/contextMenu/";
const chrome_base =
  "chrome://mochitests/content/browser/browser/base/content/test/contextMenu/";
const head_base =
  "chrome://mochitests/content/browser/browser/base/content/test/contextMenu/";

/* import-globals-from contextmenu_common.js */
Services.scriptloader.loadSubScript(
  chrome_base + "contextmenu_common.js",
  this
);

function getThisFrameSubMenu(base_menu) {
  if (AppConstants.NIGHTLY_BUILD) {
    let osPidItem = ["context-frameOsPid", false];
    base_menu = base_menu.concat(osPidItem);
  }
  return base_menu;
}

add_task(async function init() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.separatePrivateDefault.ui.enabled", true],
      ["extensions.screenshots.disabled", false],
    ],
  });
});

// Below are test cases for XUL element
add_task(async function test_xul_text_link_label() {
  let url = chrome_base + "subtst_contextmenu_xul.xhtml";

  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url,
    waitForLoad: true,
    waitForStateStop: true,
  });

  await test_contextmenu("#test-xul-text-link-label", [
    "context-openlinkintab",
    true,
    ...(hasContainers ? ["context-openlinkinusercontext-menu", true] : []),
    // We need a blank entry here because the containers submenu is
    // dynamically generated with no ids.
    ...(hasContainers ? ["", null] : []),
    "context-openlink",
    true,
    "context-openlinkprivate",
    true,
    "---",
    null,
    "context-bookmarklink",
    true,
    "context-savelink",
    true,
    ...(hasPocket ? ["context-savelinktopocket", true] : []),
    "context-copylink",
    true,
    "---",
    null,
    "context-searchselect",
    true,
    "context-searchselect-private",
    true,
  ]);

  // Clean up so won't affect HTML element test cases.
  lastElementSelector = null;
  gBrowser.removeCurrentTab();
});

// Below are test cases for HTML element.

add_task(async function test_setup_html() {
  let url = example_base + "subtst_contextmenu.html";

  await SpecialPowers.pushPrefEnv({
    set: [["dom.menuitem.enabled", true]],
  });

  await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    let doc = content.document;
    let audioIframe = doc.querySelector("#test-audio-in-iframe");
    // media documents always use a <video> tag.
    let audio = audioIframe.contentDocument.querySelector("video");
    let videoIframe = doc.querySelector("#test-video-in-iframe");
    let video = videoIframe.contentDocument.querySelector("video");

    audio.loop = true;
    audio.src = "audio.ogg";
    video.loop = true;
    video.src = "video.ogg";

    let awaitPause = ContentTaskUtils.waitForEvent(audio, "pause");
    await ContentTaskUtils.waitForCondition(
      () => !audio.paused,
      "Making sure audio is playing before calling pause"
    );
    audio.pause();
    await awaitPause;

    awaitPause = ContentTaskUtils.waitForEvent(video, "pause");
    await ContentTaskUtils.waitForCondition(
      () => !video.paused,
      "Making sure video is playing before calling pause"
    );
    video.pause();
    await awaitPause;
  });
});

let plainTextItems;
add_task(async function test_plaintext() {
  await test_contextmenu("#test-text", [
    ...NAVIGATION_ITEMS,
    "context-savepage",
    true,
    ...(hasPocket ? ["context-pocket", true] : []),
    "context-selectall",
    true,
    "---",
    null,
    "context-take-screenshot",
    true,
    "---",
    null,
    "context-viewsource",
    true,
  ]);
});

add_task(async function test_link() {
  await test_contextmenu("#test-link", [
    "context-openlinkintab",
    true,
    ...(hasContainers ? ["context-openlinkinusercontext-menu", true] : []),
    // We need a blank entry here because the containers submenu is
    // dynamically generated with no ids.
    ...(hasContainers ? ["", null] : []),
    "context-openlink",
    true,
    "context-openlinkprivate",
    true,
    "---",
    null,
    "context-bookmarklink",
    true,
    "context-savelink",
    true,
    ...(hasPocket ? ["context-savelinktopocket", true] : []),
    "context-copylink",
    true,
    "---",
    null,
    "context-searchselect",
    true,
    "context-searchselect-private",
    true,
  ]);
});

add_task(async function test_link_in_shadow_dom() {
  await test_contextmenu(
    "#shadow-host",
    [
      "context-openlinkintab",
      true,
      ...(hasContainers ? ["context-openlinkinusercontext-menu", true] : []),
      // We need a blank entry here because the containers submenu is
      // dynamically generated with no ids.
      ...(hasContainers ? ["", null] : []),
      "context-openlink",
      true,
      "context-openlinkprivate",
      true,
      "---",
      null,
      "context-bookmarklink",
      true,
      "context-savelink",
      true,
      ...(hasPocket ? ["context-savelinktopocket", true] : []),
      "context-copylink",
      true,
      "---",
      null,
      "context-searchselect",
      true,
      "context-searchselect-private",
      true,
    ],
    {
      offsetX: 6,
      offsetY: 6,
    }
  );
});

add_task(async function test_mailto() {
  await test_contextmenu("#test-mailto", [
    "context-copyemail",
    true,
    "---",
    null,
    "context-searchselect",
    true,
    "context-searchselect-private",
    true,
  ]);
});

add_task(async function test_image() {
  for (let selector of ["#test-image", "#test-svg-image"]) {
    await test_contextmenu(
      selector,
      [
        "context-viewimage",
        true,
        "context-saveimage",
        true,
        "context-copyimage-contents",
        true,
        "context-copyimage",
        true,
        "context-sendimage",
        true,
        ...(Services.prefs.getBoolPref("browser.menu.showViewImageInfo", false)
          ? ["context-viewimageinfo", true]
          : []),
        "---",
        null,
        "context-setDesktopBackground",
        true,
      ],
      {
        onContextMenuShown() {
          is(
            typeof gContextMenu.imageInfo.height,
            "number",
            "Should have height"
          );
          is(
            typeof gContextMenu.imageInfo.width,
            "number",
            "Should have width"
          );
        },
      }
    );
  }
});

add_task(async function test_canvas() {
  await test_contextmenu("#test-canvas", [
    "context-viewimage",
    true,
    "context-saveimage",
    true,
    "---",
    null,
    "context-selectall",
    true,
    "---",
    null,
    "context-take-screenshot",
    true,
  ]);
});

add_task(async function test_video_ok() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.videocontrols.picture-in-picture.enabled", true]],
  });

  await test_contextmenu("#test-video-ok", [
    "context-media-play",
    true,
    "context-media-mute",
    true,
    "context-media-playbackrate",
    null,
    [
      "context-media-playbackrate-050x",
      true,
      "context-media-playbackrate-100x",
      true,
      "context-media-playbackrate-125x",
      true,
      "context-media-playbackrate-150x",
      true,
      "context-media-playbackrate-200x",
      true,
    ],
    null,
    "context-media-loop",
    true,
    "context-video-fullscreen",
    true,
    "context-media-hidecontrols",
    true,
    "---",
    null,
    "context-viewvideo",
    true,
    "context-video-pictureinpicture",
    true,
    "---",
    null,
    "context-video-saveimage",
    true,
    "context-savevideo",
    true,
    "context-copyvideourl",
    true,
    "context-sendvideo",
    true,
  ]);

  await SpecialPowers.popPrefEnv();

  await SpecialPowers.pushPrefEnv({
    set: [["media.videocontrols.picture-in-picture.enabled", false]],
  });

  await test_contextmenu("#test-video-ok", [
    "context-media-play",
    true,
    "context-media-mute",
    true,
    "context-media-playbackrate",
    null,
    [
      "context-media-playbackrate-050x",
      true,
      "context-media-playbackrate-100x",
      true,
      "context-media-playbackrate-125x",
      true,
      "context-media-playbackrate-150x",
      true,
      "context-media-playbackrate-200x",
      true,
    ],
    null,
    "context-media-loop",
    true,
    "context-video-fullscreen",
    true,
    "context-media-hidecontrols",
    true,
    "---",
    null,
    "context-viewvideo",
    true,
    "---",
    null,
    "context-video-saveimage",
    true,
    "context-savevideo",
    true,
    "context-copyvideourl",
    true,
    "context-sendvideo",
    true,
  ]);

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_audio_in_video() {
  await test_contextmenu("#test-audio-in-video", [
    "context-media-play",
    true,
    "context-media-mute",
    true,
    "context-media-playbackrate",
    null,
    [
      "context-media-playbackrate-050x",
      true,
      "context-media-playbackrate-100x",
      true,
      "context-media-playbackrate-125x",
      true,
      "context-media-playbackrate-150x",
      true,
      "context-media-playbackrate-200x",
      true,
    ],
    null,
    "context-media-loop",
    true,
    "context-media-showcontrols",
    true,
    "---",
    null,
    "context-saveaudio",
    true,
    "context-copyaudiourl",
    true,
    "context-sendaudio",
    true,
  ]);
});

add_task(async function test_video_bad() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.videocontrols.picture-in-picture.enabled", true]],
  });

  await test_contextmenu("#test-video-bad", [
    "context-media-play",
    false,
    "context-media-mute",
    false,
    "context-media-playbackrate",
    null,
    [
      "context-media-playbackrate-050x",
      false,
      "context-media-playbackrate-100x",
      false,
      "context-media-playbackrate-125x",
      false,
      "context-media-playbackrate-150x",
      false,
      "context-media-playbackrate-200x",
      false,
    ],
    null,
    "context-media-loop",
    true,
    "context-video-fullscreen",
    false,
    "context-media-hidecontrols",
    false,
    "---",
    null,
    "context-viewvideo",
    true,
    "---",
    null,
    "context-video-saveimage",
    false,
    "context-savevideo",
    true,
    "context-copyvideourl",
    true,
    "context-sendvideo",
    true,
  ]);

  await SpecialPowers.popPrefEnv();

  await SpecialPowers.pushPrefEnv({
    set: [["media.videocontrols.picture-in-picture.enabled", false]],
  });

  await test_contextmenu("#test-video-bad", [
    "context-media-play",
    false,
    "context-media-mute",
    false,
    "context-media-playbackrate",
    null,
    [
      "context-media-playbackrate-050x",
      false,
      "context-media-playbackrate-100x",
      false,
      "context-media-playbackrate-125x",
      false,
      "context-media-playbackrate-150x",
      false,
      "context-media-playbackrate-200x",
      false,
    ],
    null,
    "context-media-loop",
    true,
    "context-video-fullscreen",
    false,
    "context-media-hidecontrols",
    false,
    "---",
    null,
    "context-viewvideo",
    true,
    "---",
    null,
    "context-video-saveimage",
    false,
    "context-savevideo",
    true,
    "context-copyvideourl",
    true,
    "context-sendvideo",
    true,
  ]);

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_video_bad2() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.videocontrols.picture-in-picture.enabled", true]],
  });

  await test_contextmenu("#test-video-bad2", [
    "context-media-play",
    false,
    "context-media-mute",
    false,
    "context-media-playbackrate",
    null,
    [
      "context-media-playbackrate-050x",
      false,
      "context-media-playbackrate-100x",
      false,
      "context-media-playbackrate-125x",
      false,
      "context-media-playbackrate-150x",
      false,
      "context-media-playbackrate-200x",
      false,
    ],
    null,
    "context-media-loop",
    true,
    "context-video-fullscreen",
    false,
    "context-media-hidecontrols",
    false,
    "---",
    null,
    "context-viewvideo",
    false,
    "---",
    null,
    "context-video-saveimage",
    false,
    "context-savevideo",
    false,
    "context-copyvideourl",
    false,
    "context-sendvideo",
    false,
  ]);

  await SpecialPowers.popPrefEnv();

  await SpecialPowers.pushPrefEnv({
    set: [["media.videocontrols.picture-in-picture.enabled", false]],
  });

  await test_contextmenu("#test-video-bad2", [
    "context-media-play",
    false,
    "context-media-mute",
    false,
    "context-media-playbackrate",
    null,
    [
      "context-media-playbackrate-050x",
      false,
      "context-media-playbackrate-100x",
      false,
      "context-media-playbackrate-125x",
      false,
      "context-media-playbackrate-150x",
      false,
      "context-media-playbackrate-200x",
      false,
    ],
    null,
    "context-media-loop",
    true,
    "context-video-fullscreen",
    false,
    "context-media-hidecontrols",
    false,
    "---",
    null,
    "context-viewvideo",
    false,
    "---",
    null,
    "context-video-saveimage",
    false,
    "context-savevideo",
    false,
    "context-copyvideourl",
    false,
    "context-sendvideo",
    false,
  ]);

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_iframe() {
  await test_contextmenu("#test-iframe", [
    ...NAVIGATION_ITEMS,
    "context-savepage",
    true,
    ...(hasPocket ? ["context-pocket", true] : []),
    "context-selectall",
    true,
    "---",
    null,
    "frame",
    null,
    getThisFrameSubMenu([
      "context-showonlythisframe",
      true,
      "context-openframeintab",
      true,
      "context-openframe",
      true,
      "---",
      null,
      "context-reloadframe",
      true,
      "---",
      null,
      "context-bookmarkframe",
      true,
      "context-saveframe",
      true,
      "---",
      null,
      "context-printframe",
      true,
      "---",
      null,
      "context-viewframesource",
      true,
      "context-viewframeinfo",
      true,
    ]),
    null,
    "---",
    null,
    "context-viewsource",
    true,
  ]);
});

add_task(async function test_video_in_iframe() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.videocontrols.picture-in-picture.enabled", true]],
  });

  await test_contextmenu("#test-video-in-iframe", [
    "context-media-play",
    true,
    "context-media-mute",
    true,
    "context-media-playbackrate",
    null,
    [
      "context-media-playbackrate-050x",
      true,
      "context-media-playbackrate-100x",
      true,
      "context-media-playbackrate-125x",
      true,
      "context-media-playbackrate-150x",
      true,
      "context-media-playbackrate-200x",
      true,
    ],
    null,
    "context-media-loop",
    true,
    "context-video-fullscreen",
    true,
    "context-media-hidecontrols",
    true,
    "---",
    null,
    "context-viewvideo",
    true,
    "context-video-pictureinpicture",
    true,
    "---",
    null,
    "context-video-saveimage",
    true,
    "context-savevideo",
    true,
    "context-copyvideourl",
    true,
    "context-sendvideo",
    true,
    "---",
    null,
    "frame",
    null,
    getThisFrameSubMenu([
      "context-showonlythisframe",
      true,
      "context-openframeintab",
      true,
      "context-openframe",
      true,
      "---",
      null,
      "context-reloadframe",
      true,
      "---",
      null,
      "context-bookmarkframe",
      true,
      "context-saveframe",
      true,
      "---",
      null,
      "context-printframe",
      true,
      "---",
      null,
      "context-viewframeinfo",
      true,
    ]),
    null,
  ]);

  await SpecialPowers.popPrefEnv();

  await SpecialPowers.pushPrefEnv({
    set: [["media.videocontrols.picture-in-picture.enabled", false]],
  });

  await test_contextmenu("#test-video-in-iframe", [
    "context-media-play",
    true,
    "context-media-mute",
    true,
    "context-media-playbackrate",
    null,
    [
      "context-media-playbackrate-050x",
      true,
      "context-media-playbackrate-100x",
      true,
      "context-media-playbackrate-125x",
      true,
      "context-media-playbackrate-150x",
      true,
      "context-media-playbackrate-200x",
      true,
    ],
    null,
    "context-media-loop",
    true,
    "context-video-fullscreen",
    true,
    "context-media-hidecontrols",
    true,
    "---",
    null,
    "context-viewvideo",
    true,
    "---",
    null,
    "context-video-saveimage",
    true,
    "context-savevideo",
    true,
    "context-copyvideourl",
    true,
    "context-sendvideo",
    true,
    "---",
    null,
    "frame",
    null,
    getThisFrameSubMenu([
      "context-showonlythisframe",
      true,
      "context-openframeintab",
      true,
      "context-openframe",
      true,
      "---",
      null,
      "context-reloadframe",
      true,
      "---",
      null,
      "context-bookmarkframe",
      true,
      "context-saveframe",
      true,
      "---",
      null,
      "context-printframe",
      true,
      "---",
      null,
      "context-viewframeinfo",
      true,
    ]),
    null,
  ]);

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_audio_in_iframe() {
  await test_contextmenu("#test-audio-in-iframe", [
    "context-media-play",
    true,
    "context-media-mute",
    true,
    "context-media-playbackrate",
    null,
    [
      "context-media-playbackrate-050x",
      true,
      "context-media-playbackrate-100x",
      true,
      "context-media-playbackrate-125x",
      true,
      "context-media-playbackrate-150x",
      true,
      "context-media-playbackrate-200x",
      true,
    ],
    null,
    "context-media-loop",
    true,
    "---",
    null,
    "context-saveaudio",
    true,
    "context-copyaudiourl",
    true,
    "context-sendaudio",
    true,
    "---",
    null,
    "frame",
    null,
    getThisFrameSubMenu([
      "context-showonlythisframe",
      true,
      "context-openframeintab",
      true,
      "context-openframe",
      true,
      "---",
      null,
      "context-reloadframe",
      true,
      "---",
      null,
      "context-bookmarkframe",
      true,
      "context-saveframe",
      true,
      "---",
      null,
      "context-printframe",
      true,
      "---",
      null,
      "context-viewframeinfo",
      true,
    ]),
    null,
  ]);
});

add_task(async function test_image_in_iframe() {
  await test_contextmenu("#test-image-in-iframe", [
    "context-viewimage",
    true,
    "context-saveimage",
    true,
    "context-copyimage-contents",
    true,
    "context-copyimage",
    true,
    "context-sendimage",
    true,
    ...(Services.prefs.getBoolPref("browser.menu.showViewImageInfo", false)
      ? ["context-viewimageinfo", true]
      : []),
    "---",
    null,
    "context-setDesktopBackground",
    true,
    "---",
    null,
    "frame",
    null,
    getThisFrameSubMenu([
      "context-showonlythisframe",
      true,
      "context-openframeintab",
      true,
      "context-openframe",
      true,
      "---",
      null,
      "context-reloadframe",
      true,
      "---",
      null,
      "context-bookmarkframe",
      true,
      "context-saveframe",
      true,
      "---",
      null,
      "context-printframe",
      true,
      "---",
      null,
      "context-viewframeinfo",
      true,
    ]),
    null,
  ]);
});

add_task(async function test_pdf_viewer_in_iframe() {
  await test_contextmenu(
    "#test-pdf-viewer-in-frame",
    [
      ...NAVIGATION_ITEMS,
      "context-savepage",
      true,
      ...(hasPocket ? ["context-pocket", true] : []),
      "context-selectall",
      true,
      "---",
      null,
      "frame",
      null,
      getThisFrameSubMenu([
        "context-showonlythisframe",
        true,
        "context-openframeintab",
        true,
        "context-openframe",
        true,
        "---",
        null,
        "context-reloadframe",
        true,
        "---",
        null,
        "context-bookmarkframe",
        true,
        "context-saveframe",
        true,
        "---",
        null,
        "context-printframe",
        true,
        "---",
        null,
        "context-viewframeinfo",
        true,
      ]),
      null,
      "---",
      null,
      "context-viewsource",
      true,
    ],
    {
      shiftkey: true,
    }
  );
});

add_task(async function test_textarea() {
  // Disabled since this is seeing spell-check-enabled
  // instead of spell-add-dictionaries-main
  todo(false, "spell checker tests are failing, bug 1246296");

  /*
  yield test_contextmenu("#test-textarea",
    ["context-undo",                false,
     "context-redo",                false,
     "---",                         null,
     "context-cut",                 true,
     "context-copy",                true,
     "context-paste",               null,
     "context-delete",              false,
     "---",                         null,
     "context-selectall",           true,
     "---",                         null,
     "spell-add-dictionaries-main", true,
    ],
    {
      skipFocusChange: true,
    }
  );
  */
});

add_task(async function test_textarea_spellcheck() {
  todo(false, "spell checker tests are failing, bug 1246296");

  /*
  yield test_contextmenu("#test-textarea",
    ["*chubbiness",         true, // spelling suggestion
     "spell-add-to-dictionary", true,
     "---",                 null,
     "context-undo",        false,
     "context-redo",        false,
     "---",                 null,
     "context-cut",         true,
     "context-copy",        true,
     "context-paste",       null, // ignore clipboard state
     "context-delete",      false,
     "---",                 null,
     "context-selectall",   true,
     "---",                 null,
     "spell-check-enabled", true,
     "spell-dictionaries",  true,
         ["spell-check-dictionary-en-US", true,
          "---",                          null,
          "spell-add-dictionaries",       true], null
    ],
    {
      waitForSpellCheck: true,
      offsetX: 6,
      offsetY: 6,
      postCheckContextMenuFn() {
        document.getElementById("spell-add-to-dictionary").doCommand();
      }
    }
  );
  */
});

add_task(async function test_plaintext2() {
  await test_contextmenu("#test-text", plainTextItems);
});

add_task(async function test_undo_add_to_dictionary() {
  todo(false, "spell checker tests are failing, bug 1246296");

  /*
  yield test_contextmenu("#test-textarea",
    ["spell-undo-add-to-dictionary", true,
     "---",                 null,
     "context-undo",        false,
     "context-redo",        false,
     "---",                 null,
     "context-cut",         true,
     "context-copy",        true,
     "context-paste",       null, // ignore clipboard state
     "context-delete",      false,
     "context-selectall",   true,
     "---",                 null,
     "spell-check-enabled", true,
     "spell-dictionaries",  true,
         ["spell-check-dictionary-en-US", true,
          "---",                          null,
          "spell-add-dictionaries",       true], null
    ],
    {
      waitForSpellCheck: true,
      postCheckContextMenuFn() {
        document.getElementById("spell-undo-add-to-dictionary")
                .doCommand();
      }
    }
  );
  */
});

add_task(async function test_contenteditable() {
  todo(false, "spell checker tests are failing, bug 1246296");

  /*
  yield test_contextmenu("#test-contenteditable",
    ["spell-no-suggestions", false,
     "spell-add-to-dictionary", true,
     "---",                 null,
     "context-undo",        false,
     "context-redo",        false,
     "---",                 null,
     "context-cut",         true,
     "context-copy",        true,
     "context-paste",       null, // ignore clipboard state
     "context-delete",      false,
     "context-selectall",   true,
     "---",                 null,
     "spell-check-enabled", true,
     "spell-dictionaries",  true,
         ["spell-check-dictionary-en-US", true,
          "---",                          null,
          "spell-add-dictionaries",       true], null
    ],
    {waitForSpellCheck: true}
  );
  */
});

add_task(async function test_copylinkcommand() {
  await test_contextmenu("#test-link", null, {
    async postCheckContextMenuFn() {
      document.commandDispatcher
        .getControllerForCommand("cmd_copyLink")
        .doCommand("cmd_copyLink");

      // The easiest way to check the clipboard is to paste the contents
      // into a textbox.
      await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
        let doc = content.document;
        let input = doc.getElementById("test-input");
        input.focus();
        input.value = "";
      });
      document.commandDispatcher
        .getControllerForCommand("cmd_paste")
        .doCommand("cmd_paste");
      await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
        let doc = content.document;
        let input = doc.getElementById("test-input");
        Assert.equal(
          input.value,
          "http://mozilla.com/",
          "paste for command cmd_paste"
        );
        // Don't keep focus, because that may affect clipboard commands in
        // subsequently-opened menus.
        input.blur();
      });
    },
  });
});

add_task(async function test_pagemenu() {
  let pagemenuItems = [
    "+Plain item",
    { type: "", icon: "", checked: false, disabled: false },
    "+Disabled item",
    { type: "", icon: "", checked: false, disabled: true },
    "+Item w/ textContent",
    { type: "", icon: "", checked: false, disabled: false },
    "---",
    null,
    "+Checkbox",
    { type: "checkbox", icon: "", checked: true, disabled: false },
    "---",
    null,
    "+Radio1",
    { type: "checkbox", icon: "", checked: true, disabled: false },
    "+Radio2",
    { type: "checkbox", icon: "", checked: false, disabled: false },
    "+Radio3",
    { type: "checkbox", icon: "", checked: false, disabled: false },
    "---",
    null,
    "+Item w/ icon",
    { type: "", icon: "favicon.ico", checked: false, disabled: false },
    "+Item w/ bad icon",
    { type: "", icon: "", checked: false, disabled: false },
    "---",
    null,
    "generated-submenu-1",
    true,
    [
      "+Radio1",
      { type: "checkbox", icon: "", checked: false, disabled: false },
      "+Radio2",
      { type: "checkbox", icon: "", checked: true, disabled: false },
      "+Radio3",
      { type: "checkbox", icon: "", checked: false, disabled: false },
      "---",
      null,
      "+Checkbox",
      { type: "checkbox", icon: "", checked: false, disabled: false },
    ],
    null,
    "---",
    null,
    "context-savepage",
    true,
    ...(hasPocket ? ["context-pocket", true] : []),
    "context-selectall",
    true,
    "---",
    null,
    "context-take-screenshot",
    true,
    "---",
    null,
    "context-viewsource",
    true,
  ];
  pagemenuItems = NAVIGATION_ITEMS.concat(pagemenuItems);
  if (AppConstants.platform == "macosx") {
    // Take out the bookmarks page menu:
    let bookmarkItemIndex = pagemenuItems.indexOf("context-bookmarkpage");
    let bookmarksItemAndSeparator = pagemenuItems.splice(bookmarkItemIndex, 2);
    // Put it back before the save page item:
    pagemenuItems.splice(
      pagemenuItems.indexOf("context-savepage"),
      0,
      ...bookmarksItemAndSeparator
    );
  }
  await test_contextmenu("#test-pagemenu", pagemenuItems, {
    async postCheckContextMenuFn() {
      let item = contextMenu.getElementsByAttribute("generateditemid", "1")[0];
      ok(item, "Got generated XUL menu item");
      item.doCommand();
      await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
        let pagemenu = content.document.getElementById("test-pagemenu");
        Assert.ok(!pagemenu.hasAttribute("hopeless"), "attribute got removed");
      });
    },
  });
});

add_task(async function test_dom_full_screen() {
  let fullscreenItems = NAVIGATION_ITEMS.concat([
    "context-leave-dom-fullscreen",
    true,
    "---",
    null,
    "context-savepage",
    true,
    ...(hasPocket ? ["context-pocket", true] : []),
    "context-selectall",
    true,
    "---",
    null,
    "context-take-screenshot",
    true,
    "---",
    null,
    "context-viewsource",
    true,
  ]);
  if (AppConstants.platform == "macosx") {
    // Put the bookmarks item next to save page:
    const bmPageIndex = fullscreenItems.indexOf("context-bookmarkpage");
    let bmPageItems = fullscreenItems.splice(bmPageIndex, 2);
    fullscreenItems.splice(
      fullscreenItems.indexOf("context-savepage"),
      0,
      ...bmPageItems
    );
  }
  await test_contextmenu("#test-dom-full-screen", fullscreenItems, {
    shiftkey: true,
    async preCheckContextMenuFn() {
      await SpecialPowers.pushPrefEnv({
        set: [
          ["full-screen-api.allow-trusted-requests-only", false],
          ["full-screen-api.transition-duration.enter", "0 0"],
          ["full-screen-api.transition-duration.leave", "0 0"],
        ],
      });
      await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
        let doc = content.document;
        let win = doc.defaultView;
        let full_screen_element = doc.getElementById("test-dom-full-screen");
        let awaitFullScreenChange = ContentTaskUtils.waitForEvent(
          win,
          "fullscreenchange"
        );
        full_screen_element.requestFullscreen();
        await awaitFullScreenChange;
      });
    },
    async postCheckContextMenuFn() {
      await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
        let win = content.document.defaultView;
        let awaitFullScreenChange = ContentTaskUtils.waitForEvent(
          win,
          "fullscreenchange"
        );
        content.document.exitFullscreen();
        await awaitFullScreenChange;
      });
    },
  });
});

add_task(async function test_pagemenu2() {
  await test_contextmenu(
    "#test-text",
    [
      ...NAVIGATION_ITEMS,
      "context-savepage",
      true,
      ...(hasPocket ? ["context-pocket", true] : []),
      "context-selectall",
      true,
      "---",
      null,
      "context-take-screenshot",
      true,
      "---",
      null,
      "context-viewsource",
      true,
    ],
    { shiftkey: true }
  );
});

add_task(async function test_select_text() {
  await test_contextmenu(
    "#test-select-text",
    [
      "context-copy",
      true,
      "context-selectall",
      true,
      "context-print-selection",
      true,
      "---",
      null,
      "context-take-screenshot",
      true,
      "---",
      null,
      "context-searchselect",
      true,
      "context-searchselect-private",
      true,
      "---",
      null,
      "context-viewpartialsource-selection",
      true,
    ],
    {
      offsetX: 6,
      offsetY: 6,
      async preCheckContextMenuFn() {
        await selectText("#test-select-text");
      },
    }
  );
});

add_task(async function test_select_text_search_service_not_initialized() {
  // Pretend the search service is not initialised.
  Services.search.wrappedJSObject._initialized = false;
  await test_contextmenu(
    "#test-select-text",
    [
      "context-copy",
      true,
      "context-selectall",
      true,
      "context-print-selection",
      true,
      "---",
      null,
      "context-take-screenshot",
      true,
      "---",
      null,
      "context-viewpartialsource-selection",
      true,
    ],
    {
      offsetX: 6,
      offsetY: 6,
      async preCheckContextMenuFn() {
        await selectText("#test-select-text");
      },
    }
  );
  // Pretend the search service is not initialised.
  Services.search.wrappedJSObject._initialized = true;
});

add_task(async function test_select_text_link() {
  await test_contextmenu(
    "#test-select-text-link",
    [
      "context-openlinkincurrent",
      true,
      "context-openlinkintab",
      true,
      ...(hasContainers ? ["context-openlinkinusercontext-menu", true] : []),
      // We need a blank entry here because the containers submenu is
      // dynamically generated with no ids.
      ...(hasContainers ? ["", null] : []),
      "context-openlink",
      true,
      "context-openlinkprivate",
      true,
      "---",
      null,
      "context-bookmarklink",
      true,
      "context-savelink",
      true,
      "---",
      null,
      "context-copy",
      true,
      "context-selectall",
      true,
      "context-print-selection",
      true,
      "---",
      null,
      "context-searchselect",
      true,
      "context-searchselect-private",
      true,
      "---",
      null,
      "context-viewpartialsource-selection",
      true,
    ],
    {
      offsetX: 6,
      offsetY: 6,
      async preCheckContextMenuFn() {
        await selectText("#test-select-text-link");
      },
      async postCheckContextMenuFn() {
        await SpecialPowers.spawn(
          gBrowser.selectedBrowser,
          [],
          async function() {
            let win = content.document.defaultView;
            win.getSelection().removeAllRanges();
          }
        );
      },
    }
  );
});

add_task(async function test_imagelink() {
  await test_contextmenu("#test-image-link", [
    "context-openlinkintab",
    true,
    ...(hasContainers ? ["context-openlinkinusercontext-menu", true] : []),
    // We need a blank entry here because the containers submenu is
    // dynamically generated with no ids.
    ...(hasContainers ? ["", null] : []),
    "context-openlink",
    true,
    "context-openlinkprivate",
    true,
    "---",
    null,
    "context-bookmarklink",
    true,
    "context-savelink",
    true,
    ...(hasPocket ? ["context-savelinktopocket", true] : []),
    "context-copylink",
    true,
    "---",
    null,
    "context-viewimage",
    true,
    "context-saveimage",
    true,
    "context-copyimage-contents",
    true,
    "context-copyimage",
    true,
    "context-sendimage",
    true,
    ...(Services.prefs.getBoolPref("browser.menu.showViewImageInfo", false)
      ? ["context-viewimageinfo", true]
      : []),
    "---",
    null,
    "context-setDesktopBackground",
    true,
  ]);
});

add_task(async function test_select_input_text() {
  todo(false, "spell checker tests are failing, bug 1246296");

  /*
  yield test_contextmenu("#test-select-input-text",
    ["context-undo",                 false,
     "context-redo",                 false,
     "---",                          null,
     "context-cut",                  true,
     "context-copy",                 true,
     "context-paste",                null, // ignore clipboard state
     "context-delete",               true,
     "context-selectall",            true,
     "---",                          null,
     "context-searchselect",         true,
     "context-searchselect-private", true,
     "---",                          null,
     "spell-check-enabled",          true
    ].concat(LOGIN_FILL_ITEMS),
    {
      *preCheckContextMenuFn() {
        yield ContentTask.spawn(gBrowser.selectedBrowser, null, function*() {
          let doc = content.document;
          let win = doc.defaultView;
          win.getSelection().removeAllRanges();
          let element = doc.querySelector("#test-select-input-text");
          element.select();
        });
      }
    }
  );
  */
});

add_task(async function test_select_input_text_password() {
  todo(false, "spell checker tests are failing, bug 1246296");

  /*
  yield test_contextmenu("#test-select-input-text-type-password",
    ["context-undo",        false,
     "context-redo",        false,
     "---",                 null,
     "context-cut",         true,
     "context-copy",        true,
     "context-paste",       null, // ignore clipboard state
     "context-delete",      true,
     "context-selectall",   true,
     "---",                 null,
     "spell-check-enabled", true,
     // spell checker is shown on input[type="password"] on this testcase
     "spell-dictionaries",  true,
         ["spell-check-dictionary-en-US", true,
          "---",                          null,
          "spell-add-dictionaries",       true], null
    ].concat(LOGIN_FILL_ITEMS),
    {
      *preCheckContextMenuFn() {
        yield ContentTask.spawn(gBrowser.selectedBrowser, null, function*() {
          let doc = content.document;
          let win = doc.defaultView;
          win.getSelection().removeAllRanges();
          let element = doc.querySelector("#test-select-input-text-type-password");
          element.select();
        });
      },
      *postCheckContextMenuFn() {
        yield ContentTask.spawn(gBrowser.selectedBrowser, null, function*() {
          let win = content.document.defaultView;
          win.getSelection().removeAllRanges();
        });
      }
    }
  );
  */
});

add_task(async function test_longdesc() {
  await test_contextmenu("#test-longdesc", [
    "context-viewimage",
    true,
    "context-saveimage",
    true,
    "context-copyimage-contents",
    true,
    "context-copyimage",
    true,
    "context-sendimage",
    true,
    ...(Services.prefs.getBoolPref("browser.menu.showViewImageInfo", false)
      ? ["context-viewimageinfo", true]
      : []),
    "context-viewimagedesc",
    true,
    "---",
    null,
    "context-setDesktopBackground",
    true,
  ]);
});

add_task(async function test_srcdoc() {
  await test_contextmenu("#test-srcdoc", [
    ...NAVIGATION_ITEMS,
    "context-savepage",
    true,
    ...(hasPocket ? ["context-pocket", true] : []),
    "context-selectall",
    true,
    "---",
    null,
    "frame",
    null,
    getThisFrameSubMenu([
      "context-reloadframe",
      true,
      "---",
      null,
      "context-saveframe",
      true,
      "---",
      null,
      "context-printframe",
      true,
      "---",
      null,
      "context-viewframesource",
      true,
      "context-viewframeinfo",
      true,
    ]),
    null,
    "---",
    null,
    "context-viewsource",
    true,
  ]);
});

add_task(async function test_input_spell_false() {
  todo(false, "spell checker tests are failing, bug 1246296");

  /*
  yield test_contextmenu("#test-contenteditable-spellcheck-false",
    ["context-undo",        false,
     "context-redo",        false,
     "---",                 null,
     "context-cut",         true,
     "context-copy",        true,
     "context-paste",       null, // ignore clipboard state
     "context-delete",      false,
     "context-selectall",   true,
    ]
  );
  */
});

add_task(async function test_svg_link() {
  await test_contextmenu("#svg-with-link > a", [
    "context-openlinkintab",
    true,
    ...(hasContainers ? ["context-openlinkinusercontext-menu", true] : []),
    // We need a blank entry here because the containers submenu is
    // dynamically generated with no ids.
    ...(hasContainers ? ["", null] : []),
    "context-openlink",
    true,
    "context-openlinkprivate",
    true,
    "---",
    null,
    "context-bookmarklink",
    true,
    "context-savelink",
    true,
    ...(hasPocket ? ["context-savelinktopocket", true] : []),
    "context-copylink",
    true,
    "---",
    null,
    "context-searchselect",
    true,
    "context-searchselect-private",
    true,
  ]);

  await test_contextmenu("#svg-with-link2 > a", [
    "context-openlinkintab",
    true,
    ...(hasContainers ? ["context-openlinkinusercontext-menu", true] : []),
    // We need a blank entry here because the containers submenu is
    // dynamically generated with no ids.
    ...(hasContainers ? ["", null] : []),
    "context-openlink",
    true,
    "context-openlinkprivate",
    true,
    "---",
    null,
    "context-bookmarklink",
    true,
    "context-savelink",
    true,
    ...(hasPocket ? ["context-savelinktopocket", true] : []),
    "context-copylink",
    true,
    "---",
    null,
    "context-searchselect",
    true,
    "context-searchselect-private",
    true,
  ]);

  await test_contextmenu("#svg-with-link3 > a", [
    "context-openlinkintab",
    true,
    ...(hasContainers ? ["context-openlinkinusercontext-menu", true] : []),
    // We need a blank entry here because the containers submenu is
    // dynamically generated with no ids.
    ...(hasContainers ? ["", null] : []),
    "context-openlink",
    true,
    "context-openlinkprivate",
    true,
    "---",
    null,
    "context-bookmarklink",
    true,
    "context-savelink",
    true,
    ...(hasPocket ? ["context-savelinktopocket", true] : []),
    "context-copylink",
    true,
    "---",
    null,
    "context-searchselect",
    true,
    "context-searchselect-private",
    true,
  ]);
});

add_task(async function test_svg_relative_link() {
  await test_contextmenu("#svg-with-relative-link > a", [
    "context-openlinkintab",
    true,
    ...(hasContainers ? ["context-openlinkinusercontext-menu", true] : []),
    // We need a blank entry here because the containers submenu is
    // dynamically generated with no ids.
    ...(hasContainers ? ["", null] : []),
    "context-openlink",
    true,
    "context-openlinkprivate",
    true,
    "---",
    null,
    "context-bookmarklink",
    true,
    "context-savelink",
    true,
    ...(hasPocket ? ["context-savelinktopocket", true] : []),
    "context-copylink",
    true,
    "---",
    null,
    "context-searchselect",
    true,
    "context-searchselect-private",
    true,
  ]);

  await test_contextmenu("#svg-with-relative-link2 > a", [
    "context-openlinkintab",
    true,
    ...(hasContainers ? ["context-openlinkinusercontext-menu", true] : []),
    // We need a blank entry here because the containers submenu is
    // dynamically generated with no ids.
    ...(hasContainers ? ["", null] : []),
    "context-openlink",
    true,
    "context-openlinkprivate",
    true,
    "---",
    null,
    "context-bookmarklink",
    true,
    "context-savelink",
    true,
    ...(hasPocket ? ["context-savelinktopocket", true] : []),
    "context-copylink",
    true,
    "---",
    null,
    "context-searchselect",
    true,
    "context-searchselect-private",
    true,
  ]);

  await test_contextmenu("#svg-with-relative-link3 > a", [
    "context-openlinkintab",
    true,
    ...(hasContainers ? ["context-openlinkinusercontext-menu", true] : []),
    // We need a blank entry here because the containers submenu is
    // dynamically generated with no ids.
    ...(hasContainers ? ["", null] : []),
    "context-openlink",
    true,
    "context-openlinkprivate",
    true,
    "---",
    null,
    "context-bookmarklink",
    true,
    "context-savelink",
    true,
    ...(hasPocket ? ["context-savelinktopocket", true] : []),
    "context-copylink",
    true,
    "---",
    null,
    "context-searchselect",
    true,
    "context-searchselect-private",
    true,
  ]);
});

add_task(async function test_background_image() {
  let bgImageItems = [
    "context-viewimage",
    true,
    "context-copyimage",
    true,
    "context-sendimage",
    true,
    "---",
    null,
    "context-savepage",
    true,
    ...(hasPocket ? ["context-pocket", true] : []),
    "context-selectall",
    true,
    "---",
    null,
    "context-take-screenshot",
    true,
    "---",
    null,
    "context-viewsource",
    true,
  ];
  if (AppConstants.platform == "macosx") {
    // Back/fwd/(stop|reload) and their separator go before the image items,
    // followed by the bookmark item which goes with save page - so we need
    // to split up NAVIGATION_ITEMS and bgImageItems:
    bgImageItems = [
      ...NAVIGATION_ITEMS.slice(0, 8),
      ...bgImageItems.slice(0, 8),
      ...NAVIGATION_ITEMS.slice(8),
      ...bgImageItems.slice(8),
    ];
  } else {
    bgImageItems = NAVIGATION_ITEMS.concat(bgImageItems);
  }
  await test_contextmenu("#test-background-image", bgImageItems);

  // Don't show image related context menu commands for links with background images.
  await test_contextmenu("#test-background-image-link", [
    "context-openlinkintab",
    true,
    ...(hasContainers ? ["context-openlinkinusercontext-menu", true] : []),
    // We need a blank entry here because the containers submenu is
    // dynamically generated with no ids.
    ...(hasContainers ? ["", null] : []),
    "context-openlink",
    true,
    "context-openlinkprivate",
    true,
    "---",
    null,
    "context-bookmarklink",
    true,
    "context-savelink",
    true,
    "context-copylink",
    true,
    "---",
    null,
    "context-searchselect",
    true,
    "context-searchselect-private",
    true,
  ]);

  // Don't show image related context menu commands when there is a selection
  // with background images.
  await test_contextmenu(
    "#test-background-image",
    [
      "context-copy",
      true,
      "context-selectall",
      true,
      "context-print-selection",
      true,
      "---",
      null,
      "context-take-screenshot",
      true,
      "---",
      null,
      "context-searchselect",
      true,
      "context-searchselect-private",
      true,
      "---",
      null,
      "context-viewpartialsource-selection",
      true,
    ],
    {
      async preCheckContextMenuFn() {
        await selectText("#test-background-image");
      },
    }
  );
});

add_task(async function test_cleanup_html() {
  gBrowser.removeCurrentTab();
});

/**
 * Selects the text of the element that matches the provided `selector`
 *
 * @param {String} selector
 *        A selector passed to querySelector to find
 *        the element that will be referenced.
 */
async function selectText(selector) {
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [selector],
    async function(contentSelector) {
      info(`Selecting text of ${contentSelector}`);
      let doc = content.document;
      let win = doc.defaultView;
      win.getSelection().removeAllRanges();
      let div = doc.createRange();
      let element = doc.querySelector(contentSelector);
      Assert.ok(element, "Found element to select text from");
      div.setStartBefore(element);
      div.setEndAfter(element);
      win.getSelection().addRange(div);
    }
  );
}
