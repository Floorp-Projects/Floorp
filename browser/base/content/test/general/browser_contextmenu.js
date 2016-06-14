"use strict";

let contextMenu;
let LOGIN_FILL_ITEMS = [
  "---", null,
  "fill-login", null,
    [
      "fill-login-no-logins", false,
      "---", null,
      "fill-login-saved-passwords", true
    ], null,
];

let hasPocket = Services.prefs.getBoolPref("extensions.pocket.enabled");
let hasContainers = Services.prefs.getBoolPref("privacy.userContext.enabled");

add_task(function* test_setup() {
  const example_base = "http://example.com/browser/browser/base/content/test/general/";
  const url = example_base + "subtst_contextmenu.html";
  yield BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  const chrome_base = "chrome://mochitests/content/browser/browser/base/content/test/general/";
  const contextmenu_common = chrome_base + "contextmenu_common.js";
  Services.scriptloader.loadSubScript(contextmenu_common, this);

  yield ContentTask.spawn(gBrowser.selectedBrowser, null, function*() {
    let doc = content.document;
    let videoIframe = doc.querySelector("#test-video-in-iframe");
    let video = videoIframe.contentDocument.querySelector("video");
    let awaitPause = ContentTaskUtils.waitForEvent(video, "pause");
    video.pause();
    yield awaitPause;

    let audioIframe = doc.querySelector("#test-audio-in-iframe");
    // media documents always use a <video> tag.
    let audio = audioIframe.contentDocument.querySelector("video");
    awaitPause = ContentTaskUtils.waitForEvent(audio, "pause");
    audio.pause();
    yield awaitPause;
  });
});

let plainTextItems;
add_task(function* test_plaintext() {
  plainTextItems = ["context-navigation",   null,
                        ["context-back",         false,
                         "context-forward",      false,
                         "context-reload",       true,
                         "context-bookmarkpage", true], null,
                    "---",                  null,
                    "context-savepage",     true,
                    ...(hasPocket ? ["context-pocket", true] : []),
                    "---",                  null,
                    "context-viewbgimage",  false,
                    "context-selectall",    true,
                    "---",                  null,
                    "context-viewsource",   true,
                    "context-viewinfo",     true
                   ];
  yield test_contextmenu("#test-text", plainTextItems);
});

add_task(function* test_link() {
  yield test_contextmenu("#test-link",
    ["context-openlinkintab", true,
     ...(hasContainers ? ["context-openlinkinusercontext-menu", true] : []),
     // We need a blank entry here because the containers submenu is
     // dynamically generated with no ids.
     ...(hasContainers ? ["", null] : []),
     "context-openlink",      true,
     "context-openlinkprivate", true,
     "---",                   null,
     "context-bookmarklink",  true,
     "context-savelink",      true,
     ...(hasPocket ? ["context-savelinktopocket", true] : []),
     "context-copylink",      true,
     "context-searchselect",  true
    ]
  );
});

add_task(function* test_mailto() {
  yield test_contextmenu("#test-mailto",
    ["context-copyemail", true,
     "context-searchselect", true
    ]
  );
});

add_task(function* test_image() {
  yield test_contextmenu("#test-image",
    ["context-viewimage",            true,
     "context-copyimage-contents",   true,
     "context-copyimage",            true,
     "---",                          null,
     "context-saveimage",            true,
     "context-sendimage",            true,
     "context-setDesktopBackground", true,
     "context-viewimageinfo",        true
    ]
  );
});

add_task(function* test_canvas() {
  yield test_contextmenu("#test-canvas",
    ["context-viewimage",    true,
     "context-saveimage",    true,
     "context-selectall",    true
    ]
  );
});

add_task(function* test_video_ok() {
  yield test_contextmenu("#test-video-ok",
    ["context-media-play",         true,
     "context-media-mute",         true,
     "context-media-playbackrate", null,
         ["context-media-playbackrate-050x", true,
          "context-media-playbackrate-100x", true,
          "context-media-playbackrate-125x", true,
          "context-media-playbackrate-150x", true,
          "context-media-playbackrate-200x", true], null,
     "context-media-loop",         true,
     "context-media-hidecontrols", true,
     "context-video-fullscreen",   true,
     "---",                        null,
     "context-viewvideo",          true,
     "context-copyvideourl",       true,
     "---",                        null,
     "context-savevideo",          true,
     "context-video-saveimage",    true,
     "context-sendvideo",          true,
     "context-castvideo",          null,
       [], null
    ]
  );
});

