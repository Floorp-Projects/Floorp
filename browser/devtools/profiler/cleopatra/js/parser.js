/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Array.prototype.clone = function() { return this.slice(0); }

function makeSample(frames, extraInfo, lines) {
  return {
    frames: frames,
    extraInfo: extraInfo,
    lines: lines
  };
}

function cloneSample(sample) {
  return makeSample(sample.frames.clone(), sample.extraInfo, sample.lines.clone());
}

function bucketsBySplittingArray(array, maxItemsPerBucket) {
  var buckets = [];
  while (buckets.length * maxItemsPerBucket < array.length) {
    buckets.push(array.slice(buckets.length * maxItemsPerBucket,
                             (buckets.length + 1) * maxItemsPerBucket));
  }
  return buckets;
}

var gParserWorker = new Worker("profiler/cleopatra/js/parserWorker.js");
gParserWorker.nextRequestID = 0;

function WorkerRequest(worker) {
  var self = this;
  this._eventListeners = {};
  var requestID = worker.nextRequestID++;
  this._requestID = requestID;
  this._worker = worker;
  this._totalReporter = new ProgressReporter();
  this._totalReporter.addListener(function (reporter) {
    self._fireEvent("progress", reporter.getProgress(), reporter.getAction());
  })
  this._sendChunkReporter = this._totalReporter.addSubreporter(500);
  this._executeReporter = this._totalReporter.addSubreporter(3000);
  this._receiveChunkReporter = this._totalReporter.addSubreporter(100);
  this._totalReporter.begin("Processing task in worker...");
  var partialResult = null;
  function onMessageFromWorker(msg) {
    pendingMessages.push(msg);
    scheduleMessageProcessing();
  }
  function processMessage(msg) {
    var startTime = Date.now();
    var data = msg.data;
    var readTime = Date.now() - startTime;
    if (readTime > 10)
      console.log("reading data from worker message: " + readTime + "ms");
    if (data.requestID == requestID || !data.requestID) {
      switch(data.type) {
        case "error":
          self._sendChunkReporter.setAction("Error in worker: " + data.error);
          self._executeReporter.setAction("Error in worker: " + data.error);
          self._receiveChunkReporter.setAction("Error in worker: " + data.error);
          self._totalReporter.setAction("Error in worker: " + data.error);
          PROFILERERROR("Error in worker: " + data.error);
          self._fireEvent("error", data.error);
          break;
        case "progress":
          self._executeReporter.setProgress(data.progress);
          break;
        case "finished":
          self._executeReporter.finish();
          self._receiveChunkReporter.begin("Receiving data from worker...");
          self._receiveChunkReporter.finish();
          self._fireEvent("finished", data.result);
          worker.removeEventListener("message", onMessageFromWorker);
          break;
        case "finishedStart":
          partialResult = null;
          self._totalReceiveChunks = data.numChunks;
          self._gotReceiveChunks = 0;
          self._executeReporter.finish();
          self._receiveChunkReporter.begin("Receiving data from worker...");
          break;
        case "finishedChunk":
          partialResult = partialResult ? partialResult.concat(data.chunk) : data.chunk;
          var chunkIndex = self._gotReceiveChunks++;
          self._receiveChunkReporter.setProgress((chunkIndex + 1) / self._totalReceiveChunks);
          break;
        case "finishedEnd":
          self._receiveChunkReporter.finish();
          self._fireEvent("finished", partialResult);
          worker.removeEventListener("message", onMessageFromWorker);
          break;
      }
      // dump log if present
      if (data.log) {
        for (var line in data.log) {
          PROFILERLOG(line);
        }
      }
    }
  }
  var pendingMessages = [];
  var messageProcessingTimer = 0;
  function processMessages() {
    messageProcessingTimer = 0;
    processMessage(pendingMessages.shift());
    if (pendingMessages.length)
      scheduleMessageProcessing();
  }
  function scheduleMessageProcessing() {
    if (messageProcessingTimer)
      return;
    messageProcessingTimer = setTimeout(processMessages, 10);
  }
  worker.addEventListener("message", onMessageFromWorker);
}

