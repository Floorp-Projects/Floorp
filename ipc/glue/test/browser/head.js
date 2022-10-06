/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const utilityProcessTest = () => {
  return Cc["@mozilla.org/utility-process-test;1"].createInstance(
    Ci.nsIUtilityProcessTest
  );
};

const kGenericUtilitySandbox = 0;
const kGenericUtilityActor = "unknown";

async function startUtilityProcess(actors) {
  info("Start a UtilityProcess");
  return utilityProcessTest().startProcess(actors);
}

async function cleanUtilityProcessShutdown(utilityPid, preferKill = false) {
  info(`CleanShutdown Utility Process ${utilityPid}`);
  ok(utilityPid !== undefined, "Utility needs to be defined");

  const utilityProcessGone = TestUtils.topicObserved(
    "ipc:utility-shutdown",
    (subject, data) => parseInt(data, 10) === utilityPid
  );

  if (preferKill) {
    SimpleTest.expectChildProcessCrash();
    info(`Kill Utility Process ${utilityPid}`);
    const ProcessTools = Cc["@mozilla.org/processtools-service;1"].getService(
      Ci.nsIProcessToolsService
    );
    ProcessTools.kill(utilityPid);
  } else {
    await utilityProcessTest().stopProcess();
  }

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

async function killPendingUtilityProcess() {
  let audioDecoderProcesses = (
    await ChromeUtils.requestProcInfo()
  ).children.filter(p => {
    return (
      p.type === "utility" &&
      p.utilityActors.find(a => a.actorName.startsWith("audioDecoder_Generic"))
    );
  });
  info(`audioDecoderProcesses=${JSON.stringify(audioDecoderProcesses)}`);
  for (let audioDecoderProcess of audioDecoderProcesses) {
    info(`Stopping audio decoder PID ${audioDecoderProcess.pid}`);
    await cleanUtilityProcessShutdown(
      audioDecoderProcess.pid,
      /* preferKill */ true
    );
  }
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

    const startPlaybackHandler = async ev => {
      ok(
        await audio.play().then(
          _ => true,
          _ => false
        ),
        "audio started playing"
      );

      audio.addEventListener("timeupdate", timeUpdateHandler, { once: true });
    };

    audio.addEventListener("canplaythrough", startPlaybackHandler, {
      once: true,
    });
  });

  // We need to make sure the decoder is ready before play()ing otherwise we
  // could get into bad situations
  audio.load();
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

async function crashSomeUtility(utilityPid, actorsCheck) {
  SimpleTest.expectChildProcessCrash();

  const crashMan = Services.crashmanager;
  const utilityProcessGone = TestUtils.topicObserved(
    "ipc:utility-shutdown",
    (subject, data) => {
      info(`ipc:utility-shutdown: data=${data} subject=${subject}`);
      return parseInt(data, 10) === utilityPid;
    }
  );

  info("prune any previous crashes");
  const future = new Date(Date.now() + 1000 * 60 * 60 * 24);
  await crashMan.pruneOldCrashes(future);

  info("crash Utility Process");
  const ProcessTools = Cc["@mozilla.org/processtools-service;1"].getService(
    Ci.nsIProcessToolsService
  );

  info(`Crash Utility Process ${utilityPid}`);
  ProcessTools.crash(utilityPid);

  info(`Waiting for utility process ${utilityPid} to go away.`);
  let [subject, data] = await utilityProcessGone;
  ok(
    parseInt(data, 10) === utilityPid,
    `Should match the crashed PID ${utilityPid} with ${data}`
  );
  ok(
    subject instanceof Ci.nsIPropertyBag2,
    "Subject needs to be a nsIPropertyBag2 to clean up properly"
  );

  const dumpID = subject.getPropertyAsAString("dumpID");
  ok(dumpID, "There should be a dumpID");

  await crashMan.ensureCrashIsPresent(dumpID);
  await crashMan.getCrashes().then(crashes => {
    is(crashes.length, 1, "There should be only one record");
    const crash = crashes[0];
    ok(
      crash.isOfType(
        crashMan.processTypes[Ci.nsIXULRuntime.PROCESS_TYPE_UTILITY],
        crashMan.CRASH_TYPE_CRASH
      ),
      "Record should be a utility process crash"
    );
    ok(crash.id === dumpID, "Record should have an ID");
    ok(
      actorsCheck(crash.metadata.UtilityActorsName),
      `Record should have the correct actors name for: ${crash.metadata.UtilityActorsName}`
    );
  });

  let minidumpDirectory = Services.dirsvc.get("ProfD", Ci.nsIFile);
  minidumpDirectory.append("minidumps");

  let dumpfile = minidumpDirectory.clone();
  dumpfile.append(dumpID + ".dmp");
  if (dumpfile.exists()) {
    info(`Removal of ${dumpfile.path}`);
    dumpfile.remove(false);
  }

  let extrafile = minidumpDirectory.clone();
  extrafile.append(dumpID + ".extra");
  info(`Removal of ${extrafile.path}`);
  if (extrafile.exists()) {
    extrafile.remove(false);
  }
}
