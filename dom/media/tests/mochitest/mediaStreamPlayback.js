/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TIMEUPDATE_TIMEOUT_LENGTH = 10000;
const ENDED_TIMEOUT_LENGTH = 30000;

/* Time we wait for the canplaythrough event to fire
 * Note: this needs to be at least 30s because the
 *       B2G emulator in VMs is really slow. */
const CANPLAYTHROUGH_TIMEOUT_LENGTH = 60000;

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
   * Starts media with a media stream, runs it until a canplaythrough and
   * timeupdate event fires, and stops the media.
   *
   * @param {Boolean} isResume specifies if this media element is being resumed
   *                           from a previous run
   */
  playMedia : function(isResume) {
    return this.startMedia(isResume)
      .then(() => this.stopMediaElement());
  },

  /**
   * Starts the media with the associated stream.
   *
   * @param {Boolean} isResume specifies if the media element playback
   *                           is being resumed from a previous run
   */
  startMedia : function(isResume) {
    var canPlayThroughFired = false;

    // If we're initially running this media, check that the time is zero
    if (!isResume) {
      is(this.mediaStream.currentTime, 0,
         "Before starting the media element, currentTime = 0");
    }

    return new Promise((resolve, reject) => {
      /**
       * Callback fired when the canplaythrough event is fired. We only
       * run the logic of this function once, as this event can fire
       * multiple times while a HTMLMediaStream is playing content from
       * a real-time MediaStream.
       */
      var canPlayThroughCallback = () => {
        // Disable the canplaythrough event listener to prevent multiple calls
        canPlayThroughFired = true;
        this.mediaElement.removeEventListener('canplaythrough',
                                              canPlayThroughCallback, false);

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

        var timeUpdateCallback = () => {
          if (this.mediaStream.currentTime > 0 &&
              this.mediaElement.currentTime > 0) {
            this.mediaElement.removeEventListener('timeupdate',
                                                  timeUpdateCallback, false);
            resolve();
          }
        };

        // When timeupdate fires, we validate time has passed and move
        // onto the success condition
        this.mediaElement.addEventListener('timeupdate', timeUpdateCallback,
                                           false);

        // If timeupdate doesn't fire in enough time, we fail the test
        setTimeout(() => {
          this.mediaElement.removeEventListener('timeupdate',
                                                timeUpdateCallback, false);
          reject(new Error("timeUpdate event never fired"));
        }, TIMEUPDATE_TIMEOUT_LENGTH);
      };

      // Adds a listener intended to be fired when playback is available
      // without further buffering.
      this.mediaElement.addEventListener('canplaythrough', canPlayThroughCallback,
                                         false);

      // Hooks up the media stream to the media element and starts playing it
      this.mediaElement.mozSrcObject = this.mediaStream;
      this.mediaElement.play();

      // If canplaythrough doesn't fire in enough time, we fail the test
      setTimeout(() => {
        this.mediaElement.removeEventListener('canplaythrough',
                                              canPlayThroughCallback, false);
        reject(new Error("canplaythrough event never fired"));
      }, CANPLAYTHROUGH_TIMEOUT_LENGTH);
    });
  },

  /**
   * Stops the media with the associated stream.
   *
   * Precondition: The media stream and element should both be actively
   *               being played.
   */
  stopMediaElement : function() {
    this.mediaElement.pause();
    this.mediaElement.mozSrcObject = null;
  }
}


/**
 * This class is basically the same as MediaStreamPlayback except
 * ensures that the instance provided startMedia is a MediaStream.
 *
 * @param {HTMLMediaElement} mediaElement the media element for playback
 * @param {LocalMediaStream} mediaStream the media stream used in
 *                                       the mediaElement for playback
 */
function LocalMediaStreamPlayback(mediaElement, mediaStream) {
  ok(mediaStream instanceof LocalMediaStream,
     "Stream should be a LocalMediaStream");
  MediaStreamPlayback.call(this, mediaElement, mediaStream);
}

LocalMediaStreamPlayback.prototype = Object.create(MediaStreamPlayback.prototype, {

  /**
   * Starts media with a media stream, runs it until a canplaythrough and
   * timeupdate event fires, and calls stop() on the stream.
   *
   * @param {Boolean} isResume specifies if this media element is being resumed
   *                           from a previous run
   */
  playMediaWithStreamStop : {
    value: function(isResume) {
      return this.startMedia(isResume)
        .then(() => this.stopStreamInMediaPlayback())
        .then(() => this.stopMediaElement());
    }
  },

  /**
   * Stops the local media stream while it's currently in playback in
   * a media element.
   *
   * Precondition: The media stream and element should both be actively
   *               being played.
   *
   */
  stopStreamInMediaPlayback : {
    value: function () {
      return new Promise((resolve, reject) => {
        /**
         * Callback fired when the ended event fires when stop() is called on the
         * stream.
         */
        var endedCallback = () => {
          this.mediaElement.removeEventListener('ended', endedCallback, false);
          ok(true, "ended event successfully fired");
          resolve();
        };

        this.mediaElement.addEventListener('ended', endedCallback, false);
        this.mediaStream.stop();

        // If ended doesn't fire in enough time, then we fail the test
        setTimeout(() => {
          reject(new Error("ended event never fired"));
        }, ENDED_TIMEOUT_LENGTH);
      });
    }
  }
});

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

var runTest = testFunction => scriptsReady
  .then(() => runTestWhenReady(testFunction))
  .then(() => finish());
