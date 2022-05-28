/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const utilityProcessTest = () => {
  return Cc["@mozilla.org/utility-process-test;1"].createInstance(
    Ci.nsIUtilityProcessTest
  );
};

const kGenericUtility = 0x0;

async function startUtilityProcess() {
  info("Start a UtilityProcess");
  return utilityProcessTest().startProcess();
}

async function cleanUtilityProcessShutdown(utilityPid) {
  info(`CleanShutdown Utility Process ${utilityPid}`);
  ok(utilityPid !== undefined, "Utility needs to be defined");

  const utilityProcessGone = TestUtils.topicObserved(
    "ipc:utility-shutdown",
    (subject, data) => parseInt(data, 10) === utilityPid
  );
  await utilityProcessTest().stopProcess();

  let [subject, data] = await utilityProcessGone;
  ok(
    subject instanceof Ci.nsIPropertyBag2,
    "Subject needs to be a nsIPropertyBag2 to clean up properly"
  );
  is(
    parseInt(data, 10),
    utilityPid,
    `Should match the crashed PID ${utilityPid} with ${data}`
  );

  ok(!subject.hasKey("dumpID"), "There should be no dumpID");
}

async function addMediaTab(src) {
  const tab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    forceNewProcess: true,
  });
  const browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);
  await SpecialPowers.spawn(browser, [src], createAudioElement);
  return tab;
}

async function play(tab, expectUtility, expectContent = false) {
  let browser = tab.linkedBrowser;
  return SpecialPowers.spawn(
    browser,
    [expectUtility, expectContent],
    checkAudioDecoder
  );
}

async function stop(tab) {
  let browser = tab.linkedBrowser;
  await SpecialPowers.spawn(browser, [], async function() {
    let audio = content.document.querySelector("audio");
    audio.pause();
  });
}

async function createAudioElement(src) {
  const doc = typeof content !== "undefined" ? content.document : document;
  const ROOT = "https://example.com/browser/ipc/glue/test/browser";
  let audio = doc.createElement("audio");
  audio.setAttribute("controls", "true");
  audio.setAttribute("loop", true);
  audio.src = `${ROOT}/${src}`;
  doc.body.appendChild(audio);
}

async function checkAudioDecoder(expectUtility, expectContent = false) {
  const doc = typeof content !== "undefined" ? content.document : document;
  let audio = doc.querySelector("audio");
  const checkPromise = new Promise((resolve, reject) => {
    const timeUpdateHandler = async ev => {
      const debugInfo = await SpecialPowers.wrap(audio).mozRequestDebugInfo();
      const audioDecoderName = debugInfo.decoder.reader.audioDecoderName;
      const isUtility = audioDecoderName.indexOf("(Utility remote)") > 0;
      const isRDD = audioDecoderName.indexOf("(RDD remote)") > 0;
      const isJavaRemote = audioDecoderName.indexOf("(remote)") > 0;
      const isContent = !isUtility && !isRDD;
      const isOk =
        (expectUtility === true && isUtility && !isJavaRemote) ||
        (expectUtility === false && isRDD && !isJavaRemote) ||
        (expectContent === true && isContent);

      ok(
        isOk,
        `playback ${audio.src} was from expected decoder ${audioDecoderName}`
      );

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
  return checkPromise;
}

async function runMochitestUtilityAudio(
  src,
  { expectUtility, expectContent = false } = {}
) {
  info(`Add media: ${src}`);
  await createAudioElement(src);
  let audio = document.querySelector("audio");
  ok(audio, "Found an audio element created");

  info(`Play media: ${src}`);
  await checkAudioDecoder(expectUtility, expectContent);

  info(`Pause media: ${src}`);
  await audio.pause();

  info(`Remove media: ${src}`);
  document.body.removeChild(audio);
}
