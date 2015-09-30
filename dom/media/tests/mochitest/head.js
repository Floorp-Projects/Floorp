/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Cc = SpecialPowers.Cc;
var Ci = SpecialPowers.Ci;

// Specifies whether we are using fake streams to run this automation
var FAKE_ENABLED = true;
try {
  var audioDevice = SpecialPowers.getCharPref('media.audio_loopback_dev');
  var videoDevice = SpecialPowers.getCharPref('media.video_loopback_dev');
  dump('TEST DEVICES: Using media devices:\n');
  dump('audio: ' + audioDevice + '\nvideo: ' + videoDevice + '\n');
  FAKE_ENABLED = false;
} catch (e) {
  dump('TEST DEVICES: No test devices found (in media.{audio,video}_loopback_dev, using fake streams.\n');
  FAKE_ENABLED = true;
}

/**
 * This class provides helpers around analysing the audio content in a stream
 * using WebAudio AnalyserNodes.
 *
 * @constructor
 * @param {object} stream
 *                 A MediaStream object whose audio track we shall analyse.
 */
function AudioStreamAnalyser(ac, stream) {
  if (stream.getAudioTracks().length === 0) {
    throw new Error("No audio track in stream");
  }
  this.audioContext = ac;
  this.stream = stream;
  this.sourceNode = this.audioContext.createMediaStreamSource(this.stream);
  this.analyser = this.audioContext.createAnalyser();
  this.sourceNode.connect(this.analyser);
  this.data = new Uint8Array(this.analyser.frequencyBinCount);
}

AudioStreamAnalyser.prototype = {
  /**
   * Get an array of frequency domain data for our stream's audio track.
   *
   * @returns {array} A Uint8Array containing the frequency domain data.
   */
  getByteFrequencyData: function() {
    this.analyser.getByteFrequencyData(this.data);
    return this.data;
  },

  /**
   * Append a canvas to the DOM where the frequency data are drawn.
   * Useful to debug tests.
   */
  enableDebugCanvas: function() {
    var cvs = document.createElement("canvas");
    document.getElementById("content").appendChild(cvs);

    // Easy: 1px per bin
    cvs.width = this.analyser.frequencyBinCount;
    cvs.height = 256;
    cvs.style.border = "1px solid red";

    var c = cvs.getContext('2d');

    var self = this;
    function render() {
      c.clearRect(0, 0, cvs.width, cvs.height);
      var array = self.getByteFrequencyData();
      for (var i = 0; i < array.length; i++) {
        c.fillRect(i, (256 - (array[i])), 1, 256);
      }
      requestAnimationFrame(render);
    }
    requestAnimationFrame(render);
  },

  /**
   * Return a Promise, that will be resolved when the function passed as
   * argument, when called, returns true (meaning the analysis was a
   * success).
   *
   * @param {function} analysisFunction
   *        A fonction that performs an analysis, and returns true if the
   *        analysis was a success (i.e. it found what it was looking for)
   */
  waitForAnalysisSuccess: function(analysisFunction) {
    var self = this;
    return new Promise((resolve, reject) => {
      function analysisLoop() {
        var success = analysisFunction(self.getByteFrequencyData());
        if (success) {
          resolve();
          return;
        }
        // else, we need more time
        requestAnimationFrame(analysisLoop);
      }
      analysisLoop();
    });
  },

  /**
   * Return the FFT bin index for a given frequency.
   *
   * @param {double} frequency
   *        The frequency for whicht to return the bin number.
   * @returns {integer} the index of the bin in the FFT array.
   */
  binIndexForFrequency: function(frequency) {
    return 1 + Math.round(frequency *
                          this.analyser.fftSize /
                          this.audioContext.sampleRate);
  },

  /**
   * Reverse operation, get the frequency for a bin index.
   *
   * @param {integer} index an index in an FFT array
   * @returns {double} the frequency for this bin
   */
  frequencyForBinIndex: function(index) {
    return (index - 1) *
           this.audioContext.sampleRate /
           this.analyser.fftSize;
  }
};

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
  var test = document.getElementById('test');

  // Create the head content
  var elem = document.createElement('meta');
  elem.setAttribute('charset', 'utf-8');
  document.head.appendChild(elem);

  var title = document.createElement('title');
  title.textContent = meta.title;
  document.head.appendChild(title);

  // Create the body content
  var anchor = document.createElement('a');
  anchor.textContent = meta.title;
  if (meta.bug) {
    anchor.setAttribute('href', 'https://bugzilla.mozilla.org/show_bug.cgi?id=' + meta.bug);
  } else {
    anchor.setAttribute('target', '_blank');
  }

  document.body.insertBefore(anchor, test);

  var display = document.createElement('p');
  display.setAttribute('id', 'display');
  document.body.insertBefore(display, test);

  var content = document.createElement('div');
  content.setAttribute('id', 'content');
  content.style.display = meta.visible ? 'block' : "none";
  document.body.appendChild(content);
}


