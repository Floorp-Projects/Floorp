"use strict";

/**
 * Return a web-based URL for a given file based on the testing directory.
 * @param {String} fileName
 *        file that caller wants its web-based url
 * @param {Boolean} cors [optional]
 *        if set, then return a url with different origin
 */
function GetTestWebBasedURL(fileName) {
  const origin = "https://example.com";
  return (
    getRootDirectory(gTestPath).replace("chrome://mochitests/content", origin) +
    fileName
  );
}

/**
 * Return current process Id for the Media Foundation CDM process.
 */
async function getMFCDMProcessId() {
  const process = (await ChromeUtils.requestProcInfo()).children.find(
    p =>
      p.type === "utility" &&
      p.utilityActors.find(a => a.actorName === "mfMediaEngineCDM")
  );
  return process.pid;
}

/**
 * Make the utility process with given process id crash.
 * @param {int} pid
 *        the process id for the process which is going to crash
 */
async function crashUtilityProcess(utilityPid) {
  info(`Crashing process ${utilityPid}`);
  SimpleTest.expectChildProcessCrash();

  const crashMan = Services.crashmanager;
  const utilityProcessGone = TestUtils.topicObserved(
    "ipc:utility-shutdown",
    (subject, data) => {
      info(`ipc:utility-shutdown: data=${data} subject=${subject}`);
      return parseInt(data, 10) === utilityPid;
    }
  );

  info("Prune any previous crashes");
  const future = new Date(Date.now() + 1000 * 60 * 60 * 24);
  await crashMan.pruneOldCrashes(future);

  info("Crash Utility Process");
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

/**
 * Make video in the tab play.
 * @param {object} tab
 *        the tab contains at least one video element
 */
async function playVideo(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], async _ => {
    const video = content.document.querySelector("video");
    ok(
      await video.play().then(
        () => true,
        () => false
      ),
      "video started playing"
    );
  });
}

/**
 * Check whether the video playback is performed in the right process and right decoder.
 * @param {object} tab
 *        the tab which has a playing video
 * @param {string} expectedProcess
 *        the expected process name
 * @param {string} expectedDecoder
 *        the expected decoder name
 */
async function assertRunningProcessAndDecoderName(
  tab,
  { expectedProcess, expectedDecoder } = {}
) {
  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [expectedProcess, expectedDecoder],
    // eslint-disable-next-line no-shadow
    async (expectedProcess, expectedDecoder) => {
      const video = content.document.querySelector("video");
      ok(!video.paused, "checking a playing video");

      const debugInfo = await SpecialPowers.wrap(video).mozRequestDebugInfo();
      const videoDecoderName = debugInfo.decoder.reader.videoDecoderName;

      const isExpectedDecoder =
        videoDecoderName.indexOf(`${expectedDecoder}`) == 0;
      ok(
        isExpectedDecoder,
        `Playback running by decoder '${videoDecoderName}', expected '${expectedDecoder}'`
      );

      const isExpectedProcess =
        videoDecoderName.indexOf(`(${expectedProcess} remote)`) > 0;
      ok(
        isExpectedProcess,
        `Playback running in process '${videoDecoderName}', expected '${expectedProcess}'`
      );
    }
  );
}

/**
 * Check whether the video playback is not performed in the given process and given decoder.
 * @param {object} tab
 *        the tab which has a playing video
 * @param {string} givenProcess
 *        the process name on which the video playback should not be running
 * @param {string} givenDecoder
 *        the decoder name with which the video playback should not be running
 */
async function assertNotEqualRunningProcessAndDecoderName(
  tab,
  { givenProcess, givenDecoder } = {}
) {
  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [givenProcess, givenDecoder],
    // eslint-disable-next-line no-shadow
    async (givenProcess, givenDecoder) => {
      const video = content.document.querySelector("video");
      ok(!video.paused, "checking a playing video");

      const debugInfo = await SpecialPowers.wrap(video).mozRequestDebugInfo();
      const videoDecoderName = debugInfo.decoder.reader.videoDecoderName;
      const pattern = /(.+?)\s+\((\S+)\s+remote\)/;
      const match = videoDecoderName.match(pattern);
      if (match) {
        const decoder = match[1];
        const process = match[2];
        isnot(decoder, givenDecoder, `Decoder name is not equal`);
        isnot(process, givenProcess, `Process name is not equal`);
      } else {
        ok(false, "failed to match decoder/process name?");
      }
    }
  );
}
