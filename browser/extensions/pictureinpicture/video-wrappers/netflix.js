/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class PictureInPictureVideoWrapper {
  constructor() {
    let netflixPlayerAPI =
      window.wrappedJSObject.netflix.appContext.state.playerApp.getAPI()
        .videoPlayer;
    let sessionId = null;
    for (let id of netflixPlayerAPI.getAllPlayerSessionIds()) {
      if (id.startsWith("watch-")) {
        sessionId = id;
        break;
      }
    }
    this.player = netflixPlayerAPI.getVideoPlayerBySessionId(sessionId);
  }
  /**
   * The Netflix player returns the current time in milliseconds so we convert
   * to seconds before returning.
   *
   * @param {HTMLVideoElement} video The original video element
   * @returns {number} The current time in seconds
   */
  getCurrentTime(video) {
    return this.player.getCurrentTime() / 1000;
  }
  /**
   * The Netflix player returns the duration in milliseconds so we convert to
   * seconds before returning.
   *
   * @param {HTMLVideoElement} video The original video element
   * @returns {number} The duration in seconds
   */
  getDuration(video) {
    return this.player.getDuration() / 1000;
  }
  play() {
    this.player.play();
  }
  pause() {
    this.player.pause();
  }

  setCaptionContainerObserver(video, updateCaptionsFunction) {
    let container = document.querySelector(".watch-video");

    if (container) {
      updateCaptionsFunction("");
      const callback = function (mutationsList, observer) {
        let text = container.querySelector(".player-timedtext").innerText;
        updateCaptionsFunction(text);
      };

      // immediately invoke the callback function to add subtitles to the PiP window
      callback([1], null);

      let captionsObserver = new MutationObserver(callback);

      captionsObserver.observe(container, {
        attributes: false,
        childList: true,
        subtree: true,
      });
    }
  }

  /**
   * Set the current time of the video in milliseconds.
   *
   * @param {HTMLVideoElement} video The original video element
   * @param {number} position The new time in seconds
   */
  setCurrentTime(video, position) {
    this.player.seek(position * 1000);
  }
  setVolume(video, volume) {
    this.player.setVolume(volume);
  }
  getVolume() {
    return this.player.getVolume();
  }
  setMuted(video, shouldMute) {
    this.player.setMuted(shouldMute);
  }
  isMuted() {
    return this.player.isMuted();
  }
}

this.PictureInPictureVideoWrapper = PictureInPictureVideoWrapper;
