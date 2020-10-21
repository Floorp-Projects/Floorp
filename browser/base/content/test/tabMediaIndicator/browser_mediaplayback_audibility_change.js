/**
 * When media changes its audible state, the sound indicator should be
 * updated as well, which should appear only when web audio is audible.
 */
add_task(async function testUpdateSoundIndicatorWhenMediaPlaybackChanges() {
  info("create a tab loading media document");
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    gEMPTY_PAGE_URL
  );
  await initMediaPlaybackDocument(tab, "audio.ogg");

  info(`sound indicator should appear when audible audio starts playing`);
  await playAudio(tab);
  await waitForTabSoundIndicatorAppears(tab);

  info(`sound indicator should disappear when audio stops playing`);
  await pauseAudio(tab);
  await waitForTabSoundIndicatorDisappears(tab);

  info("remove tab");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function testUpdateSoundIndicatorWhenMediaBecomeSilent() {
  info("create a tab loading media document");
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    gEMPTY_PAGE_URL
  );
  await initMediaPlaybackDocument(tab, "audioEndedDuringPlaying.webm");

  info(`sound indicator should appear when audible audio starts playing`);
  await playAudio(tab);
  await waitForTabSoundIndicatorAppears(tab);

  info(`sound indicator should disappear when audio becomes silent`);
  await waitForTabSoundIndicatorDisappears(tab);

  info("remove tab");
  BrowserTestUtils.removeTab(tab);
});

/**
 * Following are helper functions
 */
function initMediaPlaybackDocument(tab, fileName) {
  return SpecialPowers.spawn(tab.linkedBrowser, [fileName], async fileName => {
    content.audio = content.document.createElement("audio");
    content.audio.src = fileName;
  });
}

function playAudio(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], async _ => {
    await content.audio.play();
  });
}

function pauseAudio(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], async _ => {
    content.audio.pause();
  });
}
