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

// Start a generic utility process with the given array of utility actor names
// registered.
async function startUtilityProcess(actors = []) {
  info("Start a UtilityProcess");
  return utilityProcessTest().startProcess(actors);
}

// Returns an array of process infos for utility processes of the given type
// or all utility processes if actor is not defined.
async function getUtilityProcesses(actor = undefined, options = {}) {
  let procInfos = (await ChromeUtils.requestProcInfo()).children.filter(p => {
    return (
      p.type === "utility" &&
      (actor == undefined ||
        p.utilityActors.find(a => a.actorName.startsWith(actor)))
    );
  });

  if (!options?.quiet) {
    info(`Utility process infos = ${JSON.stringify(procInfos)}`);
  }
  return procInfos;
}

async function tryGetUtilityPid(actor, options = {}) {
  let process = await getUtilityProcesses(actor, options);
  if (!options?.quiet) {
    ok(process.length <= 1, `at most one ${actor} process exists`);
  }
  return process[0]?.pid;
}

async function checkUtilityExists(actor) {
  info(`Looking for a running ${actor} utility process`);
  const utilityPid = await tryGetUtilityPid(actor);
  ok(utilityPid > 0, `Found ${actor} utility process ${utilityPid}`);
  return utilityPid;
}

// "Cleanly stop" a utility process.  This will never leave a crash dump file.
// preferKill will "kill" the process (e.g. SIGABRT) instead of using the
// UtilityProcessManager.
// To "crash" -- i.e. shutdown and generate a crash dump -- use
// crashSomeUtility().
async function cleanUtilityProcessShutdown(actor, preferKill = false) {
  info(`${preferKill ? "Kill" : "Clean shutdown"} Utility Process ${actor}`);

  const utilityPid = await tryGetUtilityPid(actor);
  ok(utilityPid !== undefined, `Must have PID for ${actor} utility process`);

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
    info(`Stopping Utility Process ${utilityPid}`);
    await utilityProcessTest().stopProcess(actor);
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

  // Make sure the process is dead, otherwise there is a risk of race for
  // writing leak logs
  utilityProcessTest().noteIntentionalCrash(utilityPid);

  ok(!subject.hasKey("dumpID"), "There should be no dumpID");
}

async function killUtilityProcesses() {
  let utilityProcesses = await getUtilityProcesses();
  for (const utilityProcess of utilityProcesses) {
    for (const actor of utilityProcess.utilityActors) {
      info(`Stopping ${actor.actorName} utility process`);
      await cleanUtilityProcessShutdown(actor.actorName, /* preferKill */ true);
    }
  }
}

function audioTestData() {
  return [
    {
      src: "small-shot.ogg",
      expectations: {
        Android: {
          process: "Utility Generic",
          decoder: "ffvpx audio decoder",
        },
        Linux: {
          process: "Utility Generic",
          decoder: "ffvpx audio decoder",
        },
        WINNT: {
          process: "Utility Generic",
          decoder: "ffvpx audio decoder",
        },
        Darwin: {
          process: "Utility Generic",
          decoder: "ffvpx audio decoder",
        },
      },
    },
    {
      src: "small-shot.mp3",
      expectations: {
        Android: { process: "Utility Generic", decoder: "ffvpx audio decoder" },
        Linux: {
          process: "Utility Generic",
          decoder: "ffvpx audio decoder",
        },
        WINNT: {
          process: "Utility Generic",
          decoder: "ffvpx audio decoder",
        },
        Darwin: {
          process: "Utility Generic",
          decoder: "ffvpx audio decoder",
        },
      },
    },
    {
      src: "small-shot.m4a",
      expectations: {
        // Add Android after Bug 1771196
        Linux: {
          process: "Utility Generic",
          decoder: "ffmpeg audio decoder",
        },
        WINNT: {
          process: "Utility WMF",
          decoder: "wmf audio decoder",
        },
        Darwin: {
          process: "Utility AppleMedia",
          decoder: "apple coremedia decoder",
        },
      },
    },
    {
      src: "small-shot.flac",
      expectations: {
        Android: { process: "Utility Generic", decoder: "ffvpx audio decoder" },
        Linux: {
          process: "Utility Generic",
          decoder: "ffvpx audio decoder",
        },
        WINNT: {
          process: "Utility Generic",
          decoder: "ffvpx audio decoder",
        },
        Darwin: {
          process: "Utility Generic",
          decoder: "ffvpx audio decoder",
        },
      },
    },
  ];
}

