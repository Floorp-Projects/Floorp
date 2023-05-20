/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class PictureInPictureVideoWrapper {
  constructor(video) {
    this.player = video.wrappedJSObject.__HuluDashPlayer__;
  }
  play() {
    this.player.play();
  }
  pause() {
    this.player.pause();
  }
  isMuted(video) {
    return video.volume === 0;
  }
  setMuted(video, shouldMute) {
    let muteButton = document.querySelector(".VolumeControl > div");

    if (this.isMuted(video) !== shouldMute) {
      muteButton.click();
    }
  }
  setCurrentTime(video, position) {
    this.player.currentTime = position;
  }
  setCaptionContainerObserver(video, updateCaptionsFunction) {
    let container = document.querySelector(".ClosedCaption");

    if (container) {
      updateCaptionsFunction("");
      const callback = function (mutationsList, observer) {
        // This will get the subtitles for both live and regular playback videos
        // and combine them to display. liveVideoText should be an empty string
        // when the video is regular playback and vice versa. If both
        // liveVideoText and regularVideoText are non empty strings, which
        // doesn't seem to be the case, they will both show.
        let liveVideoText = Array.from(
          container.querySelectorAll(
            "#inband-closed-caption > div > div > div"
          ),
          x => x.textContent.trim()
        )
          .filter(String)
          .join("\n");
        let regularVideoText = container.querySelector(".CaptionBox").innerText;

        updateCaptionsFunction(liveVideoText + regularVideoText);
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
  getDuration(video) {
    return this.player.duration;
  }
}

this.PictureInPictureVideoWrapper = PictureInPictureVideoWrapper;
