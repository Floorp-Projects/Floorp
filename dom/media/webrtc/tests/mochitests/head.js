/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Cc = SpecialPowers.Cc;
var Ci = SpecialPowers.Ci;

// Specifies if we want fake audio streams for this run
let WANT_FAKE_AUDIO = true;
// Specifies if we want fake video streams for this run
let WANT_FAKE_VIDEO = true;
let TEST_AUDIO_FREQ = 1000;

/**
 * Reads the current values of preferences affecting fake and loopback devices
 * and sets the WANT_FAKE_AUDIO and WANT_FAKE_VIDEO gloabals appropriately.
 */
function updateConfigFromFakeAndLoopbackPrefs() {
  let audioDevice = SpecialPowers.getCharPref("media.audio_loopback_dev", "");
  if (audioDevice) {
    WANT_FAKE_AUDIO = false;
    dump("TEST DEVICES: Got loopback audio: " + audioDevice + "\n");
  } else {
    WANT_FAKE_AUDIO = true;
    dump(
      "TEST DEVICES: No test device found in media.audio_loopback_dev, using fake audio streams.\n"
    );
  }
  let videoDevice = SpecialPowers.getCharPref("media.video_loopback_dev", "");
  if (videoDevice) {
    WANT_FAKE_VIDEO = false;
    dump("TEST DEVICES: Got loopback video: " + videoDevice + "\n");
  } else {
    WANT_FAKE_VIDEO = true;
    dump(
      "TEST DEVICES: No test device found in media.video_loopback_dev, using fake video streams.\n"
    );
  }
}

updateConfigFromFakeAndLoopbackPrefs();

/**
 * Helper class to setup a sine tone of a given frequency.
 */
class LoopbackTone {
  constructor(audioContext, frequency) {
    if (!audioContext) {
      throw new Error("You must provide a valid AudioContext");
    }
    this.oscNode = audioContext.createOscillator();
    var gainNode = audioContext.createGain();
    gainNode.gain.value = 0.5;
    this.oscNode.connect(gainNode);
    gainNode.connect(audioContext.destination);
    this.changeFrequency(frequency);
  }

  // Method should be used when WANT_FAKE_AUDIO is false.
  start() {
    if (!this.oscNode) {
      throw new Error("Attempt to start a stopped LoopbackTone");
    }
    info(`Start loopback tone at ${this.oscNode.frequency.value}`);
    this.oscNode.start();
  }

  // Change the frequency of the tone. It can be used after start.
  // Frequency will change on the fly. No need to stop and create a new instance.
  changeFrequency(frequency) {
    if (!this.oscNode) {
      throw new Error("Attempt to change frequency on a stopped LoopbackTone");
    }
    this.oscNode.frequency.value = frequency;
  }

  stop() {
    if (!this.oscNode) {
      throw new Error("Attempt to stop a stopped LoopbackTone");
    }
    this.oscNode.stop();
    this.oscNode = null;
  }
}
// Object that holds the default loopback tone.
var DefaultLoopbackTone = null;

/**
 * This class provides helpers around analysing the audio content in a stream
 * using WebAudio AnalyserNodes.
 *
 * @constructor
 * @param {object} stream
 *                 A MediaStream object whose audio track we shall analyse.
 */
function AudioStreamAnalyser(ac, stream) {
  this.audioContext = ac;
  this.stream = stream;
  this.sourceNodes = [];
  this.analyser = this.audioContext.createAnalyser();
  // Setting values lower than default for speedier testing on emulators
  this.analyser.smoothingTimeConstant = 0.2;
  this.analyser.fftSize = 1024;
  this.connectTrack = t => {
    let source = this.audioContext.createMediaStreamSource(
      new MediaStream([t])
    );
    this.sourceNodes.push(source);
    source.connect(this.analyser);
  };
  this.stream.getAudioTracks().forEach(t => this.connectTrack(t));
  this.onaddtrack = ev => this.connectTrack(ev.track);
  this.stream.addEventListener("addtrack", this.onaddtrack);
  this.data = new Uint8Array(this.analyser.frequencyBinCount);
}