WorkerRequest.prototype = {
  send: function WorkerRequest_send(task, taskData) {
    this._sendChunkReporter.begin("Sending data to worker...");
    var startTime = Date.now();
    this._worker.postMessage({
      requestID: this._requestID,
      task: task,
      taskData: taskData
    });
    var postTime = Date.now() - startTime;
    if (true || postTime > 10)
      console.log("posting message to worker: " + postTime + "ms");
    this._sendChunkReporter.finish();
    this._executeReporter.begin("Processing worker request...");
  },
  sendInChunks: function WorkerRequest_sendInChunks(task, taskData, params, maxChunkSize) {
    this._sendChunkReporter.begin("Sending data to worker...");
    var self = this;
    var chunks = bucketsBySplittingArray(taskData, maxChunkSize);
    var pendingMessages = [
      {
        requestID: this._requestID,
        task: "chunkedStart",
        numChunks: chunks.length
      }
    ].concat(chunks.map(function (chunk) {
      return {
        requestID: self._requestID,
        task: "chunkedChunk",
        chunk: chunk
      };
    })).concat([
      {
        requestID: this._requestID,
        task: "chunkedEnd"
      },
      {
        requestID: this._requestID,
        params: params,
        task: task
      },
    ]);
    var totalMessages = pendingMessages.length;
    var numSentMessages = 0;
    function postMessage(msg) {
      var msgIndex = numSentMessages++;
      var startTime = Date.now();
      self._worker.postMessage(msg);
      var postTime = Date.now() - startTime;
      if (postTime > 10)
        console.log("posting message to worker: " + postTime + "ms");
      self._sendChunkReporter.setProgress((msgIndex + 1) / totalMessages);
    }
    var messagePostingTimer = 0;
    function postMessages() {
      messagePostingTimer = 0;
      postMessage(pendingMessages.shift());
      if (pendingMessages.length) {
        scheduleMessagePosting();
      } else {
        self._sendChunkReporter.finish();
        self._executeReporter.begin("Processing worker request...");
      }
    }
    function scheduleMessagePosting() {
      if (messagePostingTimer)
        return;
      messagePostingTimer = setTimeout(postMessages, 10);
    }
    scheduleMessagePosting();
  },

  // TODO: share code with TreeView
  addEventListener: function WorkerRequest_addEventListener(eventName, callbackFunction) {
    if (!(eventName in this._eventListeners))
      this._eventListeners[eventName] = [];
    if (this._eventListeners[eventName].indexOf(callbackFunction) != -1)
      return;
    this._eventListeners[eventName].push(callbackFunction);
  },
  removeEventListener: function WorkerRequest_removeEventListener(eventName, callbackFunction) {
    if (!(eventName in this._eventListeners))
      return;
    var index = this._eventListeners[eventName].indexOf(callbackFunction);
    if (index == -1)
      return;
    this._eventListeners[eventName].splice(index, 1);
  },
  _fireEvent: function WorkerRequest__fireEvent(eventName, eventObject, p1) {
    if (!(eventName in this._eventListeners))
      return;
    this._eventListeners[eventName].forEach(function (callbackFunction) {
      callbackFunction(eventObject, p1);
    });
  },
}

var Parser = {
  parse: function Parser_parse(data, params) {
    console.log("profile num chars: " + data.length);
    var request = new WorkerRequest(gParserWorker);
    request.sendInChunks("parseRawProfile", data, params, 3000000);
    return request;
  },

  updateFilters: function Parser_updateFilters(filters) {
    var request = new WorkerRequest(gParserWorker);
    request.send("updateFilters", {
      filters: filters,
      profileID: 0
    });
    return request;
  },

  updateViewOptions: function Parser_updateViewOptions(options) {
    var request = new WorkerRequest(gParserWorker);
    request.send("updateViewOptions", {
      options: options,
      profileID: 0
    });
    return request;
  },

  getSerializedProfile: function Parser_getSerializedProfile(complete, callback) {
    var request = new WorkerRequest(gParserWorker);
    request.send("getSerializedProfile", {
      profileID: 0,
      complete: complete
    });
    request.addEventListener("finished", callback);
  },

  calculateHistogramData: function Parser_calculateHistogramData() {
    var request = new WorkerRequest(gParserWorker);
    request.send("calculateHistogramData", {
      profileID: 0
    });
    return request;
  },

  calculateDiagnosticItems: function Parser_calculateDiagnosticItems(meta) {
    var request = new WorkerRequest(gParserWorker);
    request.send("calculateDiagnosticItems", {
      profileID: 0,
      meta: meta
    });
    return request;
  },

  updateLogSetting: function Parser_updateLogSetting() {
    var request = new WorkerRequest(gParserWorker);
    request.send("initWorker", {
      debugLog: gDebugLog,
      debugTrace: gDebugTrace,
    });
    return request;
  },
};
