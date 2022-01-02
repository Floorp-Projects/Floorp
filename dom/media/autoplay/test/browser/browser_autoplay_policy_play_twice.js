const VIDEO_PAGE = GetTestWebBasedURL("file_video.html");

function setup_test_preference(enableUserGesture) {
  let state = enableUserGesture ? "enable" : "disable";
  info(`- set pref : ${state} user gesture -`);
  return SpecialPowers.pushPrefEnv({
    set: [
      ["media.autoplay.default", SpecialPowers.Ci.nsIAutoplay.BLOCKED],
      ["media.autoplay.blocking_policy", enableUserGesture ? 0 : 1],
    ],
  });
}

async function allow_play_for_played_video() {
  info("- open new tab  -");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "about:blank"
  );
  BrowserTestUtils.loadURI(tab.linkedBrowser, VIDEO_PAGE);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info("- simulate user-click to start video -");
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#v",
    { button: 0 },
    tab.linkedBrowser
  );

  async function play_video_again() {
    let video = content.document.getElementById("v");
    ok(!video.paused, "video is playing");

    info("- call video play() again -");
    try {
      await video.play();
      ok(true, "success to resolve play promise");
    } catch (e) {
      ok(false, "promise should not be rejected");
    }
  }
  await SpecialPowers.spawn(tab.linkedBrowser, [], play_video_again);

  info("- remove tab -");
  BrowserTestUtils.removeTab(tab);
}

add_task(async function start_test() {
  await setup_test_preference(true);
  await allow_play_for_played_video();

  await setup_test_preference(false);
  await allow_play_for_played_video();
});