function audioTestDataEME() {
  return [
    {
      src: {
        audioFile:
          "https://example.com/browser/ipc/glue/test/browser/short-aac-encrypted-audio.mp4",
        sourceBuffer: "audio/mp4",
      },
      expectations: {
        Linux: {
          process: "Utility Generic",
          decoder: "ffmpeg audio decoder",
        },
        WINNT: {
          process: "Utility WMF",
          decoder: "wmf audio decoder",
        },
        Darwin: {
          process: "Utility AppleMedia",
          decoder: "apple coremedia decoder",
        },
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

async function addMediaTabWithEME(sourceBuffer, audioFile) {
  const tab = BrowserTestUtils.addTab(
    gBrowser,
    "https://example.com/browser/",
    {
      forceNewProcess: true,
    }
  );
  const browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);
  await SpecialPowers.spawn(
    browser,
    [sourceBuffer, audioFile],
    createAudioElementEME
  );
  return tab;
}

async function play(
  tab,
  expectUtility,
  expectDecoder,
  expectContent = false,
  expectJava = false,
  expectError = false,
  withEME = false
) {
  let browser = tab.linkedBrowser;
  return SpecialPowers.spawn(
    browser,
    [
      expectUtility,
      expectDecoder,
      expectContent,
      expectJava,
      expectError,
      withEME,
    ],
    checkAudioDecoder
  );
}

async function stop(tab) {
  let browser = tab.linkedBrowser;
  await SpecialPowers.spawn(browser, [], async function () {
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

async function createAudioElementEME(sourceBuffer, audioFile) {
  // Helper to clone data into content so the EME helper can use the data.
  function cloneIntoContent(data) {
    return Cu.cloneInto(data, content.wrappedJSObject);
  }

  // Load the EME helper into content.
  Services.scriptloader.loadSubScript(
    "chrome://mochitests/content/browser/ipc/glue/test/browser/eme_standalone.js",
    content
  );

  let audio = content.document.createElement("audio");
  audio.setAttribute("controls", "true");
  audio.setAttribute("loop", true);
  audio.setAttribute("_sourceBufferType", sourceBuffer);
  audio.setAttribute("_audioUrl", audioFile);
  content.document.body.appendChild(audio);

  let emeHelper = new content.wrappedJSObject.EmeHelper();
  emeHelper.SetKeySystem(
    content.wrappedJSObject.EmeHelper.GetClearkeyKeySystemString()
  );
  emeHelper.SetInitDataTypes(cloneIntoContent(["keyids", "cenc"]));
  emeHelper.SetAudioCapabilities(
    cloneIntoContent([{ contentType: 'audio/mp4; codecs="mp4a.40.2"' }])
  );
  emeHelper.AddKeyIdAndKey(
    "2cdb0ed6119853e7850671c3e9906c3c",
    "808B9ADAC384DE1E4F56140F4AD76194"
  );
  emeHelper.onerror = error => {
    is(false, `Got unexpected error from EME helper: ${error}`);
  };
  await emeHelper.ConfigureEme(audio);
  // Done setting up EME.
}

async function checkAudioDecoder(
  expectedProcess,
  expectedDecoder,
  expectContent = false,
  expectJava = false,
  expectError = false,
  withEME = false
) {
  const doc = typeof content !== "undefined" ? content.document : document;
  let audio = doc.querySelector("audio");
  const checkPromise = new Promise((resolve, reject) => {
    const timeUpdateHandler = async ev => {
      const debugInfo = await SpecialPowers.wrap(audio).mozRequestDebugInfo();
      const audioDecoderName = debugInfo.decoder.reader.audioDecoderName;

      const isExpectedDecoder =
        audioDecoderName.indexOf(`${expectedDecoder}`) == 0;
      ok(
        isExpectedDecoder,
        `playback ${audio.src} was from decoder '${audioDecoderName}', expected '${expectedDecoder}'`
      );

      const isExpectedProcess =
        audioDecoderName.indexOf(`(${expectedProcess} remote)`) > 0;
      const isJavaRemote = audioDecoderName.indexOf("(remote)") > 0;
      const isOk =
        (isExpectedProcess && !isJavaRemote && !expectContent && !expectJava) || // Running in Utility
        (expectJava && !isExpectedProcess && isJavaRemote) || // Running in Java remote
        (expectContent && !isExpectedProcess && !isJavaRemote); // Running in Content

      ok(
        isOk,
        `playback ${audio.src} was from process '${audioDecoderName}', expected '${expectedProcess}'`
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

    audio.addEventListener("error", async err => {
      info(
        `Received HTML media error: ${audio.error.code}: ${audio.error.message}`
      );
      if (expectError) {
        const w = typeof content !== "undefined" ? content.window : window;
        ok(
          audio.error.code === w.MediaError.MEDIA_ERR_SRC_NOT_SUPPORTED ||
            w.MediaError.MEDIA_ERR_DECODE,
          "Media supported but decoding failed"
        );
        resolve();
      } else {
        info(`Unexpected error`);
        reject();
      }
    });

    audio.addEventListener("canplaythrough", startPlaybackHandler, {
      once: true,
    });
  });

  if (!withEME) {
    // We need to make sure the decoder is ready before play()ing otherwise we
    // could get into bad situations
    audio.load();
  } else {
    // For EME we need to create and load content ourselves. We do this here
    // because if we do it in createAudioElementEME() above then we end up
    // with events fired before we get a chance to listen to them here
    async function once(target, name) {
      return new Promise(r => target.addEventListener(name, r, { once: true }));
    }

    // Setup MSE.
    const ms = new content.wrappedJSObject.MediaSource();
    audio.src = content.wrappedJSObject.URL.createObjectURL(ms);
    await once(ms, "sourceopen");
    const sb = ms.addSourceBuffer(audio.getAttribute("_sourceBufferType"));
    let fetchResponse = await content.fetch(audio.getAttribute("_audioUrl"));
    let dataBuffer = await fetchResponse.arrayBuffer();
    sb.appendBuffer(dataBuffer);
    await once(sb, "updateend");
    ms.endOfStream();
    await once(ms, "sourceended");
  }

  return checkPromise;
}

async function runMochitestUtilityAudio(
  src,
  {
    expectUtility,
    expectDecoder,
    expectContent = false,
    expectJava = false,
    expectError = false,
  } = {}
) {
  info(`Add media: ${src}`);
  await createAudioElement(src);
  let audio = document.querySelector("audio");
  ok(audio, "Found an audio element created");

  info(`Play media: ${src}`);
  await checkAudioDecoder(
    expectUtility,
    expectDecoder,
    expectContent,
    expectJava,
    expectError
  );

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

  // Make sure the process is dead, otherwise there is a risk of race for
  // writing leak logs
  utilityProcessTest().noteIntentionalCrash(utilityPid);

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

// Crash a utility process and generate a crash dump.  To close a utility
// process (forcefully or not) without a generating a crash, use
// cleanUtilityProcessShutdown.
async function crashSomeUtilityActor(
  actor,
  actorsCheck = () => {
    return true;
  }
) {
  // Get PID for utility type
  const procInfos = await getUtilityProcesses(actor);
  ok(
    procInfos.length == 1,
    `exactly one ${actor} utility process should be found`
  );
  const utilityPid = procInfos[0].pid;
  return crashSomeUtility(utilityPid, actorsCheck);
}

function isNightlyOnly() {
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs"
  );
  return AppConstants.NIGHTLY_BUILD;
}

function isBetaOnly() {
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs"
  );
  return !AppConstants.NIGHTLY_BUILD && AppConstants.EARLY_BETA_OR_EARLIER;
}

function isNightlyOrEalyBeta() {
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs"
  );
  return AppConstants.NIGHTLY_BUILD || AppConstants.EARLY_BETA_OR_EARLIER;
}
