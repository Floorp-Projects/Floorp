/**
 * When media changes its audible state, the sound indicator should be
 * updated as well, which should appear only when web audio is audible.
 */
add_task(async function testUpdateSoundIndicatorWhenMediaPlaybackChanges() {
  info("create a tab loading media document");
  const tab = await createBlankForegroundTab();
  await initMediaPlaybackDocument(tab, "audio.ogg");

  info(`sound indicator should appear when audible audio starts playing`);
  await playMedia(tab);
  await waitForTabSoundIndicatorAppears(tab);

  info(`sound indicator should disappear when audio stops playing`);
  await pauseMedia(tab);
  await waitForTabSoundIndicatorDisappears(tab);

  info("remove tab");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function testUpdateSoundIndicatorWhenMediaBecomeSilent() {
  info("create a tab loading media document");
  const tab = await createBlankForegroundTab();
  await initMediaPlaybackDocument(tab, "audioEndedDuringPlaying.webm");

  info(`sound indicator should appear when audible audio starts playing`);
  await playMedia(tab);
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
  await playMedia(tab);
  await waitForTabSoundIndicatorAppears(tab);

  info(`sound indicator should disappear when audio stops playing`);
  await pauseMedia(tab);
  await waitForTabSoundIndicatorDisappears(tab);

  info("remove tab");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function testSoundIndicatorShouldDisappearAfterTabNavigation() {
  info("create a tab loading media document");
  const tab = await createBlankForegroundTab();
  await initMediaPlaybackDocument(tab, "audio.ogg");

  info(`sound indicator should appear when audible audio starts playing`);
  await playMedia(tab);
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
  await playMedia(tab);
  await waitForTabSoundIndicatorAppears(tab);

  info(`sound indicator should disappear when audio stops playing`);
  await pauseMedia(tab);
  await waitForTabSoundIndicatorDisappears(tab);

  info("remove tab");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function testPerformPlayOnMediaLoadingNewSource() {
  info("create a tab loading media document");
  const tab = await createBlankForegroundTab();
  await initMediaPlaybackDocument(tab, "audio.ogg");

  info(`sound indicator should appear when audible audio starts playing`);
  await playMedia(tab);
  await waitForTabSoundIndicatorAppears(tab);

  info(`sound indicator should disappear when audio stops playing`);
  await pauseMedia(tab);
  await waitForTabSoundIndicatorDisappears(tab);

  info(`reset media src and play it again should make sound indicator appear`);
  await assignNewSourceForAudio(tab, "audio.ogg");
  await playMedia(tab);
  await waitForTabSoundIndicatorAppears(tab);

  info("remove tab");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function testSoundIndicatorShouldDisappearWhenAbortingMedia() {
  info("create a tab loading media document");
  const tab = await createBlankForegroundTab();
  await initMediaPlaybackDocument(tab, "audio.ogg");

  info(`sound indicator should appear when audible audio starts playing`);
  await playMedia(tab);
  await waitForTabSoundIndicatorAppears(tab);

  info(`sound indicator should disappear when aborting audio source`);
  await assignNewSourceForAudio(tab, "");
  await waitForTabSoundIndicatorDisappears(tab);

  info("remove tab");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function testNoSoundIndicatorForMediaWithoutAudioTrack() {
  info("create a tab loading media document");
  const tab = await createBlankForegroundTab({ needObserver: true });
  await initMediaPlaybackDocument(tab, "noaudio.webm", { createVideo: true });

  info(`no sound indicator should show for playing media without audio track`);
  await playMedia(tab, { resolveOnEnded: true });
  ok(!tab.observer.hasEverUpdated(), "didn't ever update sound indicator");

  info("remove tab");
  BrowserTestUtils.removeTab(tab);
});

/**
 * Following are helper functions
 */
function initMediaPlaybackDocument(
  tab,
  fileName,
  { preload, createVideo } = {}
) {
  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [fileName, preload, createVideo],
    async (fileName, preload, createVideo) => {
      if (createVideo) {
        content.media = content.document.createElement("video");
      } else {
        content.media = content.document.createElement("audio");
      }
      if (preload) {
        content.media.preload = preload;
      }
      content.media.src = fileName;
    }
  );
}

function initMediaStreamPlaybackDocument(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], async _ => {
    content.media = content.document.createElement("audio");
    content.media.srcObject = new content.AudioContext().createMediaStreamDestination().stream;
  });
}

function playMedia(tab, { resolveOnEnded } = {}) {
  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [resolveOnEnded],
    async resolveOnEnded => {
      await content.media.play();
      if (resolveOnEnded) {
        await new Promise(r => (content.media.onended = r));
      }
    }
  );
}

function pauseMedia(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], async _ => {
    content.media.pause();
  });
}

function assignNewSourceForAudio(tab, fileName) {
  return SpecialPowers.spawn(tab.linkedBrowser, [fileName], async fileName => {
    content.media.src = "";
    content.media.removeAttribute("src");
    content.media.src = fileName;
  });
}
