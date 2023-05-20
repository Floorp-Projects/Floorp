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

const ALL_SCALAR = "media.video_hardware_decoding_support";
const HD_SCALAR = "media.video_hd_hardware_decoding_support";

add_task(async function testVideoCodecs() {
  // There are still other video codecs, but we only care about these popular
  // codec types.
  const testFiles = [
    { fileName: "gizmo.mp4", type: "video/avc" },
    { fileName: "gizmo.webm", type: "video/vp9" },
    { fileName: "bipbop_short_vp8.webm", type: "video/vp8" },
    { fileName: "av1.mp4", type: "video/av1" },
    { fileName: "bunny_hd_5s.mp4", type: "video/avc", hd: true },
  ];

  for (const file of testFiles) {
    const { fileName, type, hd } = file;
    let video = document.createElement("video");
    video.src = GetTestWebBasedURL(fileName);
    await video.play();
    let snapshot = Services.telemetry.getSnapshotForKeyedScalars(
      "main",
      false
    ).parent;
    ok(
      snapshot.hasOwnProperty(ALL_SCALAR),
      `Found stored scalar '${ALL_SCALAR}'`
    );
    ok(
      snapshot[ALL_SCALAR].hasOwnProperty(type),
      `Found key '${type}' in '${ALL_SCALAR}'`
    );
    if (hd) {
      ok(
        snapshot.hasOwnProperty(HD_SCALAR),
        `HD video '${fileName}' should record a scalar '${HD_SCALAR}'`
      );
      ok(
        snapshot[HD_SCALAR].hasOwnProperty(type),
        `Found key '${type}' in '${HD_SCALAR}'`
      );
    } else {
      ok(
        !snapshot.hasOwnProperty(HD_SCALAR),
        `SD video won't store a scalar '${HD_SCALAR}'`
      );
    }
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
    let snapshot = Services.telemetry.getSnapshotForKeyedScalars(
      "main",
      false
    ).parent;
    ok(
      !snapshot ||
        (!snapshot.hasOwnProperty(ALL_SCALAR) &&
          !snapshot.hasOwnProperty(HD_SCALAR)),
      `Did not record scalar for ${file}`
    );
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
