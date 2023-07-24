/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head-telemetry.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/ipc/glue/test/browser/head-telemetry.js",
  this
);

SimpleTest.requestCompleteLog();

add_setup(async function testNoTelemetry() {
  await Telemetry.clearScalars();
});

add_task(async function testAudioDecodingInUtility() {
  await runTestWithEME();
});

add_task(async function testUtilityTelemetry() {
  const platform = Services.appinfo.OS;
  const extraKey = getExtraKey({ rddPref: true, utilityPref: true });
  for (let exp of utilityPerCodecs[platform]) {
    if (exp.codecs.includes("aac")) {
      await verifyTelemetryForProcess(exp.process, ["aac"], extraKey);
    }
  }
});
