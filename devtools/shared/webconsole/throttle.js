/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft= javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {CC, Ci, Cc} = require("chrome");

const ArrayBufferInputStream = CC("@mozilla.org/io/arraybuffer-input-stream;1",
                                  "nsIArrayBufferInputStream");
const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream", "setInputStream");

loader.lazyServiceGetter(this, "gActivityDistributor",
                         "@mozilla.org/network/http-activity-distributor;1",
                         "nsIHttpActivityDistributor");

const ChromeUtils = require("ChromeUtils");
const {setTimeout} = require("resource://gre/modules/Timer.jsm");

/**
 * Construct a new nsIStreamListener that buffers data and provides a
 * method to notify another listener when data is available.  This is
 * used to throttle network data on a per-channel basis.
 *
 * After construction, @see setOriginalListener must be called on the
 * new object.
 *
 * @param {NetworkThrottleQueue} queue the NetworkThrottleQueue to
 *        which status changes should be reported
 */
function NetworkThrottleListener(queue) {
  this.queue = queue;
  this.pendingData = [];
  this.pendingException = null;
  this.offset = 0;
  this.responseStarted = false;
  this.activities = {};
}

NetworkThrottleListener.prototype = {
  QueryInterface:
    ChromeUtils.generateQI([Ci.nsIStreamListener, Ci.nsIInterfaceRequestor]),

  /**
   * Set the original listener for this object.  The original listener
   * will receive requests from this object when the queue allows data
   * through.
   *
   * @param {nsIStreamListener} originalListener the original listener
   *        for the channel, to which all requests will be sent
   */
  setOriginalListener: function(originalListener) {
    this.originalListener = originalListener;
  },

  /**
   * @see nsIStreamListener.onStartRequest.
   */
  onStartRequest: function(request, context) {
    this.originalListener.onStartRequest(request, context);
    this.queue.start(this);
  },

  /**
   * @see nsIStreamListener.onStopRequest.
   */
  onStopRequest: function(request, context, statusCode) {
    this.pendingData.push({request, context, statusCode});
    this.queue.dataAvailable(this);
  },

  /**
   * @see nsIStreamListener.onDataAvailable.
   */
  onDataAvailable: function(request, context, inputStream, offset, count) {
    if (this.pendingException) {
      throw this.pendingException;
    }

    const bin = new BinaryInputStream(inputStream);
    const bytes = new ArrayBuffer(count);
    bin.readArrayBuffer(count, bytes);

    const stream = new ArrayBufferInputStream();
    stream.setData(bytes, 0, count);

    this.pendingData.push({request, context, stream, count});
    this.queue.dataAvailable(this);
  },

  /**
   * Allow some buffered data from this object to be forwarded to this
   * object's originalListener.
   *
   * @param {Number} bytesPermitted The maximum number of bytes
   *        permitted to be sent.
   * @return {Object} an object of the form {length, done}, where
   *         |length| is the number of bytes actually forwarded, and
   *         |done| is a boolean indicating whether this particular
   *         request has been completed.  (A NetworkThrottleListener
   *         may be queued multiple times, so this does not mean that
   *         all available data has been sent.)
   */
  sendSomeData: function(bytesPermitted) {
    if (this.pendingData.length === 0) {
      // Shouldn't happen.
      return {length: 0, done: true};
    }

    const {request, context, stream, count, statusCode} = this.pendingData[0];

    if (statusCode !== undefined) {
      this.pendingData.shift();
      this.originalListener.onStopRequest(request, context, statusCode);
      return {length: 0, done: true};
    }

    if (bytesPermitted > count) {
      bytesPermitted = count;
    }

    try {
      this.originalListener.onDataAvailable(request, context, stream,
                                            this.offset, bytesPermitted);
    } catch (e) {
      this.pendingException = e;
    }

    let done = false;
    if (bytesPermitted === count) {
      this.pendingData.shift();
      done = true;
    } else {
      this.pendingData[0].count -= bytesPermitted;
    }

    this.offset += bytesPermitted;
    // Maybe our state has changed enough to emit an event.
    this.maybeEmitEvents();

    return {length: bytesPermitted, done};
  },

  /**
   * Return the number of pending data requests available for this
   * listener.
   */
  pendingCount: function() {
    return this.pendingData.length;
  },

  /**
   * This is called when an http activity event is delivered.  This
   * object delays the event until the appropriate moment.
   */
  addActivityCallback: function(callback, httpActivity, channel, activityType,
                                 activitySubtype, timestamp, extraSizeData,
                                 extraStringData) {
    let datum = {callback, httpActivity, channel, activityType,
                 activitySubtype, extraSizeData,
                 extraStringData};
    this.activities[activitySubtype] = datum;

    if (activitySubtype ===
        gActivityDistributor.ACTIVITY_SUBTYPE_RESPONSE_COMPLETE) {
      this.totalSize = extraSizeData;
    }

    this.maybeEmitEvents();
  },

  /**
   * This is called for a download throttler when the latency timeout
   * has ended.
   */
  responseStart: function() {
    this.responseStarted = true;
    this.maybeEmitEvents();
  },

  /**
   * Check our internal state and emit any http activity events as
   * needed.  Note that we wait until both our internal state has
   * changed and we've received the real http activity event from
   * platform.  This approach ensures we can both pass on the correct
   * data from the original event, and update the reported time to be
   * consistent with the delay we're introducing.
   */
  maybeEmitEvents: function() {
    if (this.responseStarted) {
      this.maybeEmit(gActivityDistributor.ACTIVITY_SUBTYPE_RESPONSE_START);
      this.maybeEmit(gActivityDistributor.ACTIVITY_SUBTYPE_RESPONSE_HEADER);
    }

    if (this.totalSize !== undefined && this.offset >= this.totalSize) {
      this.maybeEmit(gActivityDistributor.ACTIVITY_SUBTYPE_RESPONSE_COMPLETE);
      this.maybeEmit(gActivityDistributor.ACTIVITY_SUBTYPE_TRANSACTION_CLOSE);
    }
  },

  /**
   * Emit an event for |code|, if the appropriate entry in
   * |activities| is defined.
   */
  maybeEmit: function(code) {
    if (this.activities[code] !== undefined) {
      let {callback, httpActivity, channel, activityType,
           activitySubtype, extraSizeData,
           extraStringData} = this.activities[code];
      let now = Date.now() * 1000;
      callback(httpActivity, channel, activityType, activitySubtype,
               now, extraSizeData, extraStringData);
      this.activities[code] = undefined;
    }
  },
};