/**
 * Create the HTML element if it doesn't exist yet and attach
 * it to the content node.
 *
 * @param {string} type
 *        Type of media element to create ('audio' or 'video')
 * @param {string} label
 *        Description to use for the element
 * @return {HTMLMediaElement} The created HTML media element
 */
function createMediaElement(type, label) {
  var id = label + '_' + type;
  var element = document.getElementById(id);

  // Sanity check that we haven't created the element already
  if (element) {
    return element;
  }

  element = document.createElement(type === 'audio' ? 'audio' : 'video');
  element.setAttribute('id', id);
  element.setAttribute('height', 100);
  element.setAttribute('width', 150);
  element.setAttribute('controls', 'controls');
  element.setAttribute('autoplay', 'autoplay');
  document.getElementById('content').appendChild(element);

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
  info("Call getUserMedia for " + JSON.stringify(constraints));
  return navigator.mediaDevices.getUserMedia(constraints);
}

// These are the promises we use to track that the prerequisites for the test
// are in place before running it.
var setTestOptions;
var testConfigured = new Promise(r => setTestOptions = r);

function setupEnvironment() {
  if (!window.SimpleTest) {
    // Running under Steeplechase
    return;
  }

  // Running as a Mochitest.
  SimpleTest.requestFlakyTimeout("WebRTC inherently depends on timeouts");
  window.finish = () => SimpleTest.finish();
  SpecialPowers.pushPrefEnv({
    'set': [
      ['media.peerconnection.enabled', true],
      ['media.peerconnection.identity.enabled', true],
      ['media.peerconnection.identity.timeout', 120000],
      ['media.peerconnection.ice.stun_client_maximum_transmits', 14],
      ['media.peerconnection.ice.trickle_grace_period', 30000],
      ['media.navigator.permission.disabled', true],
      ['media.navigator.streams.fake', FAKE_ENABLED],
      ['media.getusermedia.screensharing.enabled', true],
      ['media.getusermedia.screensharing.allowed_domains', "mochi.test"],
      ['media.getusermedia.audiocapture.enabled', true],
      ['media.recorder.audio_node.enabled', true]
    ]
  }, setTestOptions);

  // We don't care about waiting for this to complete, we just want to ensure
  // that we don't build up a huge backlog of GC work.
  SpecialPowers.exactGC(window);
}

// This is called by steeplechase; which provides the test configuration options
// directly to the test through this function.  If we're not on steeplechase,
// the test is configured directly and immediately.
function run_test(is_initiator,timeout) {
  var options = { is_local: is_initiator,
                  is_remote: !is_initiator };

  setTimeout(() => {
    unexpectedEventArrived(new Error("PeerConnectionTest timed out after "+timeout+"s"));
  }, timeout);

  // Also load the steeplechase test code.
  var s = document.createElement("script");
  s.src = "/test.js";
  s.onload = () => setTestOptions(options);
  document.head.appendChild(s);
}

function runTestWhenReady(testFunc) {
  setupEnvironment();
  return testConfigured.then(options => testFunc(options))
    .catch(e => ok(false, 'Error executing test: ' + e +
        ((typeof e.stack === 'string') ?
        (' ' + e.stack.split('\n').join(' ... ')) : '')));
}


