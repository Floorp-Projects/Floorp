/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_animate() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://mochi.test:8888/"
  );

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://mochi.test/*"],
          js: ["content-script.js"],
        },
      ],
    },

    files: {
      "content-script.js": function() {
        let elem = document.getElementsByTagName("body")[0];
        elem.style.border = "2px solid red";

        let anim = elem.animate({ opacity: [1, 0] }, 2000);
        let frames = anim.effect.getKeyframes();
        browser.test.assertEq(
          frames.length,
          2,
          "frames for Element.animate should be non-zero"
        );
        browser.test.assertEq(
          frames[0].opacity,
          "1",
          "first frame opacity for Element.animate should be specified value"
        );
        browser.test.assertEq(
          frames[0].computedOffset,
          0,
          "first frame offset for Element.animate should be 0"
        );
        browser.test.assertEq(
          frames[1].opacity,
          "0",
          "last frame opacity for Element.animate should be specified value"
        );
        browser.test.assertEq(
          frames[1].computedOffset,
          1,
          "last frame offset for Element.animate should be 1"
        );

        browser.test.notifyPass("contentScriptAnimate");
      },
    },
  });

  await extension.startup();
  await extension.awaitFinish("contentScriptAnimate");
  await extension.unload();

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_KeyframeEffect() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://mochi.test:8888/"
  );

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://mochi.test/*"],
          js: ["content-script.js"],
        },
      ],
    },

    files: {
      "content-script.js": function() {
        let elem = document.getElementsByTagName("body")[0];
        elem.style.border = "2px solid red";

        let effect = new KeyframeEffect(
          elem,
          [{ opacity: 1, offset: 0 }, { opacity: 0, offset: 1 }],
          { duration: 1000, fill: "forwards" }
        );
        let frames = effect.getKeyframes();
        browser.test.assertEq(
          frames.length,
          2,
          "frames for KeyframeEffect ctor should be non-zero"
        );
        browser.test.assertEq(
          frames[0].opacity,
          "1",
          "first frame opacity for KeyframeEffect ctor should be specified value"
        );
        browser.test.assertEq(
          frames[0].computedOffset,
          0,
          "first frame offset for KeyframeEffect ctor should be 0"
        );
        browser.test.assertEq(
          frames[1].opacity,
          "0",
          "last frame opacity for KeyframeEffect ctor should be specified value"
        );
        browser.test.assertEq(
          frames[1].computedOffset,
          1,
          "last frame offset for KeyframeEffect ctor should be 1"
        );

        let animation = new Animation(effect, document.timeline);
        animation.play();

        browser.test.notifyPass("contentScriptKeyframeEffect");
      },
    },
  });

  await extension.startup();
  await extension.awaitFinish("contentScriptKeyframeEffect");
  await extension.unload();

  BrowserTestUtils.removeTab(tab);
});
