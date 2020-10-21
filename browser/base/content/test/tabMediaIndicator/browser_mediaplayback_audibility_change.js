/**
 * When media changes its audible state, the sound indicator should be
 * updated as well, which should appear only when web audio is audible.
 */
add_task(async function testUpdateSoundIndicatorWhenMediaPlaybackChanges() {
  info("create a tab loading media document");
  const tab = await createBlankForegroundTab();
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
  const tab = await createBlankForegroundTab();
  await initMediaPlaybackDocument(tab, "audioEndedDuringPlaying.webm");

  info(`sound indicator should appear when audible audio starts playing`);
  await playAudio(tab);
  await waitForTabSoundIndicatorAppears(tab);

  info(`sound indicator should disappear when audio becomes silent`);
  await waitForTabSoundIndicatorDisappears(tab);

  info("remove tab");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function testSoundIndicatorWouldWorkForMediaWithoutPreload() {
  info("create a tab loading media document");
  const tab = await createBlankForegroundTab();
  await initMediaPlaybackDocument(tab, "audio.ogg", { preload: "none" });

  info(`sound indicator should appear when audible audio starts playing`);
  await playAudio(tab);
  await waitForTabSoundIndicatorAppears(tab);

  info(`sound indicator should disappear when audio stops playing`);
  await pauseAudio(tab);
  await waitForTabSoundIndicatorDisappears(tab);

  info("remove tab");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function testSoundIndicatorShouldDisappearAfterTabNavigation() {
  info("create a tab loading media document");
  const tab = await createBlankForegroundTab();
  await initMediaPlaybackDocument(tab, "audio.ogg");

  info(`sound indicator should appear when audible audio starts playing`);
  await playAudio(tab);
  await waitForTabSoundIndicatorAppears(tab);

  info(`sound indicator should disappear after navigating tab to blank page`);
  await BrowserTestUtils.loadURI(tab.linkedBrowser, "about:blank");
  await waitForTabSoundIndicatorDisappears(tab);

  info("remove tab");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function testSoundIndicatorForAudioStream() {
  info("create a tab loading media document");
  const tab = await createBlankForegroundTab();
  await initMediaStreamPlaybackDocument(tab);

  info(`sound indicator should appear when audible audio starts playing`);
  await playAudio(tab);
  await waitForTabSoundIndicatorAppears(tab);

  info(`sound indicator should disappear when audio stops playing`);
  await pauseAudio(tab);
  await waitForTabSoundIndicatorDisappears(tab);

  info("remove tab");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function testPerformPlayOnMediaLoadingNewSource() {
  info("create a tab loading media document");
  const tab = await createBlankForegroundTab();
  await initMediaPlaybackDocument(tab, "audio.ogg");

  info(`sound indicator should appear when audible audio starts playing`);
  await playAudio(tab);
  await waitForTabSoundIndicatorAppears(tab);

  info(`sound indicator should disappear when audio stops playing`);
  await pauseAudio(tab);
  await waitForTabSoundIndicatorDisappears(tab);

  info(`reset media src and play it again should make sound indicator appear`);
  await assignNewSourceForAudio(tab, "audio.ogg");
  await playAudio(tab);
  await waitForTabSoundIndicatorAppears(tab);

  info("remove tab");
  BrowserTestUtils.removeTab(tab);
});

/**
 * Following are helper functions
 */
function initMediaPlaybackDocument(tab, fileName, { preload } = {}) {
  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [fileName, preload],
    async (fileName, preload) => {
      content.audio = content.document.createElement("audio");
      if (preload) {
        content.audio.preload = preload;
      }
      content.audio.src = fileName;
    }
  );
}

function initMediaStreamPlaybackDocument(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], async _ => {
    content.audio = content.document.createElement("audio");
    content.audio.srcObject = new content.AudioContext().createMediaStreamDestination().stream;
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

function assignNewSourceForAudio(tab, fileName) {
  return SpecialPowers.spawn(tab.linkedBrowser, [fileName], async fileName => {
    content.audio.src = "";
    content.audio.removeAttribute("src");
    content.audio.src = fileName;
  });
}
