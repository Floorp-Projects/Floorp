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

function audioTestData() {
  return [
    {
      src: "small-shot.ogg",
      expectations: {
        Android: "Utility Generic",
        Linux: "Utility Generic",
        WINNT: "Utility Generic",
        Darwin: "Utility Generic",
      },
    },
    {
      src: "small-shot.mp3",
      expectations: {
        Android: "Utility Generic",
        Linux: "Utility Generic",
        WINNT: SpecialPowers.getBoolPref("media.ffvpx.mp3.enabled")
          ? "Utility Generic"
          : "Utility WMF",
        Darwin: "Utility Generic",
      },
    },
    {
      src: "small-shot.m4a",
      expectations: {
        // Add Android after Bug 1771196
        Linux: "Utility Generic",
        WINNT: "Utility WMF",
        Darwin: "Utility AppleMedia",
      },
    },
    {
      src: "small-shot.flac",
      expectations: {
        Android: "Utility Generic",
        Linux: "Utility Generic",
        WINNT: "Utility Generic",
        Darwin: "Utility Generic",
      },
    },
  ];
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

async function play(
  tab,
  expectUtility,
  expectContent = false,
  expectJava = false
) {
  let browser = tab.linkedBrowser;
  return SpecialPowers.spawn(
    browser,
    [expectUtility, expectContent, expectJava],
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

async function checkAudioDecoder(
  expectedProcess,
  expectContent = false,
  expectJava = false
) {
  const doc = typeof content !== "undefined" ? content.document : document;
  let audio = doc.querySelector("audio");
  const checkPromise = new Promise((resolve, reject) => {
    const timeUpdateHandler = async ev => {
      const debugInfo = await SpecialPowers.wrap(audio).mozRequestDebugInfo();
      const audioDecoderName = debugInfo.decoder.reader.audioDecoderName;
      const isExpected =
        audioDecoderName.indexOf(`(${expectedProcess} remote)`) > 0;
      const isJavaRemote = audioDecoderName.indexOf("(remote)") > 0;
      const isOk =
        (isExpected && !isJavaRemote && !expectContent && !expectJava) || // Running in Utility/RDD
        (expectJava && !isExpected && isJavaRemote) || // Running in Java remote
        (expectContent && !isExpected && !isJavaRemote); // Running in Content

      ok(
        isOk,
        `playback ${audio.src} was from decoder '${audioDecoderName}', expected '${expectedProcess}'`
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
  { expectUtility, expectContent = false, expectJava = false } = {}
) {
  info(`Add media: ${src}`);
  await createAudioElement(src);
  let audio = document.querySelector("audio");
  ok(audio, "Found an audio element created");

  info(`Play media: ${src}`);
  await checkAudioDecoder(expectUtility, expectContent, expectJava);

  info(`Pause media: ${src}`);
  await audio.pause();

  info(`Remove media: ${src}`);
  document.body.removeChild(audio);
}