add_task(function* test_audio_in_video() {
  yield test_contextmenu("#test-audio-in-video",
    ["context-media-play",         true,
     "context-media-mute",         true,
     "context-media-playbackrate", null,
         ["context-media-playbackrate-050x", true,
          "context-media-playbackrate-100x", true,
          "context-media-playbackrate-125x", true,
          "context-media-playbackrate-150x", true,
          "context-media-playbackrate-200x", true], null,
     "context-media-loop",         true,
     "context-media-showcontrols", true,
     "---",                        null,
     "context-copyaudiourl",       true,
     "---",                        null,
     "context-saveaudio",          true,
     "context-sendaudio",          true
    ]
  );
});

add_task(function* test_video_bad() {
  yield test_contextmenu("#test-video-bad",
    ["context-media-play",         false,
     "context-media-mute",         false,
     "context-media-playbackrate", null,
         ["context-media-playbackrate-050x", false,
          "context-media-playbackrate-100x", false,
          "context-media-playbackrate-125x", false,
          "context-media-playbackrate-150x", false,
          "context-media-playbackrate-200x", false], null,
     "context-media-loop",         true,
     "context-media-hidecontrols", false,
     "context-video-fullscreen",   false,
     "---",                        null,
     "context-viewvideo",          true,
     "context-copyvideourl",       true,
     "---",                        null,
     "context-savevideo",          true,
     "context-video-saveimage",    false,
     "context-sendvideo",          true,
     "context-castvideo",          null,
       [], null
    ]
  );
});

add_task(function* test_video_bad2() {
  yield test_contextmenu("#test-video-bad2",
    ["context-media-play",         false,
     "context-media-mute",         false,
     "context-media-playbackrate", null,
         ["context-media-playbackrate-050x", false,
          "context-media-playbackrate-100x", false,
          "context-media-playbackrate-125x", false,
          "context-media-playbackrate-150x", false,
          "context-media-playbackrate-200x", false], null,
     "context-media-loop",         true,
     "context-media-hidecontrols", false,
     "context-video-fullscreen",   false,
     "---",                        null,
     "context-viewvideo",          false,
     "context-copyvideourl",       false,
     "---",                        null,
     "context-savevideo",          false,
     "context-video-saveimage",    false,
     "context-sendvideo",          false,
     "context-castvideo",          null,
       [], null
    ]
  );
});

add_task(function* test_iframe() {
  yield test_contextmenu("#test-iframe",
    ["context-navigation", null,
         ["context-back",         false,
          "context-forward",      false,
          "context-reload",       true,
          "context-bookmarkpage", true], null,
     "---",                  null,
     "context-savepage",     true,
     ...(hasPocket ? ["context-pocket", true] : []),
     "---",                  null,
     "context-viewbgimage",  false,
     "context-selectall",    true,
     "frame",                null,
         ["context-showonlythisframe", true,
          "context-openframeintab",    true,
          "context-openframe",         true,
          "---",                       null,
          "context-reloadframe",       true,
          "---",                       null,
          "context-bookmarkframe",     true,
          "context-saveframe",         true,
          "---",                       null,
          "context-printframe",        true,
          "---",                       null,
          "context-viewframesource",   true,
          "context-viewframeinfo",     true], null,
     "---",                  null,
     "context-viewsource",   true,
     "context-viewinfo",     true
    ]
  );
});