/**
 * Construct a new queue that can be used to throttle the network for
 * a group of related network requests.
 *
 * meanBPS {Number} Mean bytes per second.
 * maxBPS {Number} Maximum bytes per second.
 * latencyMean {Number} Mean latency in milliseconds.
 * latencyMax {Number} Maximum latency in milliseconds.
 */
function NetworkThrottleQueue(meanBPS, maxBPS, latencyMean, latencyMax) {
  this.meanBPS = meanBPS;
  this.maxBPS = maxBPS;
  this.latencyMean = latencyMean;
  this.latencyMax = latencyMax;

  this.pendingRequests = new Set();
  this.downloadQueue = [];
  this.previousReads = [];

  this.pumping = false;
}

NetworkThrottleQueue.prototype = {
  /**
   * A helper function that, given a mean and a maximum, returns a
   * random integer between (mean - (max - mean)) and max.
   */
  random: function(mean, max) {
    return mean - (max - mean) + Math.floor(2 * (max - mean) * Math.random());
  },

  /**
   * A helper function that lets the indicating listener start sending
   * data.  This is called after the initial round trip time for the
   * listener has elapsed.
   */
  allowDataFrom: function(throttleListener) {
    throttleListener.responseStart();
    this.pendingRequests.delete(throttleListener);
    const count = throttleListener.pendingCount();
    for (let i = 0; i < count; ++i) {
      this.downloadQueue.push(throttleListener);
    }
    this.pump();
  },

  /**
   * Notice a new listener object.  This is called by the
   * NetworkThrottleListener when the request has started.  Initially
   * a new listener object is put into a "pending" state, until the
   * round-trip time has elapsed.  This is used to simulate latency.
   *
   * @param {NetworkThrottleListener} throttleListener the new listener
   */
  start: function(throttleListener) {
    this.pendingRequests.add(throttleListener);
    let delay = this.random(this.latencyMean, this.latencyMax);
    if (delay > 0) {
      setTimeout(() => this.allowDataFrom(throttleListener), delay);
    } else {
      this.allowDataFrom(throttleListener);
    }
  },

  /**
   * Note that new data is available for a given listener.  Each time
   * data is available, the listener will be re-queued.
   *
   * @param {NetworkThrottleListener} throttleListener the listener
   *        which has data available.
   */
  dataAvailable: function(throttleListener) {
    if (!this.pendingRequests.has(throttleListener)) {
      this.downloadQueue.push(throttleListener);
      this.pump();
    }
  },

  /**
   * An internal function that permits individual listeners to send
   * data.
   */
  pump: function() {
    // A redirect will cause two NetworkThrottleListeners to be on a
    // listener chain.  In this case, we might recursively call into
    // this method.  Avoid infinite recursion here.
    if (this.pumping) {
      return;
    }
    this.pumping = true;

    const now = Date.now();
    const oneSecondAgo = now - 1000;

    while (this.previousReads.length &&
           this.previousReads[0].when < oneSecondAgo) {
      this.previousReads.shift();
    }

    const totalBytes = this.previousReads.reduce((sum, elt) => {
      return sum + elt.numBytes;
    }, 0);

    let thisSliceBytes = this.random(this.meanBPS, this.maxBPS);
    if (totalBytes < thisSliceBytes) {
      thisSliceBytes -= totalBytes;
      let readThisTime = 0;
      while (thisSliceBytes > 0 && this.downloadQueue.length) {
        let {length, done} = this.downloadQueue[0].sendSomeData(thisSliceBytes);
        thisSliceBytes -= length;
        readThisTime += length;
        if (done) {
          this.downloadQueue.shift();
        }
      }
      this.previousReads.push({when: now, numBytes: readThisTime});
    }

    // If there is more data to download, then schedule ourselves for
    // one second after the oldest previous read.
    if (this.downloadQueue.length) {
      const when = this.previousReads[0].when + 1000;
      setTimeout(this.pump.bind(this), when - now);
    }

    this.pumping = false;
  },
};

