/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["BrowserProcessChild"];

ChromeUtils.defineModuleGetter(
  this,
  "WebRTCChild",
  "resource:///actors/WebRTCChild.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "AboutHomeStartupCacheChild",
  "resource:///modules/AboutNewTabService.jsm"
);

class BrowserProcessChild extends JSProcessActorChild {
  receiveMessage(message) {
    switch (message.name) {
      case "AboutHomeStartupCache:InputStreams":
        let { pageInputStream, scriptInputStream } = message.data;
        AboutHomeStartupCacheChild.init(pageInputStream, scriptInputStream);
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
        WebRTCChild.observe(subject, topic, data);
        break;
    }
  }
}
