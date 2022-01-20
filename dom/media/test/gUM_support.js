// Support script for test that use getUserMedia. This allows explicit
// configuration of prefs which affect gUM. See also
// `testing/mochitest/runtests.py` for how the harness configures values.

// Setup preconditions for tests using getUserMedia. This functions helps
// manage different prefs that affect gUM calls in tests and makes explicit
// the expected state before test runs.
async function pushGetUserMediaTestPrefs({
  fakeAudio = false,
  fakeVideo = false,
  loopbackAudio = false,
  loopbackVideo = false,
}) {
  // Make sure we have sensical arguments
  if (!fakeAudio && !loopbackAudio) {
    throw new Error(
      "pushGetUserMediaTestPrefs: Should have fake or loopback audio!"
    );
  } else if (fakeAudio && loopbackAudio) {
    throw new Error(
      "pushGetUserMediaTestPrefs: Should not have both fake and loopback audio!"
    );
  }
  if (!fakeVideo && !loopbackVideo) {
    throw new Error(
      "pushGetUserMediaTestPrefs: Should have fake or loopback video!"
    );
  } else if (fakeVideo && loopbackVideo) {
    throw new Error(
      "pushGetUserMediaTestPrefs: Should not have both fake and loopback video!"
    );
  }

  let testPrefs = [];
  if (fakeAudio) {
    // Unset the loopback device so it doesn't take precedence
    testPrefs.push(["media.audio_loopback_dev", ""]);
    // Setup fake streams pref
    testPrefs.push(["media.navigator.streams.fake", true]);
  }
  if (loopbackAudio) {
    // If audio loopback is requested we expect the test harness to have set
    // the loopback device pref, make sure it's set
    let audioLoopDev = SpecialPowers.getCharPref(
      "media.audio_loopback_dev",
      ""
    );
    if (!audioLoopDev) {
      throw new Error(
        "pushGetUserMediaTestPrefs: Loopback audio requested but " +
          "media.audio_loopback_dev does not appear to be set!"
      );
    }
  }
  if (fakeVideo) {
    // Unset the loopback device so it doesn't take precedence
    testPrefs.push(["media.video_loopback_dev", ""]);
    // Setup fake streams pref
    testPrefs.push(["media.navigator.streams.fake", true]);
  }
  if (loopbackVideo) {
    // If video loopback is requested we expect the test harness to have set
    // the loopback device pref, make sure it's set
    let videoLoopDev = SpecialPowers.getCharPref(
      "media.video_loopback_dev",
      ""
    );
    if (!videoLoopDev) {
      throw new Error(
        "pushGetUserMediaTestPrefs: Loopback video requested but " +
          "media.video_loopback_dev does not appear to be set!"
      );
    }
  }
  // Prevent presentation of the gUM permission prompt.
  testPrefs.push(["media.navigator.permission.disabled", true]);
  return SpecialPowers.pushPrefEnv({ set: testPrefs });
}

// Setup preconditions for tests using getUserMedia. This function will
// configure prefs to select loopback device(s) if it can find loopback device
// names already set in the prefs. If no loopback device name can be found then
// prefs are setup such that a fake device is used.
async function setupGetUserMediaTestPrefs() {
  let prefRequests = {};
  let audioLoopDev = SpecialPowers.getCharPref("media.audio_loopback_dev", "");
  if (audioLoopDev) {
    prefRequests.fakeAudio = false;
    prefRequests.loopbackAudio = true;
  } else {
    prefRequests.fakeAudio = true;
    prefRequests.loopbackAudio = false;
  }
  let videoLoopDev = SpecialPowers.getCharPref("media.video_loopback_dev", "");
  if (videoLoopDev) {
    prefRequests.fakeVideo = false;
    prefRequests.loopbackVideo = true;
  } else {
    prefRequests.fakeVideo = true;
    prefRequests.loopbackVideo = false;
  }
  return pushGetUserMediaTestPrefs(prefRequests);
}