AudioStreamAnalyser.prototype = {
  /**
   * Get an array of frequency domain data for our stream's audio track.
   *
   * @returns {array} A Uint8Array containing the frequency domain data.
   */
  getByteFrequencyData() {
    this.analyser.getByteFrequencyData(this.data);
    return this.data;
  },

  /**
   * Append a canvas to the DOM where the frequency data are drawn.
   * Useful to debug tests.
   */
  enableDebugCanvas() {
    var cvs = (this.debugCanvas = document.createElement("canvas"));
    const content = document.getElementById("content");
    content.insertBefore(cvs, content.children[0]);

    // Easy: 1px per bin
    cvs.width = this.analyser.frequencyBinCount;
    cvs.height = 128;
    cvs.style.border = "1px solid red";

    var c = cvs.getContext("2d");
    c.fillStyle = "black";

    var self = this;
    function render() {
      c.clearRect(0, 0, cvs.width, cvs.height);
      var array = self.getByteFrequencyData();
      for (var i = 0; i < array.length; i++) {
        c.fillRect(i, cvs.height - array[i] / 2, 1, cvs.height);
      }
      if (!cvs.stopDrawing) {
        requestAnimationFrame(render);
      }
    }
    requestAnimationFrame(render);
  },

  /**
   * Stop drawing of and remove the debug canvas from the DOM if it was
   * previously added.
   */
  disableDebugCanvas() {
    if (!this.debugCanvas || !this.debugCanvas.parentElement) {
      return;
    }

    this.debugCanvas.stopDrawing = true;
    this.debugCanvas.parentElement.removeChild(this.debugCanvas);
  },

  /**
   * Disconnects the input stream from our internal analyser node.
   * Call this to reduce main thread processing, mostly necessary on slow
   * devices.
   */
  disconnect() {
    this.disableDebugCanvas();
    this.sourceNodes.forEach(n => n.disconnect());
    this.sourceNodes = [];
    this.stream.removeEventListener("addtrack", this.onaddtrack);
  },

  /**
   * Return a Promise, that will be resolved when the function passed as
   * argument, when called, returns true (meaning the analysis was a
   * success). The promise is rejected if the cancel promise resolves first.
   *
   * @param {function} analysisFunction
   *        A function that performs an analysis, and resolves with true if the
   *        analysis was a success (i.e. it found what it was looking for)
   * @param {promise} cancel
   *        A promise that on resolving will reject the promise we returned.
   */
  async waitForAnalysisSuccess(
    analysisFunction,
    cancel = wait(60000, new Error("Audio analysis timed out"))
  ) {
    let aborted = false;
    cancel.then(() => (aborted = true));

    // We need to give the Analyser some time to start gathering data.
    await wait(200);

    do {
      await new Promise(resolve => requestAnimationFrame(resolve));
      if (aborted) {
        throw await cancel;
      }
    } while (!analysisFunction(this.getByteFrequencyData()));
  },

  /**
   * Return the FFT bin index for a given frequency.
   *
   * @param {double} frequency
   *        The frequency for whicht to return the bin number.
   * @returns {integer} the index of the bin in the FFT array.
   */
  binIndexForFrequency(frequency) {
    return Math.round(
      (frequency * this.analyser.fftSize) / this.audioContext.sampleRate
    );
  },

  /**
   * Reverse operation, get the frequency for a bin index.
   *
   * @param {integer} index an index in an FFT array
   * @returns {double} the frequency for this bin
   */
  frequencyForBinIndex(index) {
    return (index * this.audioContext.sampleRate) / this.analyser.fftSize;
  },
};

/**
 * Creates a MediaStream with an audio track containing a sine tone at the
 * given frequency.
 *
 * @param {AudioContext} ac
 *        AudioContext in which to create the OscillatorNode backing the stream
 * @param {double} frequency
 *        The frequency in Hz of the generated sine tone
 * @returns {MediaStream} the MediaStream containing sine tone audio track
 */
function createOscillatorStream(ac, frequency) {
  var osc = ac.createOscillator();
  osc.frequency.value = frequency;

  var oscDest = ac.createMediaStreamDestination();
  osc.connect(oscDest);
  osc.start();
  return oscDest.stream;
}

/**
 * Create the necessary HTML elements for head and body as used by Mochitests
 *
 * @param {object} meta
 *        Meta information of the test
 * @param {string} meta.title
 *        Description of the test
 * @param {string} [meta.bug]
 *        Bug the test was created for
 * @param {boolean} [meta.visible=false]
 *        Visibility of the media elements
 */
function realCreateHTML(meta) {
  var test = document.getElementById("test");

  // Create the head content
  var elem = document.createElement("meta");
  elem.setAttribute("charset", "utf-8");
  document.head.appendChild(elem);

  var title = document.createElement("title");
  title.textContent = meta.title;
  document.head.appendChild(title);

  // Create the body content
  var anchor = document.createElement("a");
  anchor.textContent = meta.title;
  if (meta.bug) {
    anchor.setAttribute(
      "href",
      "https://bugzilla.mozilla.org/show_bug.cgi?id=" + meta.bug
    );
  } else {
    anchor.setAttribute("target", "_blank");
  }

  document.body.insertBefore(anchor, test);

  var display = document.createElement("p");
  display.setAttribute("id", "display");
  document.body.insertBefore(display, test);

  var content = document.createElement("div");
  content.setAttribute("id", "content");
  content.style.display = meta.visible ? "block" : "none";
  document.body.appendChild(content);
}

/**
 * Creates an element of the given type, assigns the given id, sets the controls
 * and autoplay attributes and adds it to the content node.
 *
 * @param {string} type
 *        Defining if we should create an "audio" or "video" element
 * @param {string} id
 *        A string to use as the element id.
 */
function createMediaElement(type, id) {
  const element = document.createElement(type);
  element.setAttribute("id", id);
  element.setAttribute("height", 100);
  element.setAttribute("width", 150);
  element.setAttribute("controls", "controls");
  element.setAttribute("autoplay", "autoplay");
  element.setAttribute("muted", "muted");
  element.muted = true;
  document.getElementById("content").appendChild(element);

  return element;
}

/**
 * Returns an existing element for the given track with the given idPrefix,
 * as it was added by createMediaElementForTrack().
 *
 * @param {MediaStreamTrack} track
 *        Track used as the element's source.
 * @param {string} idPrefix
 *        A string to use as the element id. The track id will also be appended.
 */
