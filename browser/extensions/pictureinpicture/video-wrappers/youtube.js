/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class PictureInPictureVideoWrapper {
  constructor(video) {
    // Use shorts player only if video is from YouTube Shorts.
    let shortsPlayer = video.closest("#shorts-player")?.wrappedJSObject;
    let isYTShorts = !!(video.baseURI.includes("shorts") && shortsPlayer);

    this.player = isYTShorts
      ? shortsPlayer
      : video.closest("#movie_player")?.wrappedJSObject;
  }
  isLive(video) {
    return !!document.querySelector(".ytp-live");
  }
  setMuted(video, shouldMute) {
    if (this.player) {
      if (shouldMute) {
        this.player.mute();
      } else {
        this.player.unMute();
      }
    } else {
      video.muted = shouldMute;
    }
  }
  getDuration(video) {
    if (this.isLive(video)) {
      return Infinity;
    }
    return video.duration;
  }
  setCaptionContainerObserver(video, updateCaptionsFunction) {
    let container = document.getElementById("ytp-caption-window-container");

    if (container) {
      updateCaptionsFunction("");
      const callback = function (mutationsList, observer) {
        // eslint-disable-next-line no-unused-vars
        for (const mutation of mutationsList) {
          let textNodeList = container
            .querySelector(".captions-text")
            ?.querySelectorAll(".caption-visual-line");
          if (!textNodeList) {
            updateCaptionsFunction("");
            return;
          }

          updateCaptionsFunction(
            Array.from(textNodeList, x => x.textContent).join("\n")
          );
        }
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
  shouldHideToggle(video) {
    return !!video.closest(".ytd-video-preview");
  }
  setVolume(video, volume) {
    if (this.player) {
      this.player.setVolume(volume * 100);
    } else {
      video.volume = volume;
    }
  }
  getVolume(video) {
    if (this.player) {
      return this.player.getVolume() / 100;
    }
    return video.volume;
  }
}

this.PictureInPictureVideoWrapper = PictureInPictureVideoWrapper;
