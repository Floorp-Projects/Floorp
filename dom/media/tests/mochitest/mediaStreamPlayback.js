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
   * @param {Function} onSuccess the success callback if the media playback
   *                             start and stop cycle completes successfully
   * @param {Function} onError the error callback if the media playback
   *                           start and stop cycle fails
   */
  playMedia : function MSP_playMedia(isResume, onSuccess, onError) {
    var self = this;

    this.startMedia(isResume, function() {
      self.stopMediaElement();
      onSuccess();
    }, onError);
  },

  /**
   * Starts the media with the associated stream.
   *
   * @param {Boolean} isResume specifies if the media element playback
   *                           is being resumed from a previous run
   * @param {Function} onSuccess the success function call back
   *                             if media starts correctly
   * @param {Function} onError the error function call back
   *                           if media fails to start
   */
  startMedia : function MSP_startMedia(isResume, onSuccess, onError) {
    var self = this;
    var canPlayThroughFired = false;

    // If we're initially running this media, check that the time is zero
    if (!isResume) {
      is(this.mediaStream.currentTime, 0,
         "Before starting the media element, currentTime = 0");
    }

    /**
     * Callback fired when the canplaythrough event is fired. We only
     * run the logic of this function once, as this event can fire
     * multiple times while a HTMLMediaStream is playing content from
     * a real-time MediaStream.
     */
    var canPlayThroughCallback = function() {
      // Disable the canplaythrough event listener to prevent multiple calls
      canPlayThroughFired = true;
      self.mediaElement.removeEventListener('canplaythrough',
        canPlayThroughCallback, false);

      is(self.mediaElement.paused, false,
        "Media element should be playing");
      is(self.mediaElement.duration, Number.POSITIVE_INFINITY,
        "Duration should be infinity");

      // When the media element is playing with a real-time stream, we
      // constantly switch between having data to play vs. queuing up data,
      // so we can only check that the ready state is one of those two values
      ok(self.mediaElement.readyState === HTMLMediaElement.HAVE_ENOUGH_DATA ||
         self.mediaElement.readyState === HTMLMediaElement.HAVE_CURRENT_DATA,
         "Ready state shall be HAVE_ENOUGH_DATA or HAVE_CURRENT_DATA");

      is(self.mediaElement.seekable.length, 0,
         "Seekable length shall be zero");
      is(self.mediaElement.buffered.length, 0,
         "Buffered length shall be zero");

      is(self.mediaElement.seeking, false,
         "MediaElement is not seekable with MediaStream");
      ok(isNaN(self.mediaElement.startOffsetTime),
         "Start offset time shall not be a number");
      is(self.mediaElement.loop, false, "Loop shall be false");
      is(self.mediaElement.preload, "", "Preload should not exist");
      is(self.mediaElement.src, "", "No src should be defined");
      is(self.mediaElement.currentSrc, "",
         "Current src should still be an empty string");

      var timeUpdateFired = false;

      var timeUpdateCallback = function() {
        if (self.mediaStream.currentTime > 0 &&
            self.mediaElement.currentTime > 0) {
          timeUpdateFired = true;
          self.mediaElement.removeEventListener('timeupdate',
            timeUpdateCallback, false);
          onSuccess();
        }
      };

      // When timeupdate fires, we validate time has passed and move
      // onto the success condition
      self.mediaElement.addEventListener('timeupdate', timeUpdateCallback,
        false);

      // If timeupdate doesn't fire in enough time, we fail the test
      setTimeout(function() {
        if (!timeUpdateFired) {
          self.mediaElement.removeEventListener('timeupdate',
            timeUpdateCallback, false);
          onError("timeUpdate event never fired");
        }
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
    setTimeout(function() {
      if (!canPlayThroughFired) {
        self.mediaElement.removeEventListener('canplaythrough',
          canPlayThroughCallback, false);
        onError("canplaythrough event never fired");
      }
    }, CANPLAYTHROUGH_TIMEOUT_LENGTH);
  },

  /**
   * Stops the media with the associated stream.
   *
   * Precondition: The media stream and element should both be actively
   *               being played.
   */
  stopMediaElement : function MSP_stopMediaElement() {
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
   * @param {Function} onSuccess the success callback if the media element
   *                             successfully fires ended on a stop() call
   *                             on the stream
   * @param {Function} onError the error callback if the media element fails
   *                           to fire an ended callback on a stop() call
   *                           on the stream
   */
  playMediaWithStreamStop : {
    value: function (isResume, onSuccess, onError) {
      var self = this;

      this.startMedia(isResume, function() {
        self.stopStreamInMediaPlayback(function() {
          self.stopMediaElement();
          onSuccess();
        }, onError);
      }, onError);
    }
  },

  /**
   * Stops the local media stream while it's currently in playback in
   * a media element.
   *
   * Precondition: The media stream and element should both be actively
   *               being played.
   *
   * @param {Function} onSuccess the success callback if the media element
   *                             fires an ended event from stop() being called
   * @param {Function} onError the error callback if the media element
   *                           fails to fire an ended event from stop() being
   *                           called
   */
  stopStreamInMediaPlayback : {
    value: function (onSuccess, onError) {
      var endedFired = false;
      var self = this;

      /**
       * Callback fired when the ended event fires when stop() is called on the
       * stream.
       */
      var endedCallback = function() {
        endedFired = true;
        self.mediaElement.removeEventListener('ended', endedCallback, false);
        ok(true, "ended event successfully fired");
        onSuccess();
      };

      this.mediaElement.addEventListener('ended', endedCallback, false);
      this.mediaStream.stop();

      // If ended doesn't fire in enough time, then we fail the test
      setTimeout(function() {
        if (!endedFired) {
          onError("ended event never fired");
        }
      }, ENDED_TIMEOUT_LENGTH);
    }
  }
});