function getMediaElementForTrack(track, idPrefix) {
  return document.getElementById(idPrefix + "_" + track.id);
}

/**
 * Create a media element with a track as source and attach it to the content
 * node.
 *
 * @param {MediaStreamTrack} track
 *        Track for use as source.
 * @param {string} idPrefix
 *        A string to use as the element id. The track id will also be appended.
 * @return {HTMLMediaElement} The created HTML media element
 */
function createMediaElementForTrack(track, idPrefix) {
  const id = idPrefix + "_" + track.id;
  const element = createMediaElement(track.kind, id);
  element.srcObject = new MediaStream([track]);

  return element;
}

/**
 * Wrapper function for mediaDevices.getUserMedia used by some tests. Whether
 * to use fake devices or not is now determined in pref further below instead.
 *
 * @param {Dictionary} constraints
 *        The constraints for this mozGetUserMedia callback
 */
function getUserMedia(constraints) {
  if (!constraints.fake && constraints.audio) {
    // Disable input processing mode when it's not explicity enabled.
    // This is to avoid distortion of the loopback tone
    constraints.audio = Object.assign(
      {},
      { autoGainControl: false },
      { echoCancellation: false },
      { noiseSuppression: false },
      constraints.audio
    );
  }
  info("Call getUserMedia for " + JSON.stringify(constraints));
  return navigator.mediaDevices.getUserMedia(constraints).then(stream => {
    checkMediaStreamTracks(constraints, stream);
    return stream;
  });
}

// These are the promises we use to track that the prerequisites for the test
// are in place before running it.
var setTestOptions;
var testConfigured = new Promise(r => (setTestOptions = r));

function pushPrefs(...p) {
  return SpecialPowers.pushPrefEnv({ set: p });
}

async function withPrefs(prefs, func) {
  await SpecialPowers.pushPrefEnv({ set: prefs });
  try {
    return await func();
  } finally {
    await SpecialPowers.popPrefEnv();
  }
}

function setupEnvironment() {
  var defaultMochitestPrefs = {
    set: [
      ["media.peerconnection.enabled", true],
      ["media.peerconnection.identity.timeout", 120000],
      ["media.peerconnection.ice.stun_client_maximum_transmits", 14],
      ["media.peerconnection.ice.trickle_grace_period", 30000],
      ["media.navigator.permission.disabled", true],
      // If either fake audio or video is desired we enable fake streams.
      // If loopback devices are set they will be chosen instead of fakes in gecko.
      ["media.navigator.streams.fake", WANT_FAKE_AUDIO || WANT_FAKE_VIDEO],
      ["media.getusermedia.audiocapture.enabled", true],
      ["media.getusermedia.screensharing.enabled", true],
      ["media.getusermedia.window.focus_source.enabled", false],
      ["media.recorder.audio_node.enabled", true],
      ["media.peerconnection.ice.obfuscate_host_addresses", false],
      ["media.peerconnection.nat_simulator.filtering_type", ""],
      ["media.peerconnection.nat_simulator.mapping_type", ""],
      ["media.peerconnection.nat_simulator.block_tcp", false],
      ["media.peerconnection.nat_simulator.block_udp", false],
      ["media.peerconnection.nat_simulator.redirect_address", ""],
      ["media.peerconnection.nat_simulator.redirect_targets", ""],
    ],
  };

  if (navigator.userAgent.includes("Android")) {
    defaultMochitestPrefs.set.push(
      ["media.navigator.video.default_width", 320],
      ["media.navigator.video.default_height", 240],
      ["media.navigator.video.max_fr", 10],
      ["media.autoplay.default", Ci.nsIAutoplay.ALLOWED]
    );
  }

  // Platform codec prefs should be matched because fake H.264 GMP codec doesn't
  // produce/consume real bitstreams. [TODO] remove after bug 1509012 is fixed.
  const platformEncoderEnabled = SpecialPowers.getBoolPref(
    "media.webrtc.platformencoder"
  );
  defaultMochitestPrefs.set.push([
    "media.navigator.mediadatadecoder_h264_enabled",
    platformEncoderEnabled,
  ]);

  // Running as a Mochitest.
  SimpleTest.requestFlakyTimeout("WebRTC inherently depends on timeouts");
  window.finish = () => SimpleTest.finish();
  SpecialPowers.pushPrefEnv(defaultMochitestPrefs, setTestOptions);

  // We don't care about waiting for this to complete, we just want to ensure
  // that we don't build up a huge backlog of GC work.
  SpecialPowers.exactGC();
}

// [TODO] remove after bug 1509012 is fixed.
async function matchPlatformH264CodecPrefs() {
  const hasHW264 =
    SpecialPowers.getBoolPref("media.webrtc.platformencoder") &&
    !SpecialPowers.getBoolPref("media.webrtc.platformencoder.sw_only") &&
    (navigator.userAgent.includes("Android") ||
      navigator.userAgent.includes("Mac OS X"));

  await pushPrefs(
    ["media.webrtc.platformencoder", hasHW264],
    ["media.navigator.mediadatadecoder_h264_enabled", hasHW264]
  );
}

async function runTestWhenReady(testFunc) {
  setupEnvironment();
  const options = await testConfigured;
  try {
    await testFunc(options);
  } catch (e) {
    ok(
      false,
      `Error executing test: ${e}
${e.stack ? e.stack : ""}`
    );
  } finally {
    SimpleTest.finish();
  }
}

