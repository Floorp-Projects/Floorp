/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head.js */

const Telemetry = Services.telemetry;

const { TelemetryController } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryController.sys.mjs"
);

/* eslint-disable mozilla/no-redeclare-with-import-autofix */
const { ContentTaskUtils } = ChromeUtils.importESModule(
  "resource://testing-common/ContentTaskUtils.sys.mjs"
);

const MEDIA_AUDIO_PROCESS = "media.audio_process_per_codec_name";

const utilityPerCodecs = {
  Linux: [
    {
      process: "utility+audioDecoder_Generic",
      codecs: ["vorbis", "mp3", "aac", "flac"],
    },
  ],
  WINNT: [
    {
      process: "utility+audioDecoder_Generic",
      codecs: ["vorbis", "mp3", "flac"],
    },
    {
      process: "utility+audioDecoder_WMF",
      codecs: ["aac"],
    },
  ],
  Darwin: [
    {
      process: "utility+audioDecoder_Generic",
      codecs: ["vorbis", "mp3", "flac"],
    },
    {
      process: "utility+audioDecoder_AppleMedia",
      codecs: ["aac"],
    },
  ],
};

const kInterval = 300; /* ms */
const kRetries = 5;

/**
 * This function waits until utility scalars are reported into the
 * scalar snapshot.
 */
async function waitForKeyedScalars(process) {
  await ContentTaskUtils.waitForCondition(
    () => {
      const scalars = Telemetry.getSnapshotForKeyedScalars("main", false);
      return Object.keys(scalars).includes("content");
    },
    `Waiting for ${process} scalars to have been set`,
    kInterval,
    kRetries
  );
}

async function waitForValue(process, codecNames, extra = "") {
  await ContentTaskUtils.waitForCondition(
    () => {
      const telemetry = Telemetry.getSnapshotForKeyedScalars(
        "main",
        false
      ).content;
      if (telemetry && MEDIA_AUDIO_PROCESS in telemetry) {
        const keyProcMimeTypes = Object.keys(telemetry[MEDIA_AUDIO_PROCESS]);
        const found = codecNames.every(item =>
          keyProcMimeTypes.includes(`${process},${item}${extra}`)
        );
        return found;
      }
      return false;
    },
    `Waiting for ${MEDIA_AUDIO_PROCESS} [${process}, ${codecNames}, ${extra}]`,
    kInterval,
    kRetries
  );
}

async function runTest({
  expectUtility = false,
  expectRDD = false,
  expectError = false,
}) {
  info(
    `Running tests with decoding from Utility or RDD: expectUtility=${expectUtility} expectRDD=${expectRDD} expectError=${expectError}`
  );

  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.utility-process.enabled", expectUtility],
      ["media.rdd-process.enabled", expectRDD],
      ["toolkit.telemetry.ipcBatchTimeout", 0],
    ],
  });

  const platform = Services.appinfo.OS;

  for (let { src, expectations } of audioTestData()) {
    if (!(platform in expectations)) {
      info(`Skipping ${src} for ${platform}`);
      continue;
    }

    const expectation = expectations[platform];

    info(`Add media tab: ${src}`);
    let tab = await addMediaTab(src);

    info("Play tab");
    await play(
      tab,
      expectUtility ? expectation.process : "RDD",
      expectation.decoder,
      !expectUtility && !expectRDD,
      false,
      expectError
    );

    info("Stop tab");
    await stop(tab);

    info("Remove tab");
    await BrowserTestUtils.removeTab(tab);
  }
}

async function runTestWithEME() {
  info(`Running tests with decoding from Utility for EME`);

  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.utility-process.enabled", true],
      ["toolkit.telemetry.ipcBatchTimeout", 0],
    ],
  });

  const platform = Services.appinfo.OS;

  for (let { src, expectations } of audioTestDataEME()) {
    if (!(platform in expectations)) {
      info(`Skipping ${src} for ${platform}`);
      continue;
    }

    const expectation = expectations[platform];

    info(`Add EME media tab`);
    let tab = await addMediaTabWithEME(src.sourceBuffer, src.audioFile);

    info("Play tab");
    await play(
      tab,
      expectation.process,
      expectation.decoder,
      false, // expectContent
      false, // expectJava
      false, // expectError
      true // withEME
    );

    info("Stop tab");
    await stop(tab);

    info("Remove tab");
    await BrowserTestUtils.removeTab(tab);
  }
}

function getTelemetry() {
  const telemetry = Telemetry.getSnapshotForKeyedScalars("main", false).content;
  return telemetry;
}

async function verifyTelemetryForProcess(process, codecNames, extraKey = "") {
  // Once scalars are set by the utility process, they don't immediately get
  // sent to the parent process. Wait for the Telemetry IPC Timer to trigger
  // and batch send the data back to the parent process.
  await waitForKeyedScalars(process);
  await waitForValue(process, codecNames, extraKey);

  const telemetry = getTelemetry();

  // The amount here depends on how many times RemoteAudioDecoderParent::RemoteAudioDecoderParent
  // gets called, which might be more than actual audio files being playback, e.g., we would get one for metadata loading, then one for playback etc.
  // But we dont care really we just want to ensure 0 on RDD, Content and others
  // in the wild.[${codecName}]
  codecNames.forEach(codecName => {
    Assert.equal(
      telemetry[MEDIA_AUDIO_PROCESS][`${process},${codecName}${extraKey}`],
      1,
      `${MEDIA_AUDIO_PROCESS} must have the correct value (${process}, ${codecName}).`
    );
  });
}

async function verifyNoTelemetryForProcess(process, codecNames, extraKey = "") {
  try {
    await waitForKeyedScalars(process);
    await waitForValue(process, codecNames, extraKey);
  } catch (ex) {
    if (ex.indexOf("timed out after") > 0) {
      Assert.ok(
        true,
        `Expected timeout ${process}[${MEDIA_AUDIO_PROCESS}] for ${codecNames}`
      );
    } else {
      Assert.ok(
        false,
        `Unexpected exception on ${process}[${MEDIA_AUDIO_PROCESS}] for ${codecNames}: ${ex}`
      );
    }
  }

  const telemetry = getTelemetry();

  // There could be races with telemetry for power usage coming up
  codecNames.forEach(codecName => {
    if (telemetry) {
      if (telemetry && MEDIA_AUDIO_PROCESS in telemetry) {
        Assert.ok(
          !(
            `${process},${codecName}${extraKey}` in
            telemetry[MEDIA_AUDIO_PROCESS]
          ),
          `Some telemetry but no ${process}[${MEDIA_AUDIO_PROCESS}][${codecName}]`
        );
      } else {
        Assert.ok(
          !(MEDIA_AUDIO_PROCESS in telemetry),
          `No telemetry for ${process}[${MEDIA_AUDIO_PROCESS}][${codecName}]`
        );
      }
    } else {
      Assert.equal(
        undefined,
        telemetry,
        `No telemetry for ${process}[${MEDIA_AUDIO_PROCESS}][${codecName}]`
      );
    }
  });
}

function getExtraKey({ utilityPref, rddPref, allowNonUtility }) {
  let extraKey = "";
  if (!rddPref) {
    extraKey += ",rdd-disabled";
  }
  if (!utilityPref) {
    extraKey += ",utility-disabled";
  }
  // TODO: This needs to be removed when getting rid of ability to decode on
  // non utility at all
  if (allowNonUtility || !isNightlyOrEalyBeta()) {
    extraKey += ",allow-non-utility";
  }
  return extraKey;
}
