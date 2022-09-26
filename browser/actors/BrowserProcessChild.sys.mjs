/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "WebRTCChild",
  "resource:///actors/WebRTCChild.jsm"
);

ChromeUtils.defineModuleGetter(
  lazy,
  "AboutHomeStartupCacheChild",
  "resource:///modules/AboutNewTabService.jsm"
);

export class BrowserProcessChild extends JSProcessActorChild {
  receiveMessage(message) {
    switch (message.name) {
      case "AboutHomeStartupCache:InputStreams":
        let { pageInputStream, scriptInputStream } = message.data;
        lazy.AboutHomeStartupCacheChild.init(
          pageInputStream,
          scriptInputStream
        );
        break;
    }
  }

  observe(subject, topic, data) {
    switch (topic) {
      case "getUserMedia:request":
      case "recording-device-stopped":
      case "PeerConnection:request":
      case "recording-device-events":
      case "recording-window-ended":
        lazy.WebRTCChild.observe(subject, topic, data);
        break;
    }
  }
}