/**
 * Checks that the media stream tracks have the expected amount of tracks
 * with the correct attributes based on the type and constraints given.
 *
 * @param {Object} constraints specifies whether the stream should have
 *                             audio, video, or both
 * @param {String} type the type of media stream tracks being checked
 * @param {sequence<MediaStreamTrack>} mediaStreamTracks the media stream
 *                                     tracks being checked
 */
function checkMediaStreamTracksByType(constraints, type, mediaStreamTracks) {
  if (constraints[type]) {
    is(mediaStreamTracks.length, 1, "One " + type + " track shall be present");

    if (mediaStreamTracks.length) {
      is(mediaStreamTracks[0].kind, type, "Track kind should be " + type);
      ok(mediaStreamTracks[0].id, "Track id should be defined");
      ok(!mediaStreamTracks[0].muted, "Track should not be muted");
    }
  } else {
    is(mediaStreamTracks.length, 0, "No " + type + " tracks shall be present");
  }
}

/**
 * Check that the given media stream contains the expected media stream
 * tracks given the associated audio & video constraints provided.
 *
 * @param {Object} constraints specifies whether the stream should have
 *                             audio, video, or both
 * @param {MediaStream} mediaStream the media stream being checked
 */
function checkMediaStreamTracks(constraints, mediaStream) {
  checkMediaStreamTracksByType(
    constraints,
    "audio",
    mediaStream.getAudioTracks()
  );
  checkMediaStreamTracksByType(
    constraints,
    "video",
    mediaStream.getVideoTracks()
  );
}

/**
 * Check that a media stream contains exactly a set of media stream tracks.
 *
 * @param {MediaStream} mediaStream the media stream being checked
 * @param {Array} tracks the tracks that should exist in mediaStream
 * @param {String} [message] an optional message to pass to asserts
 */
function checkMediaStreamContains(mediaStream, tracks, message) {
  message = message ? message + ": " : "";
  tracks.forEach(t =>
    ok(
      mediaStream.getTrackById(t.id),
      message + "MediaStream " + mediaStream.id + " contains track " + t.id
    )
  );
  is(
    mediaStream.getTracks().length,
    tracks.length,
    message + "MediaStream " + mediaStream.id + " contains no extra tracks"
  );
}

function checkMediaStreamCloneAgainstOriginal(clone, original) {
  isnot(clone.id.length, 0, "Stream clone should have an id string");
  isnot(clone, original, "Stream clone should be different from the original");
  isnot(
    clone.id,
    original.id,
    "Stream clone's id should be different from the original's"
  );
  is(
    clone.getAudioTracks().length,
    original.getAudioTracks().length,
    "All audio tracks should get cloned"
  );
  is(
    clone.getVideoTracks().length,
    original.getVideoTracks().length,
    "All video tracks should get cloned"
  );
  is(clone.active, original.active, "Active state should be preserved");
  original
    .getTracks()
    .forEach(t =>
      ok(!clone.getTrackById(t.id), "The clone's tracks should be originals")
    );
}

function checkMediaStreamTrackCloneAgainstOriginal(clone, original) {
  isnot(clone.id.length, 0, "Track clone should have an id string");
  isnot(clone, original, "Track clone should be different from the original");
  isnot(
    clone.id,
    original.id,
    "Track clone's id should be different from the original's"
  );
  is(
    clone.kind,
    original.kind,
    "Track clone's kind should be same as the original's"
  );
  is(
    clone.enabled,
    original.enabled,
    "Track clone's kind should be same as the original's"
  );
  is(
    clone.readyState,
    original.readyState,
    "Track clone's readyState should be same as the original's"
  );
  is(
    clone.muted,
    original.muted,
    "Track clone's muted state should be same as the original's"
  );
}

/*** Utility methods */

/** The dreadful setTimeout, use sparingly */
function wait(time, message) {
  return new Promise(r => setTimeout(() => r(message), time));
}

/** The even more dreadful setInterval, use even more sparingly */
function waitUntil(func, time) {
  return new Promise(resolve => {
    var interval = setInterval(() => {
      if (func()) {
        clearInterval(interval);
        resolve();
      }
    }, time || 200);
  });
}

/** Time out while waiting for a promise to get resolved or rejected. */
var timeout = (promise, time, msg) =>
  Promise.race([
    promise,
    wait(time).then(() => Promise.reject(new Error(msg))),
  ]);

/** Adds a |finally| function to a promise whose argument is invoked whether the
 * promise is resolved or rejected, and that does not interfere with chaining.*/
var addFinallyToPromise = promise => {
  promise.finally = func => {
    return promise.then(
      result => {
        func();
        return Promise.resolve(result);
      },
      error => {
        func();
        return Promise.reject(error);
      }
    );
  };
  return promise;
};

/** Use event listener to call passed-in function on fire until it returns true */
var listenUntil = (target, eventName, onFire) => {
  return new Promise(resolve =>
    target.addEventListener(eventName, function callback(event) {
      var result = onFire(event);
      if (result) {
        target.removeEventListener(eventName, callback);
        resolve(result);
      }
    })
  );
};

