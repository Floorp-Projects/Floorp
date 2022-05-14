/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function getAudioDecoderPid() {
  info("Finding a running AudioDecoder");
  let audioDecoderProcess = (await ChromeUtils.requestProcInfo()).children.find(
    p =>
      p.type === "utility" &&
      p.utilityActors.find(a => a.actorName === "audioDecoder")
  );
  ok(
    audioDecoderProcess,
    `Found the AudioDecoder process at ${audioDecoderProcess.pid}`
  );
  return audioDecoderProcess.pid;
}

async function crashDecoder() {
  const audioPid = await getAudioDecoderPid();
  ok(audioPid > 0, `Found an audio decoder ${audioPid}`);

  const ProcessTools = Cc["@mozilla.org/processtools-service;1"].getService(
    Ci.nsIProcessToolsService
  );
  ProcessTools.kill(audioPid);
}

async function runTest(src, withClose) {
  info(`Add media tabs: ${src}`);
  let tab = await addMediaTab(src);

  info("Play tab");
  await play(tab, true /* expectUtility */);

  info("Crash decoder");
  await crashDecoder();

  if (withClose) {
    info("Stop tab");
    await stop(tab);

    info("Remove tab");
    await BrowserTestUtils.removeTab(tab);

    info("Create tab again");
    tab = await addMediaTab(src);
  }

  info("Play tab again");
  await play(tab, true);

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

  for (let src of [
    "small-shot.ogg",
    "small-shot.mp3",
    "small-shot.m4a",
    "small-shot.flac",
  ]) {
    await runTest(src, withClose);
  }
}

add_task(async function testAudioCrashSimple() {
  await testAudioCrash(false);
});

add_task(async function testAudioCrashClose() {
  await testAudioCrash(true);
});
