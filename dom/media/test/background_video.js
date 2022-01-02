/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file expects manager to be defined in the global scope.
/* global manager */
/* import-globals-from manifest.js */

"use strict";

function startTest(test) {
  info(test.desc);
  SimpleTest.waitForExplicitFinish();
  SpecialPowers.pushPrefEnv({ set: test.prefs }, () => {
    manager.runTests(test.tests, test.runTest);
  });
}

function nextVideoEnded(video) {
  return nextEvent(video, "ended");
}

function nextVideoPlaying(video) {
  return nextEvent(video, "playing");
}

function nextVideoResumes(video) {
  return nextEvent(video, "mozexitvideosuspend");
}

function nextVideoSuspends(video) {
  return nextEvent(video, "mozentervideosuspend");
}

/**
 * @param {string} url video src.
 * @returns {HTMLMediaElement} The created video element.
 */
function appendVideoToDoc(url, token, width, height) {
  // Default size of (160, 120) is used by other media tests.
  if (width === undefined) {
    width = 160;
  }
  if (height === undefined) {
    height = (3 * width) / 4;
  }

  let v = document.createElement("video");
  v.token = token;
  v.width = width;
  v.height = height;
  v.src = url;
  document.body.appendChild(v);
  return v;
}

function appendVideoToDocWithoutLoad(token, width, height) {
  // Default size of (160, 120) is used by other media tests.
  if (width === undefined) {
    width = 160;
  }
  if (height === undefined) {
    height = (3 * width) / 4;
  }

  let v = document.createElement("video");
  v.token = token;
  document.body.appendChild(v);
  v.width = width;
  v.height = height;
  return v;
}

function loadAndWaitUntilLoadedmetadata(video, url, preloadType = "metadata") {
  return new Promise((resolve, reject) => {
    video.preload = preloadType;
    video.addEventListener(
      "loadedmetadata",
      () => {
        resolve();
      },
      true
    );
    video.src = url;
  });
}

/**
 * @param {HTMLMediaElement} video Video element with under test.
 * @returns {Promise} Promise that is resolved when video 'visibilitychanged' event fires.
 */
function waitUntilVisible(video) {
  let videoChrome = SpecialPowers.wrap(video);
  if (videoChrome.isInViewPort) {
    return Promise.resolve();
  }

  return new Promise(resolve => {
    videoChrome.addEventListener("visibilitychanged", () => {
      if (videoChrome.isInViewPort) {
        ok(true, `${video.token} is visible.`);
        videoChrome.removeEventListener("visibilitychanged", this);
        resolve();
      }
    });
  });
}

/**
 * @param {HTMLMediaElement} video Video element under test.
 * @returns {Promise} Promise that is resolved when video 'playing' event fires.
 */
function waitUntilPlaying(video) {
  var p = once(video, "playing", () => {
    ok(true, `${video.token} played.`);
  });
  Log(video.token, "Start playing");
  video.play();
  return p;
}

/**
 * @param {HTMLMediaElement} video Video element under test.
 * @returns {Promise} Promise which is resolved when video 'ended' event fires.
 */
function waitUntilEnded(video) {
  Log(video.token, "Waiting for ended");
  if (video.ended) {
    ok(true, video.token + " already ended");
    return Promise.resolve();
  }

  return once(video, "ended", () => {
    ok(true, `${video.token} ended`);
  });
}

/**
 * @param {HTMLMediaElement} video Video element under test.
 * @returns {Promise} Promise that is resolved when video decode starts
 *                    suspend timer.
 */
function testSuspendTimerStartedWhenHidden(video) {
  var p = once(video, "mozstartvideosuspendtimer").then(() => {
    ok(true, `${video.token} suspend begins`);
  });
  Log(video.token, "Set Hidden");
  video.setVisible(false);
  return p;
}

/**
 * @param {HTMLMediaElement} video Video element under test.
 * @returns {Promise} Promise that is resolved when video decode suspends.
 */
function testVideoSuspendsWhenHidden(video) {
  let p = once(video, "mozentervideosuspend").then(() => {
    ok(true, `${video.token} suspends`);
  });
  Log(video.token, "Set hidden");
  video.setVisible(false);
  return p;
}

/**
 * @param {HTMLMediaElement} video Video element under test.
 * @returns {Promise} Promise that is resolved when video decode resumes.
 */
function testVideoResumesWhenShown(video) {
  var p = once(video, "mozexitvideosuspend").then(() => {
    ok(true, `${video.token} resumes`);
  });
  Log(video.token, "Set visible");
  video.setVisible(true);
  return p;
}

/**
 * @param {HTMLMediaElement} video Video element under test.
 * @returns {Promise} Promise that is resolved when video decode resumes.
 */
function testVideoOnlySeekCompletedWhenShown(video) {
  var p = once(video, "mozvideoonlyseekcompleted").then(() => {
    ok(true, `${video.token} resumes`);
  });
  Log(video.token, "Set visible");
  video.setVisible(true);
  return p;
}

/**
 * @param {HTMLVideoElement} video Video element under test.
 * @returns {Promise} Promise that is resolved if video ends and rejects if video suspends.
 */
function checkVideoDoesntSuspend(video) {
  let p = Promise.race([
    waitUntilEnded(video).then(() => {
      ok(true, `${video.token} ended before decode was suspended`);
    }),
    once(video, "mozentervideosuspend", () => {
      Promise.reject(new Error(`${video.token} suspended`));
    }),
  ]);
  Log(video.token, "Set hidden.");
  video.setVisible(false);
  return p;
}

/**
 * @param {HTMLMediaElement} video Video element under test.
 * @param {number} time video current time to wait til.
 * @returns {Promise} Promise that is resolved once currentTime passes time.
 */
function waitTil(video, time) {
  Log(video.token, `Waiting for time to reach ${time}s`);
  return new Promise(resolve => {
    video.addEventListener("timeupdate", function timeUpdateEvent() {
      if (video.currentTime > time) {
        video.removeEventListener(name, timeUpdateEvent);
        resolve();
      }
    });
  });
}