/* Test that a function throws the right error */
function mustThrowWith(msg, reason, f) {
  try {
    f();
    ok(false, msg + " must throw");
  } catch (e) {
    is(e.name, reason, msg + " must throw: " + e.message);
  }
}

/* Get a dummy audio track */
function getSilentTrack() {
  let ctx = new AudioContext(),
    oscillator = ctx.createOscillator();
  let dst = oscillator.connect(ctx.createMediaStreamDestination());
  oscillator.start();
  return Object.assign(dst.stream.getAudioTracks()[0], { enabled: false });
}

function getBlackTrack({ width = 640, height = 480 } = {}) {
  let canvas = Object.assign(document.createElement("canvas"), {
    width,
    height,
  });
  canvas.getContext("2d").fillRect(0, 0, width, height);
  let stream = canvas.captureStream();
  return Object.assign(stream.getVideoTracks()[0], { enabled: false });
}

/*** Test control flow methods */

/**
 * Generates a callback function fired only under unexpected circumstances
 * while running the tests. The generated function kills off the test as well
 * gracefully.
 *
 * @param {String} [message]
 *        An optional message to show if no object gets passed into the
 *        generated callback method.
 */
function generateErrorCallback(message) {
  var stack = new Error().stack.split("\n");
  stack.shift(); // Don't include this instantiation frame

  /**
   * @param {object} aObj
   *        The object fired back from the callback
   */
  return aObj => {
    if (aObj) {
      if (aObj.name && aObj.message) {
        ok(
          false,
          "Unexpected callback for '" +
            aObj.name +
            "' with message = '" +
            aObj.message +
            "' at " +
            JSON.stringify(stack)
        );
      } else {
        ok(
          false,
          "Unexpected callback with = '" +
            aObj +
            "' at: " +
            JSON.stringify(stack)
        );
      }
    } else {
      ok(
        false,
        "Unexpected callback with message = '" +
          message +
          "' at: " +
          JSON.stringify(stack)
      );
    }
    throw new Error("Unexpected callback");
  };
}

var unexpectedEventArrived;
var rejectOnUnexpectedEvent = new Promise((x, reject) => {
  unexpectedEventArrived = reject;
});

/**
 * Generates a callback function fired only for unexpected events happening.
 *
 * @param {String} description
          Description of the object for which the event has been fired
 * @param {String} eventName
          Name of the unexpected event
 */
function unexpectedEvent(message, eventName) {
  var stack = new Error().stack.split("\n");
  stack.shift(); // Don't include this instantiation frame

  return e => {
    var details =
      "Unexpected event '" +
      eventName +
      "' fired with message = '" +
      message +
      "' at: " +
      JSON.stringify(stack);
    ok(false, details);
    unexpectedEventArrived(new Error(details));
  };
}

/**
 * Implements the one-shot event pattern used throughout.  Each of the 'onxxx'
 * attributes on the wrappers can be set with a custom handler.  Prior to the
 * handler being set, if the event fires, it causes the test execution to halt.
 * That handler is used exactly once, after which the original, error-generating
 * handler is re-installed.  Thus, each event handler is used at most once.
 *
 * @param {object} wrapper
 *        The wrapper on which the psuedo-handler is installed
 * @param {object} obj
 *        The real source of events
 * @param {string} event
 *        The name of the event
 */
function createOneShotEventWrapper(wrapper, obj, event) {
  var onx = "on" + event;
  var unexpected = unexpectedEvent(wrapper, event);
  wrapper[onx] = unexpected;
  obj[onx] = e => {
    info(wrapper + ': "on' + event + '" event fired');
    e.wrapper = wrapper;
    wrapper[onx](e);
    wrapper[onx] = unexpected;
  };
}

/**
 * Returns a promise that resolves when `target` has raised an event with the
 * given name the given number of times. Cancel the returned promise by passing
 * in a `cancel` promise and resolving it.
 *
 * @param {object} target
 *        The target on which the event should occur.
 * @param {string} name
 *        The name of the event that should occur.
 * @param {integer} count
 *        Optional number of times the event should be raised before resolving.
 * @param {promise} cancel
 *        Optional promise that on resolving rejects the returned promise,
 *        so we can avoid logging results after a test has finished.
 * @returns {promise} A promise that resolves to the last of the seen events.
 */
function haveEvents(target, name, count, cancel) {
  var listener;
  var counter = count || 1;
  return Promise.race([
    (cancel || new Promise(() => {})).then(e => Promise.reject(e)),
    new Promise(resolve =>
      target.addEventListener(
        name,
        (listener = e => --counter < 1 && resolve(e))
      )
    ),
  ]).then(e => {
    target.removeEventListener(name, listener);
    return e;
  });
}

/**
 * Returns a promise that resolves when `target` has raised an event with the
 * given name. Cancel the returned promise by passing in a `cancel` promise and
 * resolving it.
 *
 * @param {object} target
 *        The target on which the event should occur.
 * @param {string} name
 *        The name of the event that should occur.
 * @param {promise} cancel
 *        Optional promise that on resolving rejects the returned promise,
 *        so we can avoid logging results after a test has finished.
 * @returns {promise} A promise that resolves to the seen event.
 */
function haveEvent(target, name, cancel) {
  return haveEvents(target, name, 1, cancel);
}

