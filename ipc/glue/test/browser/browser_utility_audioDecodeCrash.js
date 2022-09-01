/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function getAudioDecoderPid(expectation) {
  info("Finding a running AudioDecoder");

  const actor = expectation.replace("Utility ", "");

  let audioDecoderProcess = (await ChromeUtils.requestProcInfo()).children.find(
    p =>
      p.type === "utility" &&
      p.utilityActors.find(a => a.actorName === `audioDecoder_${actor}`)
  );
  ok(
    audioDecoderProcess,
    `Found the AudioDecoder ${actor} process at ${audioDecoderProcess.pid}`
  );
  return audioDecoderProcess.pid;
}

async function crashDecoder(expectation) {
  const audioPid = await getAudioDecoderPid(expectation);
  ok(audioPid > 0, `Found an audio decoder ${audioPid}`);

  const ProcessTools = Cc["@mozilla.org/processtools-service;1"].getService(
    Ci.nsIProcessToolsService
  );
  ProcessTools.kill(audioPid);
}

async function runTest(src, withClose, expectation) {
  info(`Add media tabs: ${src}`);
  let tab = await addMediaTab(src);

  info("Play tab");
  await play(tab, expectation);

  info("Crash decoder");
  await crashDecoder(expectation);

  if (withClose) {
    info("Stop tab");
    await stop(tab);

    info("Remove tab");
    await BrowserTestUtils.removeTab(tab);

    info("Create tab again");
    tab = await addMediaTab(src);
  }

  info("Play tab again");
  await play(tab, expectation);

  info("Stop tab");
  await stop(tab);

  info("Remove tab");
  await BrowserTestUtils.removeTab(tab);
}

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.utility-process.enabled", true]],
  });
});

async function testAudioCrash(withClose) {
  info(`Running tests for audio decoder process crashing: ${withClose}`);

  SimpleTest.expectChildProcessCrash();

  const platform = Services.appinfo.OS;

  for (let { src, expectations } of audioTestData()) {
    if (!(platform in expectations)) {
      info(`Skipping ${src} for ${platform}`);
      continue;
    }

    await runTest(src, withClose, expectations[platform]);
  }
}

add_task(async function testAudioCrashSimple() {
  await testAudioCrash(false);
});

add_task(async function testAudioCrashClose() {
  await testAudioCrash(true);
});
