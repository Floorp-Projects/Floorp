/* global DEBUG, DEBUG_WORKER */
/* global REQUEST_GET_CURRENT_CALLS */
/* global REQUEST_ANSWER, REQUEST_CONFERENCE, REQUEST_DIAL */
/* global REQUEST_DIAL_EMERGENCY_CALL, REQUEST_HANGUP */
/* global REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND */
/* global REQUEST_HANGUP_WAITING_OR_BACKGROUND */
/* global REQUEST_SEPARATE_CONNECTION */
/* global REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE, REQUEST_UDUB */

"use strict";

(function(exports) {

  const TELEPHONY_REQUESTS = [
    REQUEST_GET_CURRENT_CALLS,
    REQUEST_ANSWER,
    REQUEST_CONFERENCE,
    REQUEST_DIAL,
    REQUEST_DIAL_EMERGENCY_CALL,
    REQUEST_HANGUP,
    REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND,
    REQUEST_HANGUP_WAITING_OR_BACKGROUND,
    REQUEST_SEPARATE_CONNECTION,
    REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE,
    REQUEST_UDUB
  ];

  // Set to true in ril_consts.js to see debug messages
  let DEBUG = DEBUG_WORKER;

  /**
   * Queue entry; only used in the queue.
   */
  let TelephonyRequestEntry = function(request, callback) {
    this.request = request;
    this.callback = callback;
  };

  let TelephonyRequestQueue = function(ril) {
    this.ril = ril;
    this.currentQueue = null;  // Point to the current running queue.

    this.queryQueue = [];
    this.controlQueue = [];
  };

  TelephonyRequestQueue.prototype._getQueue = function(request) {
    return (request === REQUEST_GET_CURRENT_CALLS) ? this.queryQueue
                                                   : this.controlQueue;
  };

  TelephonyRequestQueue.prototype._getAnotherQueue = function(queue) {
    return (this.queryQueue === queue) ? this.controlQueue : this.queryQueue;
  };

  TelephonyRequestQueue.prototype._find = function(queue, request) {
    for (let i = 0; i < queue.length; ++i) {
      if (queue[i].request === request) {
        return i;
      }
    }
    return -1;
  };

  TelephonyRequestQueue.prototype._startQueue = function(queue) {
    if (queue.length === 0) {
      return;
    }

    // We only need to keep one entry for queryQueue.
    if (queue === this.queryQueue) {
      queue.splice(1, queue.length - 1);
    }

    this.currentQueue = queue;
    for (let entry of queue) {
      this._executeEntry(entry);
    }
  };

  TelephonyRequestQueue.prototype._executeEntry = function(entry) {
    if (DEBUG) {
      this.debug("execute " + this._getRequestName(entry.request));
    }
    entry.callback();
  };

  TelephonyRequestQueue.prototype._getRequestName = function(request) {
    let method = this.ril[request];
    return (typeof method === 'function') ? method.name : "";
  };

  TelephonyRequestQueue.prototype.debug = function(msg) {
    this.ril.context.debug("[TeleQ] " + msg);
  };

  TelephonyRequestQueue.prototype.isValidRequest = function(request) {
    return TELEPHONY_REQUESTS.indexOf(request) !== -1;
  };

  TelephonyRequestQueue.prototype.push = function(request, callback) {
    if (!this.isValidRequest(request)) {
      if (DEBUG) {
        this.debug("Error: " + this._getRequestName(request) +
                   " is not a telephony request");
      }
      return;
    }

    if (DEBUG) {
      this.debug("push " + this._getRequestName(request));
    }
    let entry = new TelephonyRequestEntry(request, callback);
    let queue = this._getQueue(request);
    queue.push(entry);

    // Try to run the request.
    if (this.currentQueue === queue) {
      this._executeEntry(entry);
    } else if (!this.currentQueue) {
      this._startQueue(queue);
    }
  };

  TelephonyRequestQueue.prototype.pop = function(request) {
    if (!this.isValidRequest(request)) {
      if (DEBUG) {
        this.debug("Error: " + this._getRequestName(request) +
                   " is not a telephony request");
      }
      return;
    }

    if (DEBUG) {
      this.debug("pop " + this._getRequestName(request));
    }
    let queue = this._getQueue(request);
    let index = this._find(queue, request);
    if (index === -1) {
      throw new Error("Cannot find the request in telephonyRequestQueue.");
    } else {
      queue.splice(index, 1);
    }

    if (queue.length === 0) {
      this.currentQueue = null;
      this._startQueue(this._getAnotherQueue(queue));
    }
  };


  // Before we make sure to form it as a module would not add extra
  // overhead of module loading, we need to define it in this way
  // rather than 'module.exports' it as a module component.
  exports.TelephonyRequestQueue = TelephonyRequestQueue;
})(self); // in worker self is the global