/**
 * Returns a promise that resolves if the target has not seen the given event
 * after one crank (or until the given timeoutPromise resolves) of the event
 * loop.
 *
 * @param {object} target
 *        The target on which the event should not occur.
 * @param {string} name
 *        The name of the event that should not occur.
 * @param {promise} timeoutPromise
 *        Optional promise defining how long we should wait before resolving.
 * @returns {promise} A promise that is rejected if we see the given event, or
 *                    resolves after a timeout otherwise.
 */
function haveNoEvent(target, name, timeoutPromise) {
  return haveEvent(target, name, timeoutPromise || wait(0)).then(
    () => Promise.reject(new Error("Too many " + name + " events")),
    () => {}
  );
}

/**
 * Returns a promise that resolves after the target has seen the given number
 * of events but no such event in a following crank of the event loop.
 *
 * @param {object} target
 *        The target on which the events should occur.
 * @param {string} name
 *        The name of the event that should occur.
 * @param {integer} count
 *        Optional number of times the event should be raised before resolving.
 * @param {promise} cancel
 *        Optional promise that on resolving rejects the returned promise,
 *        so we can avoid logging results after a test has finished.
 * @returns {promise} A promise that resolves to the last of the seen events.
 */
function haveEventsButNoMore(target, name, count, cancel) {
  return haveEvents(target, name, count, cancel).then(e =>
    haveNoEvent(target, name).then(() => e)
  );
}

/*
 * Resolves the returned promise with an object with usage and reportCount
 * properties.  `usage` is in the same units as reported by the reporter for
 * `path`.
 */
const collectMemoryUsage = async path => {
  const MemoryReporterManager = Cc[
    "@mozilla.org/memory-reporter-manager;1"
  ].getService(Ci.nsIMemoryReporterManager);

  let usage = 0;
  let reportCount = 0;
  await new Promise(resolve =>
    MemoryReporterManager.getReports(
      (aProcess, aPath, aKind, aUnits, aAmount, aDesc) => {
        if (aPath != path) {
          return;
        }
        ++reportCount;
        usage += aAmount;
      },
      null,
      resolve,
      null,
      /* anonymized = */ false
    )
  );
  return { usage, reportCount };
};

// Some DNS helper functions
const dnsLookup = async hostname => {
  // Convenience API for various networking related stuff. _Almost_ convenient
  // enough.
  const neckoDashboard = SpecialPowers.Cc[
    "@mozilla.org/network/dashboard;1"
  ].getService(Ci.nsIDashboard);

  const results = await new Promise(r => {
    neckoDashboard.requestDNSLookup(hostname, results => {
      r(SpecialPowers.wrap(results));
    });
  });

  // |address| is an array-like dictionary (ie; keys are all integers).
  // We convert to an array to make it less unwieldy.
  const addresses = [...results.address];
  info(`DNS results for ${hostname}: ${JSON.stringify(addresses)}`);
  return addresses;
};

const dnsLookupV4 = async hostname => {
  const addresses = await dnsLookup(hostname);
  return addresses.filter(address => !address.includes(":"));
};

const dnsLookupV6 = async hostname => {
  const addresses = await dnsLookup(hostname);
  return addresses.filter(address => address.includes(":"));
};

const getTurnHostname = turnUrl => {
  const urlNoParams = turnUrl.split("?")[0];
  // Strip off scheme
  const hostAndMaybePort = urlNoParams.split(":", 2)[1];
  if (hostAndMaybePort[0] == "[") {
    // IPV6 literal, strip out '[', and split at closing ']'
    return hostAndMaybePort.substring(1).split("]")[0];
  }
  return hostAndMaybePort.split(":")[0];
};

// Yo dawg I heard you like yo dawg I heard you like Proxies
// Example: let value = await GleanTest.category.metric.testGetValue();
// For labeled metrics:
//    let value = await GleanTest.category.metric["label"].testGetValue();
// Please don't try to use the string "testGetValue" as a label.
const GleanTest = new Proxy(
  {},
  {
    get(target, categoryName, receiver) {
      return new Proxy(
        {},
        {
          get(target, metricName, receiver) {
            return new Proxy(
              {
                async testGetValue() {
                  return SpecialPowers.spawnChrome(
                    [categoryName, metricName],
                    async (categoryName, metricName) => {
                      await Services.fog.testFlushAllChildren();
                      const window = this.browsingContext.topChromeWindow;
                      return window.Glean[categoryName][
                        metricName
                      ].testGetValue();
                    }
                  );
                },
              },
              {
                get(target, prop, receiver) {
                  // The only prop that will be there is testGetValue, but we
                  // might add more later.
                  if (prop in target) {
                    return target[prop];
                  }

                  // |prop| must be a label?
                  const label = prop;
                  return {
                    async testGetValue() {
                      return SpecialPowers.spawnChrome(
                        [categoryName, metricName, label],
                        async (categoryName, metricName, label) => {
                          await Services.fog.testFlushAllChildren();
                          const window = this.browsingContext.topChromeWindow;
                          return window.Glean[categoryName][metricName][
                            label
                          ].testGetValue();
                        }
                      );
                    },
                  };
                },
              }
            );
          },
        }
      );
    },
  }
);