/**
 * Checks that the media stream tracks have the expected amount of tracks
 * with the correct kind and id based on the type and constraints given.
 *
 * @param {Object} constraints specifies whether the stream should have
 *                             audio, video, or both
 * @param {String} type the type of media stream tracks being checked
 * @param {sequence<MediaStreamTrack>} mediaStreamTracks the media stream
 *                                     tracks being checked
 */
function checkMediaStreamTracksByType(constraints, type, mediaStreamTracks) {
  if (constraints[type]) {
    is(mediaStreamTracks.length, 1, 'One ' + type + ' track shall be present');

    if (mediaStreamTracks.length) {
      is(mediaStreamTracks[0].kind, type, 'Track kind should be ' + type);
      ok(mediaStreamTracks[0].id, 'Track id should be defined');
    }
  } else {
    is(mediaStreamTracks.length, 0, 'No ' + type + ' tracks shall be present');
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
  checkMediaStreamTracksByType(constraints, 'audio',
    mediaStream.getAudioTracks());
  checkMediaStreamTracksByType(constraints, 'video',
    mediaStream.getVideoTracks());
}

/**
 * Check that a media stream contains exactly a set of media stream tracks.
 *
 * @param {MediaStream} mediaStream the media stream being checked
 * @param {Array} tracks the tracks that should exist in mediaStream
 * @param {String} [message] an optional message to pass to asserts
 */
function checkMediaStreamContains(mediaStream, tracks, message) {
  message = message ? (message + ": ") : "";
  tracks.forEach(t => ok(mediaStream.getTracks().includes(t),
                         message + "MediaStream " + mediaStream.id +
                         " contains track " + t.id));
  is(mediaStream.getTracks().length, tracks.length,
     message + "MediaStream " + mediaStream.id + " contains no extra tracks");
}

/*** Utility methods */

/** The dreadful setTimeout, use sparingly */
function wait(time) {
  return new Promise(r => setTimeout(r, time));
}

/** The even more dreadful setInterval, use even more sparingly */
function waitUntil(func, time) {
  return new Promise(resolve => {
    var interval = setInterval(() => {
      if (func())  {
        clearInterval(interval);
        resolve();
      }
    }, time || 200);
  });
}

/** Time out while waiting for a promise to get resolved or rejected. */
var timeout = (promise, time, msg) =>
  Promise.race([promise, wait(time).then(() => Promise.reject(new Error(msg)))]);

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
        ok(false, "Unexpected callback for '" + aObj.name +
           "' with message = '" + aObj.message + "' at " +
           JSON.stringify(stack));
      } else {
        ok(false, "Unexpected callback with = '" + aObj +
           "' at: " + JSON.stringify(stack));
      }
    } else {
      ok(false, "Unexpected callback with message = '" + message +
         "' at: " + JSON.stringify(stack));
    }
    throw new Error("Unexpected callback");
  }
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
    var details = "Unexpected event '" + eventName + "' fired with message = '" +
        message + "' at: " + JSON.stringify(stack);
    ok(false, details);
    unexpectedEventArrived(new Error(details));
  }
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
  var onx = 'on' + event;
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
  this.commands = commandList || [ ];
}

