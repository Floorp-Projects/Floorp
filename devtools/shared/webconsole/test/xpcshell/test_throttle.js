/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* eslint-disable mozilla/use-chromeutils-generateqi */

const {
  NetworkThrottleManager,
} = require("devtools/shared/webconsole/throttle");
const nsIScriptableInputStream = Ci.nsIScriptableInputStream;

function TestStreamListener() {
  this.state = "initial";
}
TestStreamListener.prototype = {
  onStartRequest() {
    this.setState("start");
  },

  onStopRequest() {
    this.setState("stop");
  },

  onDataAvailable(request, inputStream, offset, count) {
    const sin = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(
      nsIScriptableInputStream
    );
    sin.init(inputStream);
    this.data = sin.read(count);
    this.setState("data");
  },

  setState(state) {
    this.state = state;
    if (this._deferred) {
      this._deferred.resolve(state);
      this._deferred = null;
    }
  },

  onStateChanged() {
    if (!this._deferred) {
      let resolve, reject;
      const promise = new Promise(function(res, rej) {
        resolve = res;
        reject = rej;
      });
      this._deferred = { resolve, reject, promise };
    }
    return this._deferred.promise;
  },
};

function TestChannel() {
  this.state = "initial";
  this.testListener = new TestStreamListener();
  this._throttleQueue = null;
}
TestChannel.prototype = {
  QueryInterface() {
    return this;
  },

  get throttleQueue() {
    return this._throttleQueue;
  },

  set throttleQueue(q) {
    this._throttleQueue = q;
    this.state = "throttled";
  },

  setNewListener(listener) {
    this.listener = listener;
    this.state = "listener";
    return this.testListener;
  },
};

add_task(async function() {
  const throttler = new NetworkThrottleManager({
    latencyMean: 1,
    latencyMax: 1,
    downloadBPSMean: 500,
    downloadBPSMax: 500,
    uploadBPSMean: 500,
    uploadBPSMax: 500,
  });

  const uploadChannel = new TestChannel();
  throttler.manageUpload(uploadChannel);
  equal(
    uploadChannel.state,
    "throttled",
    "NetworkThrottleManager set throttleQueue"
  );

  const downloadChannel = new TestChannel();
  const testListener = downloadChannel.testListener;

  const listener = throttler.manage(downloadChannel);
  equal(
    downloadChannel.state,
    "listener",
    "NetworkThrottleManager called setNewListener"
  );

  equal(testListener.state, "initial", "test listener in initial state");

  // This method must be passed through immediately.
  listener.onStartRequest(null);
  equal(testListener.state, "start", "test listener started");

  const TEST_INPUT = "hi bob";

  const testStream = Cc["@mozilla.org/storagestream;1"].createInstance(
    Ci.nsIStorageStream
  );
  testStream.init(512, 512);
  const out = testStream.getOutputStream(0);
  out.write(TEST_INPUT, TEST_INPUT.length);
  out.close();
  const testInputStream = testStream.newInputStream(0);

  const activityDistributor = Cc[
    "@mozilla.org/network/http-activity-distributor;1"
  ].getService(Ci.nsIHttpActivityDistributor);
  let activitySeen = false;
  listener.addActivityCallback(
    () => {
      activitySeen = true;
    },
    null,
    null,
    null,
    activityDistributor.ACTIVITY_SUBTYPE_RESPONSE_COMPLETE,
    null,
    TEST_INPUT.length,
    null
  );

  // onDataAvailable is required to immediately read the data.
  listener.onDataAvailable(null, testInputStream, 0, 6);
  equal(testInputStream.available(), 0, "no more data should be available");
  equal(
    testListener.state,
    "start",
    "test listener should not have received data"
  );
  equal(activitySeen, false, "activity not distributed yet");

  let newState = await testListener.onStateChanged();
  equal(newState, "data", "test listener received data");
  equal(testListener.data, TEST_INPUT, "test listener received all the data");
  equal(activitySeen, true, "activity has been distributed");

  const onChange = testListener.onStateChanged();
  listener.onStopRequest(null, null);
  newState = await onChange;
  equal(newState, "stop", "onStateChanged reported");
});