/**
 * This class executes a series of functions in a continuous sequence.
 * Promise-bearing functions are executed after the previous promise completes.
 *
 * @constructor
 * @param {object} framework
 *        A back reference to the framework which makes use of the class. It is
 *        passed to each command callback.
 * @param {function[]} commandList
 *        Commands to set during initialization
 */
function CommandChain(framework, commandList) {
  this._framework = framework;
  this.commands = commandList || [];
}

CommandChain.prototype = {
  /**
   * Start the command chain.  This returns a promise that always resolves
   * cleanly (this catches errors and fails the test case).
   */
  execute() {
    return this.commands
      .reduce((prev, next, i) => {
        if (typeof next !== "function" || !next.name) {
          throw new Error("registered non-function" + next);
        }

        return prev.then(() => {
          info("Run step " + (i + 1) + ": " + next.name);
          return Promise.race([next(this._framework), rejectOnUnexpectedEvent]);
        });
      }, Promise.resolve())
      .catch(e =>
        ok(
          false,
          "Error in test execution: " +
            e +
            (typeof e.stack === "string"
              ? " " + e.stack.split("\n").join(" ... ")
              : "")
        )
      );
  },

  /**
   * Add new commands to the end of the chain
   */
  append(commands) {
    this.commands = this.commands.concat(commands);
  },

  /**
   * Returns the index of the specified command in the chain.
   * @param {occurrence} Optional param specifying which occurrence to match,
   * with 0 representing the first occurrence.
   */
  indexOf(functionOrName, occurrence) {
    occurrence = occurrence || 0;
    return this.commands.findIndex(func => {
      if (typeof functionOrName === "string") {
        if (func.name !== functionOrName) {
          return false;
        }
      } else if (func !== functionOrName) {
        return false;
      }
      if (occurrence) {
        --occurrence;
        return false;
      }
      return true;
    });
  },

  mustHaveIndexOf(functionOrName, occurrence) {
    var index = this.indexOf(functionOrName, occurrence);
    if (index == -1) {
      throw new Error("Unknown test: " + functionOrName);
    }
    return index;
  },

  /**
   * Inserts the new commands after the specified command.
   */
  insertAfter(functionOrName, commands, all, occurrence) {
    this._insertHelper(functionOrName, commands, 1, all, occurrence);
  },

  /**
   * Inserts the new commands after every occurrence of the specified command
   */
  insertAfterEach(functionOrName, commands) {
    this._insertHelper(functionOrName, commands, 1, true);
  },

  /**
   * Inserts the new commands before the specified command.
   */
  insertBefore(functionOrName, commands, all, occurrence) {
    this._insertHelper(functionOrName, commands, 0, all, occurrence);
  },

  _insertHelper(functionOrName, commands, delta, all, occurrence) {
    occurrence = occurrence || 0;
    for (
      var index = this.mustHaveIndexOf(functionOrName, occurrence);
      index !== -1;
      index = this.indexOf(functionOrName, ++occurrence)
    ) {
      this.commands = [].concat(
        this.commands.slice(0, index + delta),
        commands,
        this.commands.slice(index + delta)
      );
      if (!all) {
        break;
      }
    }
  },

  /**
   * Removes the specified command, returns what was removed.
   */
  remove(functionOrName, occurrence) {
    return this.commands.splice(
      this.mustHaveIndexOf(functionOrName, occurrence),
      1
    );
  },

  /**
   * Removes all commands after the specified one, returns what was removed.
   */
  removeAfter(functionOrName, occurrence) {
    return this.commands.splice(
      this.mustHaveIndexOf(functionOrName, occurrence) + 1
    );
  },

  /**
   * Removes all commands before the specified one, returns what was removed.
   */
  removeBefore(functionOrName, occurrence) {
    return this.commands.splice(
      0,
      this.mustHaveIndexOf(functionOrName, occurrence)
    );
  },

  /**
   * Replaces a single command, returns what was removed.
   */
  replace(functionOrName, commands) {
    this.insertBefore(functionOrName, commands);
    return this.remove(functionOrName);
  },

  /**
   * Replaces all commands after the specified one, returns what was removed.
   */
  replaceAfter(functionOrName, commands, occurrence) {
    var oldCommands = this.removeAfter(functionOrName, occurrence);
    this.append(commands);
    return oldCommands;
  },

  /**
   * Replaces all commands before the specified one, returns what was removed.
   */
  replaceBefore(functionOrName, commands) {
    var oldCommands = this.removeBefore(functionOrName);
    this.insertBefore(functionOrName, commands);
    return oldCommands;
  },

  /**
   * Remove all commands whose name match the specified regex.
   */
  filterOut(id_match) {
    this.commands = this.commands.filter(c => !id_match.test(c.name));
  },
};

function AudioStreamFlowingHelper() {
  this._context = new AudioContext();
  // Tests may have changed the values of prefs, so recheck
  updateConfigFromFakeAndLoopbackPrefs();
  if (!WANT_FAKE_AUDIO) {
    // Loopback device is configured, start the default loopback tone
    if (!DefaultLoopbackTone) {
      DefaultLoopbackTone = new LoopbackTone(this._context, TEST_AUDIO_FREQ);
      DefaultLoopbackTone.start();
    }
  }
}

