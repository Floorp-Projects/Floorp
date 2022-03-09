/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class PictureInPictureVideoWrapper {
  constructor() {
    let netflixPlayerAPI = window.wrappedJSObject.netflix.appContext.state.playerApp.getAPI()
      .videoPlayer;
    let sessionId = netflixPlayerAPI.getAllPlayerSessionIds()[0];
    this.player = netflixPlayerAPI.getVideoPlayerBySessionId(sessionId);
  }
  play() {
    this.player.play();
  }
  pause() {
    this.player.pause();
  }
}

this.PictureInPictureVideoWrapper = PictureInPictureVideoWrapper;
