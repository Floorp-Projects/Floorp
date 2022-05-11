/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class PictureInPictureVideoWrapper {
  constructor() {
    let netflixPlayerAPI = window.wrappedJSObject.netflix.appContext.state.playerApp.getAPI()
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
  getCurrentTime(video) {
    return this.player.getCurrentTime();
  }
  getDuration(video) {
    return this.player.getDuration();
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
      const callback = function(mutationsList, observer) {
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

  setCurrentTime(video, position) {
    let oldTime = this.player.getCurrentTime();
    let duration = this.player.getDuration();
    let isHome = position == 0;
    let isEnd = position >= duration;
    // Read pipChild's expected seek result to determine if we want
    // to move forward/backwards, or go to the start/end
    let seekDirection = position - oldTime;
    // But ignore pipChild's proposed seek forward/backward time for a better viewing
    // experience. 10 seconds (10000ms) seems to be the best value for compatibility.
    // The new currentTime will not always be 10 seconds forward/backward though, since Netflix
    // adjusts the new currentTime when seek() is called, according to the current timestamp.
    let seekTimeMS = 10000;
    let newTime = 0;

    if (isHome || isEnd) {
      newTime = position;
    } else if (seekDirection < 0) {
      newTime = Math.max(oldTime - seekTimeMS, 0);
    } else if (seekDirection > 0) {
      newTime = Math.min(oldTime + seekTimeMS, duration);
    }
    this.player.seek(newTime);
  }
}

this.PictureInPictureVideoWrapper = PictureInPictureVideoWrapper;
