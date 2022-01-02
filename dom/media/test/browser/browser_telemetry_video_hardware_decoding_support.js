/**
 * This test is used to ensure that the scalar which indicates whether hardware
 * decoding is supported for a specific video codec type can be recorded
 * correctly.
 */
"use strict";

add_task(async function setupTestingPref() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // In order to test av1 in the chrome process, see https://bit.ly/3oF0oan
      ["media.rdd-process.enabled", false],
    ],
  });
});

add_task(async function testVideoCodecs() {
  // There are still other video codecs, but we only care about these popular
  // codec types.
  const testFiles = [
    { fileName: "gizmo.mp4", type: "video/avc" },
    { fileName: "gizmo.webm", type: "video/vp9" },
    { fileName: "bipbop_short_vp8.webm", type: "video/vp8" },
    { fileName: "av1.mp4", type: "video/av1" },
  ];
  const SCALAR_NAME = "media.video_hardware_decoding_support";

  for (const file of testFiles) {
    const { fileName, type } = file;
    let video = document.createElement("video");
    video.src = GetTestWebBasedURL(fileName);
    await video.play();
    let snapshot = Services.telemetry.getSnapshotForKeyedScalars("main", false)
      .parent;
    ok(
      snapshot.hasOwnProperty(SCALAR_NAME),
      `Found stored scalar '${SCALAR_NAME}'`
    );
    ok(
      snapshot[SCALAR_NAME].hasOwnProperty(type),
      `Found key '${type}' in '${SCALAR_NAME}'`
    );
    video.src = "";
    Services.telemetry.clearScalars();
  }
});

add_task(async function testAudioCodecs() {
  const testFiles = [
    "small-shot.ogg",
    "small-shot.m4a",
    "small-shot.mp3",
    "small-shot.flac",
  ];
  for (const file of testFiles) {
    let audio = document.createElement("audio");
    info(GetTestWebBasedURL(file));
    audio.src = GetTestWebBasedURL(file);
    await audio.play();
    let snapshot = Services.telemetry.getSnapshotForKeyedScalars("main", false)
      .parent;
    ok(!snapshot, `Did not record scalar for ${file}`);
    audio.src = "";
  }
});

/**
 * Return a web-based URL for a given file based on the testing directory.
 * @param {String} fileName
 *        file that caller wants its web-based url
 */
function GetTestWebBasedURL(fileName) {
  return (
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "http://example.org"
    ) + fileName
  );
}
