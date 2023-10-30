/* eslint-disable mozilla/no-arbitrary-setTimeout */

/**
 * This test is used to ensure web audio can be allowed to start when we have
 * GUM permission.
 */
add_task(async function startTestingWebAudioWithGUM() {
  info("- setup test preference -");
  await setupTestPreferences();

  info("- test web audio with gUM success -");
  await testWebAudioWithGUM({
    constraints: { audio: true },
    shouldAllowStartingContext: true,
  });
  await testWebAudioWithGUM({
    constraints: { video: true },
    shouldAllowStartingContext: true,
  });
  await testWebAudioWithGUM({
    constraints: { video: true, audio: true },
    shouldAllowStartingContext: true,
  });

  await SpecialPowers.pushPrefEnv({
    set: [["media.navigator.permission.force", true]],
  }).then(async function () {
    info("- test web audio with gUM denied -");
    await testWebAudioWithGUM({
      constraints: { video: true },
      shouldAllowStartingContext: false,
    });
    await testWebAudioWithGUM({
      constraints: { audio: true },
      shouldAllowStartingContext: false,
    });
    await testWebAudioWithGUM({
      constraints: { video: true, audio: true },
      shouldAllowStartingContext: false,
    });
  });
});

/**
 * testing helper functions
 */
function setupTestPreferences() {
  return SpecialPowers.pushPrefEnv({
    set: [
      ["media.autoplay.default", SpecialPowers.Ci.nsIAutoplay.BLOCKED],
      ["media.autoplay.blocking_policy", 0],
      ["media.autoplay.block-event.enabled", true],
      ["media.navigator.permission.fake", true],
    ],
  });
}

function createAudioContext() {
  content.ac = new content.AudioContext();
  let ac = content.ac;
  ac.resumePromises = [];
  ac.stateChangePromise = new Promise(resolve => {
    ac.addEventListener(
      "statechange",
      function () {
        resolve();
      },
      { once: true }
    );
  });
  ac.notAllowedToStart = new Promise(resolve => {
    ac.addEventListener(
      "blocked",
      function () {
        resolve();
      },
      { once: true }
    );
  });
}

async function checkingAudioContextRunningState() {
  let ac = content.ac;
  await ac.notAllowedToStart;
  ok(ac.state === "suspended", `AudioContext is not started yet.`);
}

function resumeWithoutExpectedSuccess() {
  let ac = content.ac;
  let promise = ac.resume();
  ac.resumePromises.push(promise);
  return new Promise((resolve, reject) => {
    content.setTimeout(() => {
      if (ac.state == "suspended") {
        ok(true, "audio context is still suspended");
        resolve();
      } else {
        reject("audio context should not be allowed to start");
      }
    }, 2000);
  });
}

function resumeWithExpectedSuccess() {
  let ac = content.ac;
  ac.resumePromises.push(ac.resume());
  return Promise.all(ac.resumePromises).then(() => {
    ok(ac.state == "running", "audio context starts running");
  });
}

async function callGUM(testParameters) {
  info("- calling gum with " + JSON.stringify(testParameters.constraints));
  if (testParameters.shouldAllowStartingContext) {
    // Because of the prefs we've set and passed, this is going to allow the
    // window to start an AudioContext synchronously.
    testParameters.constraints.fake = true;
    await content.navigator.mediaDevices.getUserMedia(
      testParameters.constraints
    );
    return;
  }

  // Call gUM, without sucess: we've made it so that only fake requests
  // succeed without permission, and this is requesting non-fake-devices. Return
  // a resolved promise so that the test continues, but the getUserMedia Promise
  // will never be resolved.
  // We do this to check that it's not merely calling gUM that allows starting
  // an AudioContext, it's having the Promise it return resolved successfuly,
  // because of saved permissions for an origin or explicit user consent using
  // the prompt.
  content.navigator.mediaDevices.getUserMedia(testParameters.constraints);
}

async function testWebAudioWithGUM(testParameters) {
  info("- open new tab -");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "https://example.com"
  );
  info("- create audio context -");
  await SpecialPowers.spawn(tab.linkedBrowser, [], createAudioContext);

  info("- check whether audio context starts running -");
  try {
    await SpecialPowers.spawn(
      tab.linkedBrowser,
      [],
      checkingAudioContextRunningState
    );
  } catch (error) {
    ok(false, error.toString());
  }

  try {
    await SpecialPowers.spawn(tab.linkedBrowser, [testParameters], callGUM);
  } catch (error) {
    ok(false, error.toString());
  }

  info("- calling resume() again");
  try {
    let resumeFunc = testParameters.shouldAllowStartingContext
      ? resumeWithExpectedSuccess
      : resumeWithoutExpectedSuccess;
    await SpecialPowers.spawn(tab.linkedBrowser, [], resumeFunc);
  } catch (error) {
    ok(false, error.toString());
  }

  info("- remove tab -");
  await BrowserTestUtils.removeTab(tab);
}