add_task(function* test_video_in_iframe() {
  yield test_contextmenu("#test-video-in-iframe",
    ["context-media-play",         true,
     "context-media-mute",         true,
     "context-media-playbackrate", null,
         ["context-media-playbackrate-050x", true,
          "context-media-playbackrate-100x", true,
          "context-media-playbackrate-125x", true,
          "context-media-playbackrate-150x", true,
          "context-media-playbackrate-200x", true], null,
     "context-media-loop",         true,
     "context-media-hidecontrols", true,
     "context-video-fullscreen",   true,
     "---",                        null,
     "context-viewvideo",          true,
     "context-copyvideourl",       true,
     "---",                        null,
     "context-savevideo",          true,
     "context-video-saveimage",    true,
     "context-sendvideo",          true,
     "context-castvideo",          null,
       [], null,
     "frame",                null,
         ["context-showonlythisframe", true,
          "context-openframeintab",    true,
          "context-openframe",         true,
          "---",                       null,
          "context-reloadframe",       true,
          "---",                       null,
          "context-bookmarkframe",     true,
          "context-saveframe",         true,
          "---",                       null,
          "context-printframe",        true,
          "---",                       null,
          "context-viewframeinfo",     true], null]
  );
});

add_task(function* test_audio_in_iframe() {
  yield test_contextmenu("#test-audio-in-iframe",
    ["context-media-play",         true,
     "context-media-mute",         true,
     "context-media-playbackrate", null,
         ["context-media-playbackrate-050x", true,
          "context-media-playbackrate-100x", true,
          "context-media-playbackrate-125x", true,
          "context-media-playbackrate-150x", true,
          "context-media-playbackrate-200x", true], null,
     "context-media-loop",         true,
     "---",                        null,
     "context-copyaudiourl",       true,
     "---",                        null,
     "context-saveaudio",          true,
     "context-sendaudio",          true,
     "frame",                null,
         ["context-showonlythisframe", true,
          "context-openframeintab",    true,
          "context-openframe",         true,
          "---",                       null,
          "context-reloadframe",       true,
          "---",                       null,
          "context-bookmarkframe",     true,
          "context-saveframe",         true,
          "---",                       null,
          "context-printframe",        true,
          "---",                       null,
          "context-viewframeinfo",     true], null]
  );
});

add_task(function* test_image_in_iframe() {
  yield test_contextmenu("#test-image-in-iframe",
    ["context-viewimage",            true,
     "context-copyimage-contents",   true,
     "context-copyimage",            true,
     "---",                          null,
     "context-saveimage",            true,
     "context-sendimage",            true,
     "context-setDesktopBackground", true,
     "context-viewimageinfo",        true,
     "frame",                null,
         ["context-showonlythisframe", true,
          "context-openframeintab",    true,
          "context-openframe",         true,
          "---",                       null,
          "context-reloadframe",       true,
          "---",                       null,
          "context-bookmarkframe",     true,
          "context-saveframe",         true,
          "---",                       null,
          "context-printframe",        true,
          "---",                       null,
          "context-viewframeinfo",     true], null]
  );
});

