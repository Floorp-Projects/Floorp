/* Notes:
   - All times are expressed in milliseconds in this test suite.
   - Test harness code is at the end of this file.
   - We generate only one request at a time, to avoid overloading the HTTP
   request handlers.
 */

var inWorker = false;
try {
  inWorker = !(self instanceof Window);
} catch (e) {
  inWorker = true;
}

function is(got, expected, msg) {
  var obj = {};
  obj.type = "is";
  obj.got = got;
  obj.expected = expected;
  obj.msg = msg;

  self.postMessage(obj, "*");
}

function ok(bool, msg) {
  var obj = {};
  obj.type = "ok";
  obj.bool = bool;
  obj.msg = msg;

  self.postMessage(obj, "*");
}

/**
 * Generate and track results from a XMLHttpRequest with regards to timeouts.
 *
 * @param {String} id         The test description.
 * @param {Number} timeLimit  The initial setting for the request timeout.
 * @param {Number} resetAfter (Optional) The time after sending the request, to
 *                            reset the timeout.
 * @param {Number} resetTo    (Optional) The delay to reset the timeout to.
 *
 * @note The actual testing takes place in handleEvent(event).
 * The requests are generated in startXHR().
 *
 * @note If resetAfter and resetTo are omitted, only the initial timeout setting
 * applies.
 *
 * @constructor
 * @implements DOMEventListener
 */
function RequestTracker(async, id, timeLimit /*[, resetAfter, resetTo]*/) {
  this.async = async;
  this.id = id;
  this.timeLimit = timeLimit;

  if (arguments.length > 3) {
    this.mustReset  = true;
    this.resetAfter = arguments[3];
    this.resetTo    = arguments[4];
  }

  this.hasFired = false;
}
RequestTracker.prototype = {
  /**
   * Start the XMLHttpRequest!
   */
  startXHR: function() {
    var req = new XMLHttpRequest();
    this.request = req;
    req.open("GET", "file_XHR_timeout.sjs", this.async);
    var me = this;
    function handleEvent(e) { return me.handleEvent(e); };
    req.onerror = handleEvent;
    req.onload = handleEvent;
    req.onabort = handleEvent;
    req.ontimeout = handleEvent;

    req.timeout = this.timeLimit;
    
    if (this.mustReset) {
      var resetTo = this.resetTo;
      self.setTimeout(function() {
        req.timeout = resetTo;
      }, this.resetAfter);
    }

    req.send(null);
  },

  /**
   * Get a message describing this test.
   *
   * @returns {String} The test description.
   */
  getMessage: function() {
    var rv = this.id + ", ";
    if (this.mustReset) {
      rv += "original timeout at " + this.timeLimit + ", ";
      rv += "reset at " + this.resetAfter + " to " + this.resetTo;
    }
    else {
      rv += "timeout scheduled at " + this.timeLimit;
    }
    return rv;
  },

  /**
   * Check the event received, and if it's the right (and only) one we get.
   *
   * @param {DOMProgressEvent} evt An event of type "load" or "timeout".
   */
  handleEvent: function(evt) {
    if (this.hasFired) {
      ok(false, "Only one event should fire: " + this.getMessage());
      return;
    }
    this.hasFired = true;

    var type = evt.type, expectedType;
    // The XHR responds after 3000 milliseconds with a load event.
    var timeLimit = this.mustReset && (this.resetAfter < Math.min(3000, this.timeLimit)) ?
                    this.resetTo :
                    this.timeLimit;
    if ((timeLimit == 0) || (timeLimit >= 3000)) {
      expectedType = "load";
    }
    else {
      expectedType = "timeout";
    }
    is(type, expectedType, this.getMessage());
    TestCounter.testComplete();
  }
};

/**
 * Generate and track XMLHttpRequests which will have abort() called on.
 *
 * @param shouldAbort {Boolean} True if we should call abort at all.
 * @param abortDelay  {Number}  The time in ms to wait before calling abort().
 */
