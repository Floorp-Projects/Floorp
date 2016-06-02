/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["RunState"];

const STATE_STOPPED = 0;
const STATE_RUNNING = 1;
const STATE_QUITTING = 2;
const STATE_CLOSING = 3;
const STATE_CLOSED = 4;

// We're initially stopped.
var state = STATE_STOPPED;

/**
 * This module keeps track of SessionStore's current run state. We will
 * always start out at STATE_STOPPED. After the session was read from disk and
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
    return state >= STATE_QUITTING;
  },

  // We will enter STATE_CLOSING as soon as SessionStore is uninitialized.
  // The SessionFile module will know that a last write will happen in this
  // state and it can do some necessary cleanup.
  get isClosing() {
    return state == STATE_CLOSING;
  },

  // We will enter STATE_CLOSED as soon as SessionFile has written to disk for
  // the last time before shutdown and will not accept any further writes.
  get isClosed() {
    return state == STATE_CLOSED;
  },

  // Switch the run state to STATE_RUNNING. This must be called after the
  // session was read from, the initial browser window has loaded and we're
  // now ready to restore session data.
  setRunning() {
    if (this.isStopped) {
      state = STATE_RUNNING;
    }
  },

  // Switch the run state to STATE_CLOSING. This must be called *before* the
  // last SessionFile.write() call so that SessionFile knows we're closing and
  // can do some last cleanups and write a proper sessionstore.js file.
  setClosing() {
    if (this.isQuitting) {
      state = STATE_CLOSING;
    }
  },

  // Switch the run state to STATE_CLOSED. This must be called by SessionFile
  // after the last write to disk was accepted and no further writes will be
  // allowed. Any writes after this stage will cause exceptions.
  setClosed() {
    if (this.isClosing) {
      state = STATE_CLOSED;
    }
  },

  // Switch the run state to STATE_QUITTING. This should be called once we're
  // certain that the browser is going away and before we start collecting the
  // final window states to save in the session file.
  setQuitting() {
    if (this.isRunning) {
      state = STATE_QUITTING;
    }
  },
});
