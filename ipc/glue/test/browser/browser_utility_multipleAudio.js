/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function addMediaTab(src) {
  const tab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    forceNewProcess: true,
  });
  const browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);
  await SpecialPowers.spawn(browser, [src], async src => {
    const ROOT = "https://example.com/browser/ipc/glue/test/browser";
    let audio = content.document.createElement("audio");
    audio.setAttribute("controls", "true");
    audio.setAttribute("loop", true);
    audio.src = `${ROOT}/${src}`;
    content.document.body.appendChild(audio);
  });
  return tab;
}

async function play(tab, expectUtility) {
  let browser = tab.linkedBrowser;
  return SpecialPowers.spawn(browser, [expectUtility], async expectUtility => {
    let audio = content.document.querySelector("audio");
    const checkPromise = new Promise((resolve, reject) => {
      const timeUpdateHandler = async ev => {
        const debugInfo = await SpecialPowers.wrap(audio).mozRequestDebugInfo();
        const audioDecoderName = debugInfo.decoder.reader.audioDecoderName;
        const isUtility = audioDecoderName.indexOf("(Utility remote)") > 0;
        const isRDD = audioDecoderName.indexOf("(RDD remote)") > 0;
        const isOk = expectUtility === true ? isUtility : isRDD;

        ok(isOk, `playback was from expected decoder ${audioDecoderName}`);

        if (isOk) {
          resolve();
        } else {
          reject();
        }
      };

      audio.addEventListener("timeupdate", timeUpdateHandler, { once: true });
    });

    ok(
      await audio.play().then(
        _ => true,
        _ => false
      ),
      "audio started playing"
    );
    await checkPromise;
  });
}

async function stop(tab) {
  let browser = tab.linkedBrowser;
  await SpecialPowers.spawn(browser, [], async function() {
    let audio = content.document.querySelector("audio");
    audio.pause();
  });
}

async function runTest(expectUtility) {
  info(
    `Running tests with decoding from Utility or RDD: expectUtility=${expectUtility}`
  );

  await SpecialPowers.pushPrefEnv({
    set: [["media.utility-process.enabled", expectUtility]],
  });

  for (let src of [
    "small-shot.ogg",
    "small-shot.mp3",
    "small-shot.m4a",
    "small-shot.flac",
  ]) {
    info(`Add media tabs: ${src}`);
    let tabs = [await addMediaTab(src), await addMediaTab(src)];
    let playback = [];

    info("Play tabs");
    for (let tab of tabs) {
      playback.push(play(tab, expectUtility));
    }

    info("Wait all playback");
    await Promise.all(playback);

    let allstop = [];
    info("Stop tabs");
    for (let tab of tabs) {
      allstop.push(stop(tab));
    }

    info("Wait all stop");
    await Promise.all(allstop);

    let remove = [];
    info("Remove tabs");
    for (let tab of tabs) {
      remove.push(BrowserTestUtils.removeTab(tab));
    }

    info("Wait all tabs to be removed");
    await Promise.all(remove);
  }
}

add_task(async function testAudioDecodingInUtility() {
  await runTest(true);
});

add_task(async function testAudioDecodingInRDD() {
  await runTest(false);
});