function AbortedRequest(shouldAbort, abortDelay) {
  this.shouldAbort = shouldAbort;
  this.abortDelay  = abortDelay;
  this.hasFired    = false;
}
AbortedRequest.prototype = {
  /**
   * Start the XMLHttpRequest!
   */
  startXHR: function() {
    var req = new XMLHttpRequest();
    this.request = req;
    req.open("GET", "file_XHR_timeout.sjs");
    var me = this;
    function handleEvent(e) { return me.handleEvent(e); };
    req.onerror = handleEvent;
    req.onload = handleEvent;
    req.onabort = handleEvent;
    req.ontimeout = handleEvent;

    req.timeout = 2000;
    var _this = this;

    function abortReq() {
      req.abort();
    }

    if (!this.shouldAbort) {
      self.setTimeout(function() {
        try {
          _this.noEventsFired();
        }
        catch (e) {
          ok(false, "Unexpected error: " + e);
          TestCounter.testComplete();
        }
      }, 5000);
    }
    else {
      // Abort events can only be triggered on sent requests.
      req.send();
      if (this.abortDelay == -1) {
        abortReq();
      }
      else {
        self.setTimeout(abortReq, this.abortDelay);
      }
    }
  },

  /**
   * Ensure that no events fired at all, especially not our timeout event.
   */
  noEventsFired: function() {
    ok(!this.hasFired, "No events should fire for an unsent, unaborted request");
    // We're done; if timeout hasn't fired by now, it never will.
    TestCounter.testComplete();
  },

  /**
   * Get a message describing this test.
   *
   * @returns {String} The test description.
   */
  getMessage: function() {
    return "time to abort is " + this.abortDelay + ", timeout set at 2000";
  },

  /**
   * Check the event received, and if it's the right (and only) one we get.
   *
   * @param {DOMProgressEvent} evt An event of type "load" or "timeout".
   */
  handleEvent: function(evt) {
    if (this.hasFired) {
      ok(false, "Only abort event should fire: " + this.getMessage());
      return;
    }
    this.hasFired = true;

    var expectedEvent = (this.abortDelay >= 2000) ? "timeout" : "abort";
    is(evt.type, expectedEvent, this.getMessage());
    TestCounter.testComplete();
  }
};

var SyncRequestSettingTimeoutAfterOpen = {
  startXHR: function() {
    var pass = false;
    var req = new XMLHttpRequest();
    req.open("GET", "file_XHR_timeout.sjs", false);
    try {
      req.timeout = 1000;
    }
    catch (e) {
      pass = true;
    }
    ok(pass, "Synchronous XHR must not allow a timeout to be set");
    TestCounter.testComplete();
  }
};

var SyncRequestSettingTimeoutBeforeOpen = {
  startXHR: function() {
    var pass = false;
    var req = new XMLHttpRequest();
    req.timeout = 1000;
    try {
      req.open("GET", "file_XHR_timeout.sjs", false);
    }
    catch (e) {
      pass = true;
    }
    ok(pass, "Synchronous XHR must not allow a timeout to be set");
    TestCounter.testComplete();
  }
};

var TestRequests = [
  // Simple timeouts.
  new RequestTracker(true, "no time out scheduled, load fires normally", 0),
  new RequestTracker(true, "load fires normally", 5000),
  new RequestTracker(true, "timeout hit before load", 2000),

  // Timeouts reset after a certain delay.
  new RequestTracker(true, "load fires normally with no timeout set, twice", 0, 2000, 0),
  new RequestTracker(true, "load fires normally with same timeout set twice", 5000, 2000, 5000),
  new RequestTracker(true, "timeout fires normally with same timeout set twice", 2000, 1000, 2000),

  new RequestTracker(true, "timeout disabled after initially set", 5000, 2000, 0),
  new RequestTracker(true, "timeout overrides load after a delay", 5000, 1000, 2000),
  new RequestTracker(true, "timeout enabled after initially disabled", 0, 2000, 5000),

  new RequestTracker(true, "timeout set to expiring value after load fires", 5000, 4000, 1000),
  new RequestTracker(true, "timeout set to expired value before load fires", 5000, 2000, 1000),
  new RequestTracker(true, "timeout set to non-expiring value after timeout fires", 1000, 2000, 5000),

  // Aborted requests.
  new AbortedRequest(false),
  new AbortedRequest(true, -1),
  new AbortedRequest(true, 5000),
];

var MainThreadTestRequests = [
  new AbortedRequest(true, 0),
  new AbortedRequest(true, 1000),

  // Synchronous requests.
  SyncRequestSettingTimeoutAfterOpen,
  SyncRequestSettingTimeoutBeforeOpen
];

var WorkerThreadTestRequests = [
  // Simple timeouts.
  new RequestTracker(false, "no time out scheduled, load fires normally", 0),
  new RequestTracker(false, "load fires normally", 5000),
  new RequestTracker(false, "timeout hit before load", 2000),

  // Reset timeouts don't make much sense with a sync request ...
];

if (inWorker) {
  TestRequests = TestRequests.concat(WorkerThreadTestRequests);
} else {
  TestRequests = TestRequests.concat(MainThreadTestRequests);
}

// This code controls moving from one test to another.
var TestCounter = {
  testComplete: function() {
    // Allow for the possibility there are other events coming.
    self.setTimeout(function() {
      TestCounter.next();
    }, 5000);
  },

  next: function() {
    var test = TestRequests.shift();

    if (test) {
      test.startXHR();
    }
    else {
      self.postMessage("done", "*");
    }
  }
};

self.addEventListener("message", function (event) {
  if (event.data == "start") {
    TestCounter.next();
  }
});