add_task(function* test_textarea() {
  // Disabled since this is seeing spell-check-enabled
  // instead of spell-add-dictionaries-main
  todo(false, "spell checker tests are failing, bug 1246296");
  return;

  yield test_contextmenu("#test-textarea",
    ["context-undo",                false,
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
});

add_task(function* test_textarea_spellcheck() {
  todo(false, "spell checker tests are failing, bug 1246296");
  return;

  yield test_contextmenu("#test-textarea",
    ["*chubbiness",         true, // spelling suggestion
     "spell-add-to-dictionary", true,
     "---",                 null,
     "context-undo",        false,
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
});

add_task(function* test_plaintext2() {
  yield test_contextmenu("#test-text", plainTextItems);
});

add_task(function* test_undo_add_to_dictionary() {
  todo(false, "spell checker tests are failing, bug 1246296");
  return;

  yield test_contextmenu("#test-textarea",
    ["spell-undo-add-to-dictionary", true,
     "---",                 null,
     "context-undo",        false,
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
      postCheckContextMenuFn() {
        document.getElementById("spell-undo-add-to-dictionary")
                .doCommand();
      }
    }
  );
});

add_task(function* test_contenteditable() {
  todo(false, "spell checker tests are failing, bug 1246296");
  return;

  yield test_contextmenu("#test-contenteditable",
    ["spell-no-suggestions", false,
     "spell-add-to-dictionary", true,
     "---",                 null,
     "context-undo",        false,
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
    {waitForSpellCheck: true}
  );
});

add_task(function* test_copylinkcommand() {
  yield test_contextmenu("#test-link", null, {
    postCheckContextMenuFn: function*() {
      document.commandDispatcher
              .getControllerForCommand("cmd_copyLink")
              .doCommand("cmd_copyLink");

      // The easiest way to check the clipboard is to paste the contents
      // into a textbox.
      yield ContentTask.spawn(gBrowser.selectedBrowser, null, function*() {
        let doc = content.document;
        let input = doc.getElementById("test-input");
        input.focus();
        input.value = "";
      });
      document.commandDispatcher
              .getControllerForCommand("cmd_paste")
              .doCommand("cmd_paste");
      yield ContentTask.spawn(gBrowser.selectedBrowser, null, function*() {
        let doc = content.document;
        let input = doc.getElementById("test-input");
        Assert.equal(input.value, "http://mozilla.com/", "paste for command cmd_paste");
      });
    }
  });
});

add_task(function* test_pagemenu() {
  yield test_contextmenu("#test-pagemenu",
    ["context-navigation",   null,
         ["context-back",         false,
          "context-forward",      false,
          "context-reload",       true,
          "context-bookmarkpage", true], null,
     "---",                  null,
     "+Plain item",          {type: "", icon: "", checked: false, disabled: false},
     "+Disabled item",       {type: "", icon: "", checked: false, disabled: true},
     "+Item w/ textContent", {type: "", icon: "", checked: false, disabled: false},
     "---",                  null,
     "+Checkbox",            {type: "checkbox", icon: "", checked: true, disabled: false},
     "---",                  null,
     "+Radio1",              {type: "checkbox", icon: "", checked: true, disabled: false},
     "+Radio2",              {type: "checkbox", icon: "", checked: false, disabled: false},
     "+Radio3",              {type: "checkbox", icon: "", checked: false, disabled: false},
     "---",                  null,
     "+Item w/ icon",        {type: "", icon: "favicon.ico", checked: false, disabled: false},
     "+Item w/ bad icon",    {type: "", icon: "", checked: false, disabled: false},
     "---",                  null,
     "generated-submenu-1",  true,
         ["+Radio1",             {type: "checkbox", icon: "", checked: false, disabled: false},
          "+Radio2",             {type: "checkbox", icon: "", checked: true, disabled: false},
          "+Radio3",             {type: "checkbox", icon: "", checked: false, disabled: false},
          "---",                 null,
          "+Checkbox",           {type: "checkbox", icon: "", checked: false, disabled: false}], null,
     "---",                  null,
     "context-savepage",     true,
     ...(hasPocket ? ["context-pocket", true] : []),
     "---",                  null,
     "context-viewbgimage",  false,
     "context-selectall",    true,
     "---",                  null,
     "context-viewsource",   true,
     "context-viewinfo",     true
    ],
    {postCheckContextMenuFn: function*() {
      let item = contextMenu.getElementsByAttribute("generateditemid", "1")[0];
      ok(item, "Got generated XUL menu item");
      item.doCommand();
      yield ContentTask.spawn(gBrowser.selectedBrowser, null, function*() {
        let pagemenu = content.document.getElementById("test-pagemenu");
        Assert.ok(!pagemenu.hasAttribute("hopeless"), "attribute got removed");
      });
    }
  });
});

add_task(function* test_dom_full_screen() {
  yield test_contextmenu("#test-dom-full-screen",
    ["context-navigation",           null,
         ["context-back",            false,
          "context-forward",         false,
          "context-reload",          true,
          "context-bookmarkpage",    true], null,
     "---",                          null,
     "context-leave-dom-fullscreen", true,
     "---",                          null,
     "context-savepage",             true,
     ...(hasPocket ? ["context-pocket", true] : []),
     "---",                          null,
     "context-viewbgimage",          false,
     "context-selectall",            true,
     "---",                          null,
     "context-viewsource",           true,
     "context-viewinfo",             true
    ],
    {
      shiftkey: true,
      *preCheckContextMenuFn() {
        yield pushPrefs(["full-screen-api.allow-trusted-requests-only", false],
                        ["full-screen-api.transition-duration.enter", "0 0"],
                        ["full-screen-api.transition-duration.leave", "0 0"])
        yield ContentTask.spawn(gBrowser.selectedBrowser, null, function*() {
          let doc = content.document;
          let win = doc.defaultView;
          let full_screen_element = doc.getElementById("test-dom-full-screen");
          let awaitFullScreenChange =
            ContentTaskUtils.waitForEvent(win, "fullscreenchange");
          full_screen_element.requestFullscreen();
          yield awaitFullScreenChange;
        });
      },
      *postCheckContextMenuFn() {
        yield ContentTask.spawn(gBrowser.selectedBrowser, null, function*() {
          let win = content.document.defaultView;
          let awaitFullScreenChange =
            ContentTaskUtils.waitForEvent(win, "fullscreenchange");
          content.document.exitFullscreen();
          yield awaitFullScreenChange;
        });
      }
    }
  );
});

add_task(function* test_pagemenu2() {
  yield test_contextmenu("#test-text",
    ["context-navigation", null,
         ["context-back",         false,
          "context-forward",      false,
          "context-reload",       true,
          "context-bookmarkpage", true], null,
     "---",                  null,
     "context-savepage",     true,
     ...(hasPocket ? ["context-pocket", true] : []),
     "---",                  null,
     "context-viewbgimage",  false,
     "context-selectall",    true,
     "---",                  null,
     "context-viewsource",   true,
     "context-viewinfo",     true
    ],
    {shiftkey: true}
  );
});

add_task(function* test_select_text() {
  yield test_contextmenu("#test-select-text",
    ["context-copy",                        true,
     "context-selectall",                   true,
     "---",                                 null,
     "context-searchselect",                true,
     "context-viewpartialsource-selection", true
    ],
    {
      offsetX: 6,
      offsetY: 6,
      *preCheckContextMenuFn() {
        yield selectText("#test-select-text");
      }
    }
  );
});

add_task(function* test_select_text_link() {
  yield test_contextmenu("#test-select-text-link",
    ["context-openlinkincurrent",           true,
     "context-openlinkintab",               true,
     ...(hasContainers ? ["context-openlinkinusercontext-menu", true] : []),
     // We need a blank entry here because the containers submenu is
     // dynamically generated with no ids.
     ...(hasContainers ? ["", null] : []),
     "context-openlink",                    true,
     "context-openlinkprivate",             true,
     "---",                                 null,
     "context-bookmarklink",                true,
     "context-savelink",                    true,
     "context-copy",                        true,
     "context-selectall",                   true,
     "---",                                 null,
     "context-searchselect",                true,
     "context-viewpartialsource-selection", true
    ],
    {
      offsetX: 6,
      offsetY: 6,
      *preCheckContextMenuFn() {
        yield selectText("#test-select-text-link");
      },
      *postCheckContextMenuFn() {
        yield ContentTask.spawn(gBrowser.selectedBrowser, null, function*() {
          let win = content.document.defaultView;
          win.getSelection().removeAllRanges();
        });
      }
    }
  );
});

add_task(function* test_imagelink() {
  yield test_contextmenu("#test-image-link",
    ["context-openlinkintab", true,
     ...(hasContainers ? ["context-openlinkinusercontext-menu", true] : []),
     // We need a blank entry here because the containers submenu is
     // dynamically generated with no ids.
     ...(hasContainers ? ["", null] : []),
     "context-openlink",      true,
     "context-openlinkprivate", true,
     "---",                   null,
     "context-bookmarklink",  true,
     "context-savelink",      true,
     ...(hasPocket ? ["context-savelinktopocket", true] : []),
     "context-copylink",      true,
     "---",                   null,
     "context-viewimage",            true,
     "context-copyimage-contents",   true,
     "context-copyimage",            true,
     "---",                          null,
     "context-saveimage",            true,
     "context-sendimage",            true,
     "context-setDesktopBackground", true,
     "context-viewimageinfo",        true
    ]
  );
});

add_task(function* test_select_input_text() {
  todo(false, "spell checker tests are failing, bug 1246296");
  return;

  yield test_contextmenu("#test-select-input-text",
    ["context-undo",        false,
     "---",                 null,
     "context-cut",         true,
     "context-copy",        true,
     "context-paste",       null, // ignore clipboard state
     "context-delete",      true,
     "---",                 null,
     "context-selectall",   true,
     "context-searchselect",true,
     "---",                 null,
     "spell-check-enabled", true
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
});

add_task(function* test_select_input_text_password() {
  todo(false, "spell checker tests are failing, bug 1246296");
  return;

  yield test_contextmenu("#test-select-input-text-type-password",
    ["context-undo",        false,
     "---",                 null,
     "context-cut",         true,
     "context-copy",        true,
     "context-paste",       null, // ignore clipboard state
     "context-delete",      true,
     "---",                 null,
     "context-selectall",   true,
     "---",                 null,
     "spell-check-enabled", true,
     //spell checker is shown on input[type="password"] on this testcase
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
});

add_task(function* test_click_to_play_blocked_plugin() {
  yield test_contextmenu("#test-plugin",
    ["context-navigation", null,
         ["context-back",         false,
          "context-forward",      false,
          "context-reload",       true,
          "context-bookmarkpage", true], null,
     "---",                  null,
     "context-ctp-play",     true,
     "context-ctp-hide",     true,
     "---",                  null,
     "context-savepage",     true,
     ...(hasPocket ? ["context-pocket", true] : []),
     "---",                  null,
     "context-viewbgimage",  false,
     "context-selectall",    true,
     "---",                  null,
     "context-viewsource",   true,
     "context-viewinfo",     true
    ],
    {
      preCheckContextMenuFn: function*() {
        pushPrefs(["plugins.click_to_play", true]);
        setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY);
      },
      postCheckContextMenuFn: function*() {
        getTestPlugin().enabledState = Ci.nsIPluginTag.STATE_ENABLED;
      }
    }
  );
});

add_task(function* test_longdesc() {
  yield test_contextmenu("#test-longdesc",
    ["context-viewimage",            true,
     "context-copyimage-contents",   true,
     "context-copyimage",            true,
     "---",                          null,
     "context-saveimage",            true,
     "context-sendimage",            true,
     "context-setDesktopBackground", true,
     "context-viewimageinfo",        true,
     "context-viewimagedesc",        true
    ]
  );
});

add_task(function* test_srcdoc() {
  yield test_contextmenu("#test-srcdoc",
    ["context-navigation", null,
         ["context-back",         false,
          "context-forward",      false,
          "context-reload",       true,
          "context-bookmarkpage", true], null,
     "---",                  null,
     "context-savepage",     true,
     ...(hasPocket ? ["context-pocket", true] : []),
     "---",                  null,
     "context-viewbgimage",  false,
     "context-selectall",    true,
     "frame",                null,
         ["context-reloadframe",       true,
          "---",                       null,
          "context-saveframe",         true,
          "---",                       null,
          "context-printframe",        true,
          "---",                       null,
          "context-viewframesource",   true,
          "context-viewframeinfo",     true], null,
     "---",                  null,
     "context-viewsource",   true,
     "context-viewinfo",     true
    ]
  );
});

add_task(function* test_input_spell_false() {
  todo(false, "spell checker tests are failing, bug 1246296");
  return;

  yield test_contextmenu("#test-contenteditable-spellcheck-false",
    ["context-undo",        false,
     "---",                 null,
     "context-cut",         true,
     "context-copy",        true,
     "context-paste",       null, // ignore clipboard state
     "context-delete",      false,
     "---",                 null,
     "context-selectall",   true,
     "---",                 null,
     "spell-add-dictionaries-main",  true,
    ]
  );
});

add_task(function* test_cleanup() {
  gBrowser.removeCurrentTab();
});

/**
 * Selects the text of the element that matches the provided `selector`
 *
 * @param {String} selector
 *        A selector passed to querySelector to find
 *        the element that will be referenced.
 */
function* selectText(selector) {
  yield ContentTask.spawn(gBrowser.selectedBrowser, selector, function*(selector) {
    info(`Selecting text of ${selector}`);
    let doc = content.document;
    let win = doc.defaultView;
    win.getSelection().removeAllRanges();
    let div = doc.createRange();
    let element = doc.querySelector(selector);
    Assert.ok(element, "Found element to select text from");
    div.setStartBefore(element);
    div.setEndAfter(element);
    win.getSelection().addRange(div);
  });
}
