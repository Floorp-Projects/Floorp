/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Cc = SpecialPowers.Cc;
var Ci = SpecialPowers.Ci;
var Cr = SpecialPowers.Cr;

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
function createHTML(meta) {
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
  anchor.setAttribute('target', '_blank');

  if (meta.bug) {
    anchor.setAttribute('href', 'https://bugzilla.mozilla.org/show_bug.cgi?id=' + meta.bug);
  }

  anchor.textContent = meta.title;
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
  if (element)
    return element;

  element = document.createElement(type === 'audio' ? 'audio' : 'video');
  element.setAttribute('id', id);
  element.setAttribute('height', 100);
  element.setAttribute('width', 150);
  element.setAttribute('controls', 'controls');
  document.getElementById('content').appendChild(element);

  return element;
}


/**
 * Wrapper function for mozGetUserMedia to allow a singular area of control
 * for determining whether we run this with fake devices or not.
 *
 * @param {Dictionary} constraints
 *        The constraints for this mozGetUserMedia callback
 */
function getUserMedia(constraints) {
  if (!("fake" in constraints) && FAKE_ENABLED) {
    constraints["fake"] = FAKE_ENABLED;
  }

  info("Call getUserMedia for " + JSON.stringify(constraints));
  return navigator.mediaDevices.getUserMedia(constraints);
}


/**
 * Setup any Mochitest for WebRTC by enabling the preference for
 * peer connections. As by bug 797979 it will also enable mozGetUserMedia()
 * and disable the mozGetUserMedia() permission checking.
 *
 * @param {Function} aCallback
 *        Test method to execute after initialization
 */