AudioStreamFlowingHelper.prototype = {
  checkAudio(stream, analyser, fun) {
    /*
    analyser.enableDebugCanvas();
    return analyser.waitForAnalysisSuccess(fun)
      .then(() => analyser.disableDebugCanvas());
    */
    return analyser.waitForAnalysisSuccess(fun);
  },

  checkAudioFlowing(stream) {
    var analyser = new AudioStreamAnalyser(this._context, stream);
    var freq = analyser.binIndexForFrequency(TEST_AUDIO_FREQ);
    return this.checkAudio(stream, analyser, array => array[freq] > 200);
  },

  // Use checkAudioNotFlowing() only after checkAudioFlowing() or similar to
  // know that audio had previously been flowing on the same stream, as
  // checkAudioNotFlowing() does not wait for the loopback device to return
  // any audio that it receives.
  checkAudioNotFlowing(stream) {
    var analyser = new AudioStreamAnalyser(this._context, stream);
    var freq = analyser.binIndexForFrequency(TEST_AUDIO_FREQ);
    return this.checkAudio(stream, analyser, array => array[freq] < 50);
  },
};

class VideoFrameEmitter {
  constructor(color1, color2, width, height) {
    if (!width) {
      width = 50;
    }
    if (!height) {
      height = width;
    }
    this._helper = new CaptureStreamTestHelper2D(width, height);
    this._canvas = this._helper.createAndAppendElement(
      "canvas",
      "source_canvas"
    );
    this._canvas.width = width;
    this._canvas.height = height;
    this._color1 = color1 ? color1 : this._helper.green;
    this._color2 = color2 ? color2 : this._helper.red;
    // Make sure this is initted
    this._helper.drawColor(this._canvas, this._color1);
    this._stream = this._canvas.captureStream();
    this._started = false;
  }

  stream() {
    return this._stream;
  }

  helper() {
    return this._helper;
  }

  colors(color1, color2) {
    this._color1 = color1 ? color1 : this._helper.green;
    this._color2 = color2 ? color2 : this._helper.red;
    try {
      this._helper.drawColor(this._canvas, this._color1);
    } catch (e) {
      // ignore; stream might have shut down
    }
  }

  size(width, height) {
    this._canvas.width = width;
    this._canvas.height = height;
  }

  start() {
    if (this._started) {
      info("*** emitter already started");
      return;
    }

    let i = 0;
    this._started = true;
    this._intervalId = setInterval(() => {
      try {
        this._helper.drawColor(this._canvas, i ? this._color1 : this._color2);
        i = 1 - i;
      } catch (e) {
        // ignore; stream might have shut down, and we don't bother clearing
        // the setInterval.
      }
    }, 500);
  }

  stop() {
    if (this._started) {
      clearInterval(this._intervalId);
      this._started = false;
    }
  }
}

class VideoStreamHelper {
  constructor() {
    this._helper = new CaptureStreamTestHelper2D(50, 50);
  }

  async checkHasFrame(video, { offsetX, offsetY, threshold } = {}) {
    const h = this._helper;
    await h.waitForPixel(
      video,
      px => {
        let result = h.isOpaquePixelNot(px, h.black, threshold);
        info(
          "Checking that we have a frame, got [" +
            Array.from(px) +
            "]. Ref=[" +
            Array.from(h.black.data) +
            "]. Threshold=" +
            threshold +
            ". Pass=" +
            result
        );
        return result;
      },
      { offsetX, offsetY }
    );
  }

  async checkVideoPlaying(
    video,
    { offsetX = 10, offsetY = 10, threshold = 16 } = {}
  ) {
    const h = this._helper;
    await this.checkHasFrame(video, { offsetX, offsetY, threshold });
    let startPixel = {
      data: h.getPixel(video, offsetX, offsetY),
      name: "startcolor",
    };
    await h.waitForPixel(
      video,
      px => {
        let result = h.isPixelNot(px, startPixel, threshold);
        info(
          "Checking playing, [" +
            Array.from(px) +
            "] vs [" +
            Array.from(startPixel.data) +
            "]. Threshold=" +
            threshold +
            " Pass=" +
            result
        );
        return result;
      },
      { offsetX, offsetY }
    );
  }

  async checkVideoPaused(
    video,
    { offsetX = 10, offsetY = 10, threshold = 16, time = 5000 } = {}
  ) {
    const h = this._helper;
    await this.checkHasFrame(video, { offsetX, offsetY, threshold });
    let startPixel = {
      data: h.getPixel(video, offsetX, offsetY),
      name: "startcolor",
    };
    try {
      await h.waitForPixel(
        video,
        px => {
          let result = h.isOpaquePixelNot(px, startPixel, threshold);
          info(
            "Checking paused, [" +
              Array.from(px) +
              "] vs [" +
              Array.from(startPixel.data) +
              "]. Threshold=" +
              threshold +
              " Pass=" +
              result
          );
          return result;
        },
        { offsetX, offsetY, cancel: wait(time, "timeout") }
      );
      ok(false, "Frame changed within " + time / 1000 + " seconds");
    } catch (e) {
      is(
        e,
        "timeout",
        "Frame shouldn't change for " + time / 1000 + " seconds"
      );
    }
  }
}

(function () {
  var el = document.createElement("link");
  el.rel = "stylesheet";
  el.type = "text/css";
  el.href = "/tests/SimpleTest/test.css";
  document.head.appendChild(el);
})();
