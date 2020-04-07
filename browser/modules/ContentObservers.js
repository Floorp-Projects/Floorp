/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This process script is for small observers that we want to register
 * once per content process, usually in order to forward content-based
 * observer service notifications to the chrome process through
 * message passing. Using a process script avoids having them in
 * content.js and thereby registering N observers for N open tabs,
 * which is bad for perf.
 */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

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

var gEMEUIObserver = function(subject, topic, data) {
  let win = subject.top;
  let mm = getMessageManagerForWindow(win);
  if (mm) {
    mm.sendAsyncMessage("EMEVideo:ContentMediaKeysRequest", data);
  }
};

var gDecoderDoctorObserver = function(subject, topic, data) {
  let win = subject.top;
  let mm = getMessageManagerForWindow(win);
  if (mm) {
    mm.sendAsyncMessage("DecoderDoctor:Notification", data);
  }
};

function getMessageManagerForWindow(aContentWindow) {
  return aContentWindow.docShell.messageManager;
}

Services.obs.addObserver(gEMEUIObserver, "mediakeys-request");
Services.obs.addObserver(gDecoderDoctorObserver, "decoder-doctor-notification");

// WebRTCChild observer registration. Actor observers require the subject
// to be a window, so they are registered here instead.
const kWebRTCObserverTopics = [
  "getUserMedia:request",
  "recording-device-stopped",
  "PeerConnection:request",
  "recording-device-events",
  "recording-window-ended",
];

function webRTCObserve(aSubject, aTopic, aData) {
  WebRTCChild.observe(aSubject, aTopic, aData);
}

for (let topic of kWebRTCObserverTopics) {
  Services.obs.addObserver(webRTCObserve, topic);
}

if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
  Services.obs.addObserver(processShutdown, "content-child-shutdown");
}

function processShutdown() {
  for (let topic of kWebRTCObserverTopics) {
    Services.obs.removeObserver(webRTCObserve, topic);
  }
  Services.obs.removeObserver(processShutdown, "content-child-shutdown");
}

Services.cpmm.addMessageListener(
  "AboutHomeStartupCache:InputStreams",
  message => {
    let { pageInputStream, scriptInputStream } = message.data;
    AboutHomeStartupCacheChild.init(pageInputStream, scriptInputStream);
  }
);
