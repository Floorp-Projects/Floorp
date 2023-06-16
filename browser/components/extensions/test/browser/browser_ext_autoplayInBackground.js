"use strict";

function setup_test_preference(enableScript) {
  return SpecialPowers.pushPrefEnv({
    set: [
      ["media.autoplay.default", 1],
      ["media.autoplay.blocking_policy", 0],
      ["media.autoplay.allow-extension-background-pages", enableScript],
    ],
  });
}

async function testAutoplayInBackgroundScript(enableScript) {
  info(`- setup test preference, enableScript=${enableScript} -`);
  await setup_test_preference(enableScript);

  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.log("- create audio in background page -");
      let audio = new Audio();
      audio.src =
        "https://example.com/browser/browser/components/extensions/test/browser/silence.ogg";
      audio.play().then(
        function () {
          browser.test.log("play succeed!");
          browser.test.sendMessage("play-succeed");
        },
        function () {
          browser.test.log("play promise was rejected!");
          browser.test.sendMessage("play-failed");
        }
      );
    },
  });

  await extension.startup();

  if (enableScript) {
    await extension.awaitMessage("play-succeed");
    ok(true, "play promise was resolved!");
  } else {
    await extension.awaitMessage("play-failed");
    ok(true, "play promise was rejected!");
  }

  await extension.unload();
}

add_task(async function testMain() {
  await testAutoplayInBackgroundScript(true /* enable autoplay */);
  await testAutoplayInBackgroundScript(false /* enable autoplay */);
});
