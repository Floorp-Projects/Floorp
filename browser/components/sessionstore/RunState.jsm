/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["RunState"];

const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm", this);

const STATE_STOPPED = 0;
const STATE_RUNNING = 1;
const STATE_QUITTING = 2;

// We're initially stopped.
let state = STATE_STOPPED;

function observer(subj, topic) {
  Services.obs.removeObserver(observer, topic);
  state = STATE_QUITTING;
}

// Listen for when the application is quitting.
Services.obs.addObserver(observer, "quit-application-granted", false);

/**
 * This module keeps track of SessionStore's current run state. We will
 * always start out at STATE_STOPPED. After the sessionw as read from disk and
 * the initial browser window has loaded we switch to STATE_RUNNING. On the
 * first notice that a browser shutdown was granted we switch to STATE_QUITTING.
 */
this.RunState = Object.freeze({
  // If we're stopped then SessionStore hasn't been initialized yet. As soon
  // as the session is read from disk and the initial browser window has loaded
  // the run state will change to STATE_RUNNING.
  get isStopped() {
    return state == STATE_STOPPED;
  },

  // STATE_RUNNING is our default mode of operation that we'll spend most of
  // the time in. After the session was read from disk and the first browser
  // window has loaded we remain running until the browser quits.
  get isRunning() {
    return state == STATE_RUNNING;
  },

  // We will enter STATE_QUITTING as soon as we receive notice that a browser
  // shutdown was granted. SessionStore will use this information to prevent
  // us from collecting partial information while the browser is shutting down
  // as well as to allow a last single write to disk and block all writes after
  // that.
  get isQuitting() {
    return state == STATE_QUITTING;
  },

  // Switch the run state to STATE_RUNNING. This must be called after the
  // session was read from, the initial browser window has loaded and we're
  // now ready to restore session data.
  setRunning() {
    if (this.isStopped) {
      state = STATE_RUNNING;
    }
  }
});
