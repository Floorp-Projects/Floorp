/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This class manages playback of a HTMLMediaElement with a MediaStream.
 * When constructed by a caller, an object instance is created with
 * a media element and a media stream object.
 *
 * @param {HTMLMediaElement} mediaElement the media element for playback
 * @param {LocalMediaStream} mediaStream the media stream used in
 *                                       the mediaElement for playback
 */
function MediaStreamPlayback(mediaElement, mediaStream) {

  /** The HTMLMediaElement used for media playback */
  this.mediaElement = mediaElement;

  /** The LocalMediaStream used with the HTMLMediaElement */
  this.mediaStream = mediaStream;

  /**
   * Starts the media with the associated stream.
   *
   * @param {Integer} timeoutLength the timeout length to wait for
   *                                canplaythrough to fire in milliseconds
   * @param {Function} onSuccess the success function call back
   *                             if media starts correctly
   * @param {Function} onError the error function call back
   *                           if media fails to start
   */
  this.startMedia = function(timeoutLength, onSuccess, onError) {
    var self = this;
    var canPlayThroughFired = false;

    // Verifies we've received a correctly initialized LocalMediaStream
    ok(this.mediaStream instanceof LocalMediaStream,
      "Stream should be a LocalMediaStream");
    is(this.mediaStream.currentTime, 0,
      "Before starting the media element, currentTime = 0");

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
        if(self.mediaStream.currentTime > 0 &&
           self.mediaElement.currentTime > 0) {
          timeUpdateFired = true;
          self.mediaElement.removeEventListener('timeupdate', timeUpdateCallback,
            false);
          onSuccess();
        }
      };

      // When timeupdate fires, we validate time has passed and move
      // onto the success condition
      self.mediaElement.addEventListener('timeupdate', timeUpdateCallback,
        false);

      // If timeupdate doesn't fire in enough time, we fail the test
      setTimeout(function() {
        if(!timeUpdateFired) {
          self.mediaElement.removeEventListener('timeupdate',
            timeUpdateCallback, false);
          ok(false, "timeUpdate event never fired");
          onError();
        }
      }, timeoutLength);
    };

    // Adds a listener intended to be fired when playback is available
    // without further buffering.
    this.mediaElement.addEventListener('canplaythrough', canPlayThroughCallback,
      false);

    // Hooks up the media stream to the media element and starts playing it
    this.mediaElement.mozSrcObject = mediaStream;
    this.mediaElement.play();

    // If canplaythrough doesn't fire in enough time, we fail the test
    setTimeout(function() {
      if(!canPlayThroughFired) {
        self.mediaElement.removeEventListener('canplaythrough',
          canPlayThroughCallback, false);
        ok(false, "canplaythrough event never fired");
        onError();
      }
    }, timeoutLength);
  };

  /**
   * Stops the media with the associated stream.
   *
   * Precondition: The media stream and element should both be actively
   *               being played.
   */
  this.stopMedia = function() {
    this.mediaElement.pause();
    this.mediaElement.mozSrcObject = null;
  };

  /**
   * Starts media with a media stream, runs it until a canplaythrough and
   * timeupdate event fires, and stops the media.
   *
   * @param {Integer} timeoutLength the length of time to wait for certain
   *                                media events to fire
   * @param {Function} onSuccess the success callback if the media playback
   *                             start and stop cycle completes successfully
   * @param {Function} onError the error callback if the media playback
   *                           start and stop cycle fails
   */
  this.playMedia = function(timeoutLength, onSuccess, onError) {
    var self = this;

    this.startMedia(timeoutLength, function() {
      self.stopMedia();
      onSuccess();
    }, onError);
  };
}
