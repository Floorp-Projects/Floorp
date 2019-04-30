/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const ENDED_TIMEOUT_LENGTH = 30000;

/* The time we wait depends primarily on the canplaythrough event firing
 * Note: this needs to be at least 30s because the
 *       B2G emulator in VMs is really slow. */
const VERIFYPLAYING_TIMEOUT_LENGTH = 60000;

/**
 * This class manages playback of a HTMLMediaElement with a MediaStream.
 * When constructed by a caller, an object instance is created with
 * a media element and a media stream object.
 *
 * @param {HTMLMediaElement} mediaElement the media element for playback
 * @param {MediaStream} mediaStream the media stream used in
 *                                  the mediaElement for playback
 */
function MediaStreamPlayback(mediaElement, mediaStream) {
  this.mediaElement = mediaElement;
  this.mediaStream = mediaStream;
}

MediaStreamPlayback.prototype = {

   /**
   * Starts media element with a media stream, runs it until a canplaythrough
   * and timeupdate event fires, and calls stop() on all its tracks.
   *
   * @param {Boolean} isResume specifies if this media element is being resumed
   *                           from a previous run
   */
  playMedia : function(isResume) {
    this.startMedia(isResume);
    return this.verifyPlaying()
      .then(() => this.stopTracksForStreamInMediaPlayback())
      .then(() => this.detachFromMediaElement());
  },

  /**
   * Stops the local media stream's tracks while it's currently in playback in
   * a media element.
   *
   * Precondition: The media stream and element should both be actively
   *               being played. All the stream's tracks must be local.
   */
  stopTracksForStreamInMediaPlayback : function () {
    var elem = this.mediaElement;
    return Promise.all([
      haveEvent(elem, "ended", wait(ENDED_TIMEOUT_LENGTH, new Error("Timeout"))),
      ...this.mediaStream.getTracks().map(t => (t.stop(), haveNoEvent(t, "ended")))
    ]);
  },

  /**
   * Starts media with a media stream, runs it until a canplaythrough and
   * timeupdate event fires, and detaches from the element without stopping media.
   *
   * @param {Boolean} isResume specifies if this media element is being resumed
   *                           from a previous run
   */
  playMediaWithoutStoppingTracks : function(isResume) {
    this.startMedia(isResume);
    return this.verifyPlaying()
      .then(() => this.detachFromMediaElement());
  },

  /**
   * Starts the media with the associated stream.
   *
   * @param {Boolean} isResume specifies if the media element playback
   *                           is being resumed from a previous run
   */
  startMedia : function(isResume) {

    // If we're playing media element for the first time, check that time is zero.
    if (!isResume) {
      is(this.mediaElement.currentTime, 0,
         "Before starting the media element, currentTime = 0");
    }
    this.canPlayThroughFired = listenUntil(this.mediaElement, 'canplaythrough',
                                           () => true);

    // Hooks up the media stream to the media element and starts playing it
    this.mediaElement.srcObject = this.mediaStream;
    this.mediaElement.play();
  },

  /**
   * Verifies that media is playing.
   */
  verifyPlaying : function() {
    var lastElementTime = this.mediaElement.currentTime;

    var mediaTimeProgressed = listenUntil(this.mediaElement, 'timeupdate',
        () => this.mediaElement.currentTime > lastElementTime);

    return timeout(Promise.all([this.canPlayThroughFired, mediaTimeProgressed]),
                   VERIFYPLAYING_TIMEOUT_LENGTH, "verifyPlaying timed out")
      .then(() => {
        is(this.mediaElement.paused, false,
           "Media element should be playing");
        is(this.mediaElement.duration, Number.POSITIVE_INFINITY,
           "Duration should be infinity");

        // When the media element is playing with a real-time stream, we
        // constantly switch between having data to play vs. queuing up data,
        // so we can only check that the ready state is one of those two values
        ok(this.mediaElement.readyState === HTMLMediaElement.HAVE_ENOUGH_DATA ||
           this.mediaElement.readyState === HTMLMediaElement.HAVE_CURRENT_DATA,
           "Ready state shall be HAVE_ENOUGH_DATA or HAVE_CURRENT_DATA");

        is(this.mediaElement.seekable.length, 0,
           "Seekable length shall be zero");
        is(this.mediaElement.buffered.length, 0,
           "Buffered length shall be zero");

        is(this.mediaElement.seeking, false,
           "MediaElement is not seekable with MediaStream");
        ok(isNaN(this.mediaElement.startOffsetTime),
           "Start offset time shall not be a number");
        is(this.mediaElement.loop, false, "Loop shall be false");
        is(this.mediaElement.preload, "", "Preload should not exist");
        is(this.mediaElement.src, "", "No src should be defined");
        is(this.mediaElement.currentSrc, "",
           "Current src should still be an empty string");
      });
  },

  /**
   * Detaches from the element without stopping the media.
   *
   * Precondition: The media stream and element should both be actively
   *               being played.
   */
  detachFromMediaElement : function() {
    this.mediaElement.pause();
    this.mediaElement.srcObject = null;
  }
}

// haxx to prevent SimpleTest from failing at window.onload
function addLoadEvent() {}

var scriptsReady = Promise.all([
  "/tests/SimpleTest/SimpleTest.js",
  "head.js"
].map(script  => {
  var el = document.createElement("script");
  el.src = script;
  document.head.appendChild(el);
  return new Promise(r => el.onload = r);
}));

function createHTML(options) {
  return scriptsReady.then(() => realCreateHTML(options));
}

// noGum - Helper to detect whether active guM tracks still exist.
//
// It relies on the fact that, by spec, device labels from enumerateDevices are
// only visible during active gum calls. They're also visible when persistent
// permissions are granted, so turn off media.navigator.permission.disabled
// (which is normally on otherwise in our tests). Lastly, we must turn on
// media.navigator.permission.fake otherwise fake devices don't count as active.

var noGum = () => pushPrefs(["media.navigator.permission.disabled", false],
                            ["media.navigator.permission.fake", true],
                            ["media.devices.insecure.enabled", true])
  .then(() => navigator.mediaDevices.enumerateDevices())
  .then(([device]) => device &&
      is(device.label, "", "Test must leave no active gUM streams behind."));

var runTest = testFunction => scriptsReady
  .then(() => runTestWhenReady(testFunction))
  .then(() => noGum())
  .then(() => finish());
