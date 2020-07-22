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

Services.obs.addObserver(gDecoderDoctorObserver, "decoder-doctor-notification");