function runTest(aCallback) {
  if (window.SimpleTest) {
    // Running as a Mochitest.
    SimpleTest.waitForExplicitFinish();
    SimpleTest.requestFlakyTimeout("WebRTC inherently depends on timeouts");
    SpecialPowers.pushPrefEnv({'set': [
      ['dom.messageChannel.enabled', true],
      ['media.peerconnection.enabled', true],
      ['media.peerconnection.identity.enabled', true],
      ['media.peerconnection.identity.timeout', 12000],
      ['media.peerconnection.default_iceservers', '[]'],
      ['media.navigator.permission.disabled', true],
      ['media.getusermedia.screensharing.enabled', true],
      ['media.getusermedia.screensharing.allowed_domains', "mochi.test"]]
    }, function () {
      try {
        aCallback();
      }
      catch (err) {
        generateErrorCallback()(err);
      }
    });
  } else {
    // Steeplechase, let it call the callback.
    window.run_test = function(is_initiator) {
      var options = {is_local: is_initiator,
                     is_remote: !is_initiator};
      aCallback(options);
    };
    // Also load the steeplechase test code.
    var s = document.createElement("script");
    s.src = "/test.js";
    document.head.appendChild(s);
  }
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
  if(constraints[type]) {
    is(mediaStreamTracks.length, 1, 'One ' + type + ' track shall be present');

    if(mediaStreamTracks.length) {
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
var unexpectedEventArrivedPromise = new Promise((x, reject) => {
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
 * Implements the event guard pattern used throughout.  Each of the 'onxxx'
 * attributes on the wrappers can be set with a custom handler.  Prior to the
 * handler being set, if the event fires, it causes the test execution to halt.
 * Once but that handler is used exactly once, and subsequent events will also
 * cause test execution to halt.
 *
 * @param {object} wrapper
 *        The wrapper on which the psuedo-handler is installed
 * @param {object} obj
 *        The real source of events
 * @param {string} event
 *        The name of the event
 * @param {function} redirect
 *        (Optional) a function that determines what is passed to the event
 *        handler. By default, the handler is passed the wrapper (as opposed to
 *        the normal cases where they receive an event object).  This redirect
 *        function is passed the event.
 */
function guardEvent(wrapper, obj, event, redirect) {
  redirect = redirect || (e => wrapper);
  var onx = 'on' + event;
  var unexpected = unexpectedEvent(wrapper, event);
  wrapper[onx] = unexpected;
  obj[onx] = e => {
    info(wrapper + ': "on' + event + '" event fired');
    wrapper[onx](redirect(e));
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
        return Promise.race([
          next(this._framework),
          unexpectedEventArrivedPromise
        ]);
      });
    }, Promise.resolve())
      .catch(e =>
             ok(false, 'Error in test execution: ' + e +
                ((typeof e.stack === 'string') ?
                 (' ' + e.stack.split('\n').join(' ... ')) : '')));
  },

  /**
   * Add new commands to the end of the chain
   *
   * @param {function[]} commands
   *        List of command functions
   */
  append: function(commands) {
    this.commands = this.commands.concat(commands);
  },

  /**
   * Returns the index of the specified command in the chain.
   *
   * @param {function|string} id
   *        Command function or name
   * @returns {number} Index of the command
   */
  indexOf: function (id) {
    if (typeof id === 'string') {
      return this.commands.findIndex(f => f.name === id);
    }
    return this.commands.indexOf(id);
  },

  /**
   * Inserts the new commands after the specified command.
   *
   * @param {function|string} id
   *        Command function or name
   * @param {function[]} commands
   *        List of commands
   */
  insertAfter: function (id, commands) {
    this._insertHelper(id, commands, 1);
  },

  /**
   * Inserts the new commands before the specified command.
   *
   * @param {string} id
   *        Command function or name
   * @param {function[]} commands
   *        List of commands
   */
  insertBefore: function (id, commands) {
    this._insertHelper(id, commands);
  },

  _insertHelper: function(id, commands, delta) {
    delta = delta || 0;
    var index = this.indexOf(id);

    if (index >= 0) {
      this.commands = [].concat(
        this.commands.slice(0, index + delta),
        commands,
        this.commands.slice(index + delta));
    }
  },

  /**
   * Removes the specified command
   *
   * @param {function|string} id
   *         Command function or name
   * @returns {function[]} Removed commands
   */
  remove: function (id) {
    var index = this.indexOf(id);
    if (index >= 0) {
      return this.commands.splice(index, 1);
    }
    return [];
  },

  /**
   * Removes all commands after the specified one.
   *
   * @param {function|string} id
   *        Command function or name
   * @returns {function[]} Removed commands
   */
  removeAfter: function (id) {
    var index = this.indexOf(id);
    if (index >= 0) {
      return this.commands.splice(index + 1);
    }
    return [];
  },

  /**
   * Removes all commands before the specified one.
   *
   * @param {function|string} id
   *        Command function or name
   * @returns {function[]} Removed commands
   */
  removeBefore: function (id) {
    var index = this.indexOf(id);
    if (index >= 0) {
      return this.commands.splice(0, index);
    }
    return [];
  },

  /**
   * Replaces a single command.
   *
   * @param {function|string} id
   *        Command function or name
   * @param {function[]} commands
   *        List of commands
   * @returns {function[]} Removed commands
   */
  replace: function (id, commands) {
    this.insertBefore(id, commands);
    return this.remove(id);
  },

  /**
   * Replaces all commands after the specified one.
   *
   * @param {function|string} id
   *        Command function or name
   * @returns {object[]} Removed commands
   */
  replaceAfter: function (id, commands) {
    var oldCommands = this.removeAfter(id);
    this.append(commands);
    return oldCommands;
  },

  /**
   * Replaces all commands before the specified one.
   *
   * @param {function|string} id
   *        Command function or name
   * @returns {object[]} Removed commands
   */
  replaceBefore: function (id, commands) {
    var oldCommands = this.removeBefore(id);
    this.insertBefore(id, commands);
    return oldCommands;
  },

  /**
   * Remove all commands whose name match the specified regex.
   *
   * @param {regex} id_match
   *        Regular expression to match command names.
   */
  filterOut: function (id_match) {
    this.commands = this.commands.filter(c => !id_match.test(c.name));
  }
};


function IsMacOSX10_6orOlder() {
    var is106orOlder = false;

    if (navigator.platform.indexOf("Mac") == 0) {
        var version = Cc["@mozilla.org/system-info;1"]
                        .getService(SpecialPowers.Ci.nsIPropertyBag2)
                        .getProperty("version");
        // the next line is correct: Mac OS 10.6 corresponds to Darwin version 10.x !
        // Mac OS 10.7 is Darwin version 11.x. the |version| string we've got here
        // is the Darwin version.
        is106orOlder = (parseFloat(version) < 11.0);
    }
    return is106orOlder;
}