/**
 * Construct a new object that can be used to throttle the network for
 * a group of related network requests.
 *
 * @param {Object} An object with the following attributes:
 * latencyMean {Number} Mean latency in milliseconds.
 * latencyMax {Number} Maximum latency in milliseconds.
 * downloadBPSMean {Number} Mean bytes per second for downloads.
 * downloadBPSMax {Number} Maximum bytes per second for downloads.
 * uploadBPSMean {Number} Mean bytes per second for uploads.
 * uploadBPSMax {Number} Maximum bytes per second for uploads.
 *
 * Download throttling will not be done if downloadBPSMean and
 * downloadBPSMax are <= 0.  Upload throttling will not be done if
 * uploadBPSMean and uploadBPSMax are <= 0.
 */
function NetworkThrottleManager({latencyMean, latencyMax,
                                 downloadBPSMean, downloadBPSMax,
                                 uploadBPSMean, uploadBPSMax}) {
  if (downloadBPSMax <= 0 && downloadBPSMean <= 0) {
    this.downloadQueue = null;
  } else {
    this.downloadQueue =
      new NetworkThrottleQueue(downloadBPSMean, downloadBPSMax,
                               latencyMean, latencyMax);
  }
  if (uploadBPSMax <= 0 && uploadBPSMean <= 0) {
    this.uploadQueue = null;
  } else {
    this.uploadQueue = Cc["@mozilla.org/network/throttlequeue;1"]
      .createInstance(Ci.nsIInputChannelThrottleQueue);
    this.uploadQueue.init(uploadBPSMean, uploadBPSMax);
  }
}
exports.NetworkThrottleManager = NetworkThrottleManager;

NetworkThrottleManager.prototype = {
  /**
   * Create a new NetworkThrottleListener for a given channel and
   * install it using |setNewListener|.
   *
   * @param {nsITraceableChannel} channel the channel to manage
   * @return {NetworkThrottleListener} the new listener, or null if
   *         download throttling is not being done.
   */
  manage: function(channel) {
    if (this.downloadQueue) {
      let listener = new NetworkThrottleListener(this.downloadQueue);
      let originalListener = channel.setNewListener(listener);
      listener.setOriginalListener(originalListener);
      return listener;
    }
    return null;
  },

  /**
   * Throttle uploads taking place on the given channel.
   *
   * @param {nsITraceableChannel} channel the channel to manage
   */
  manageUpload: function(channel) {
    if (this.uploadQueue) {
      channel = channel.QueryInterface(Ci.nsIThrottledInputChannel);
      channel.throttleQueue = this.uploadQueue;
    }
  },
};