CommandChain.prototype = {
  /**
   * Start the command chain.  This returns a promise that always resolves
   * cleanly (this catches errors and fails the test case).
   */
  execute: function () {
    return this.commands.reduce((prev, next, i) => {
      if (typeof next !== 'function' || !next.name) {
        throw new Error('registered non-function' + next);
      }

      return prev.then(() => {
        info('Run step ' + (i + 1) + ': ' + next.name);
        return Promise.race([ next(this._framework), rejectOnUnexpectedEvent ]);
      });
    }, Promise.resolve())
      .catch(e =>
             ok(false, 'Error in test execution: ' + e +
                ((typeof e.stack === 'string') ?
                 (' ' + e.stack.split('\n').join(' ... ')) : '')));
  },

  /**
   * Add new commands to the end of the chain
   */
  append: function(commands) {
    this.commands = this.commands.concat(commands);
  },

  /**
   * Returns the index of the specified command in the chain.
   * @param {start} Optional param specifying the index at which the search will
   * start. If not specified, the search starts at index 0.
   */
  indexOf: function(functionOrName, start) {
    start = start || 0;
    if (typeof functionOrName === 'string') {
      var index = this.commands.slice(start).findIndex(f => f.name === functionOrName);
      if (index !== -1) {
        index += start;
      }
      return index;
    }
    return this.commands.indexOf(functionOrName, start);
  },

  /**
   * Inserts the new commands after the specified command.
   */
  insertAfter: function(functionOrName, commands, all, start) {
    this._insertHelper(functionOrName, commands, 1, all, start);
  },

  /**
   * Inserts the new commands after every occurrence of the specified command
   */
  insertAfterEach: function(functionOrName, commands) {
    this._insertHelper(functionOrName, commands, 1, true);
  },

  /**
   * Inserts the new commands before the specified command.
   */
  insertBefore: function(functionOrName, commands, all, start) {
    this._insertHelper(functionOrName, commands, 0, all, start);
  },

  _insertHelper: function(functionOrName, commands, delta, all, start) {
    var index = this.indexOf(functionOrName);
    start = start || 0;
    for (; index !== -1; index = this.indexOf(functionOrName, index)) {
      if (!start) {
        this.commands = [].concat(
          this.commands.slice(0, index + delta),
          commands,
          this.commands.slice(index + delta));
        if (!all) {
          break;
        }
      } else {
        start -= 1;
      }
      index += (commands.length + 1);
    }
  },

  /**
   * Removes the specified command, returns what was removed.
   */
  remove: function(functionOrName) {
    var index = this.indexOf(functionOrName);
    if (index >= 0) {
      return this.commands.splice(index, 1);
    }
    return [];
  },

  /**
   * Removes all commands after the specified one, returns what was removed.
   */
  removeAfter: function(functionOrName, start) {
    var index = this.indexOf(functionOrName, start);
    if (index >= 0) {
      return this.commands.splice(index + 1);
    }
    return [];
  },

  /**
   * Removes all commands before the specified one, returns what was removed.
   */
  removeBefore: function(functionOrName) {
    var index = this.indexOf(functionOrName);
    if (index >= 0) {
      return this.commands.splice(0, index);
    }
    return [];
  },

  /**
   * Replaces a single command, returns what was removed.
   */
  replace: function(functionOrName, commands) {
    this.insertBefore(functionOrName, commands);
    return this.remove(functionOrName);
  },

  /**
   * Replaces all commands after the specified one, returns what was removed.
   */
  replaceAfter: function(functionOrName, commands, start) {
    var oldCommands = this.removeAfter(functionOrName, start);
    this.append(commands);
    return oldCommands;
  },

  /**
   * Replaces all commands before the specified one, returns what was removed.
   */
  replaceBefore: function(functionOrName, commands) {
    var oldCommands = this.removeBefore(functionOrName);
    this.insertBefore(functionOrName, commands);
    return oldCommands;
  },

  /**
   * Remove all commands whose name match the specified regex.
   */
  filterOut: function (id_match) {
    this.commands = this.commands.filter(c => !id_match.test(c.name));
  },
};


function IsMacOSX10_6orOlder() {
  if (navigator.platform.indexOf("Mac") !== 0) {
    return false;
  }

  var version = Cc["@mozilla.org/system-info;1"]
      .getService(Ci.nsIPropertyBag2)
      .getProperty("version");
  // the next line is correct: Mac OS 10.6 corresponds to Darwin version 10.x !
  // Mac OS 10.7 is Darwin version 11.x. the |version| string we've got here
  // is the Darwin version.
  return (parseFloat(version) < 11.0);
}

(function(){
  var el = document.createElement("link");
  el.rel = "stylesheet";
  el.type = "text/css";
  el.href= "/tests/SimpleTest/test.css";
  document.head.appendChild(el);
}());
