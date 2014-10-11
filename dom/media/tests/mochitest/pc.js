/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const LOOPBACK_ADDR = "127.0.0.";

const iceStateTransitions = {
  "new": ["checking", "closed"], //Note: 'failed' might need to added here
                                 //      even though it is not in the standard
  "checking": ["new", "connected", "failed", "closed"], //Note: do we need to
                                                        // allow 'completed' in
                                                        // here as well?
  "connected": ["new", "completed", "disconnected", "closed"],
  "completed": ["new", "disconnected", "closed"],
  "disconnected": ["new", "connected", "completed", "failed", "closed"],
  "failed": ["new", "disconnected", "closed"],
  "closed": []
  }

const signalingStateTransitions = {
  "stable": ["have-local-offer", "have-remote-offer", "closed"],
  "have-local-offer": ["have-remote-pranswer", "stable", "closed", "have-local-offer"],
  "have-remote-pranswer": ["stable", "closed", "have-remote-pranswer"],
  "have-remote-offer": ["have-local-pranswer", "stable", "closed", "have-remote-offer"],
  "have-local-pranswer": ["stable", "closed", "have-local-pranswer"],
  "closed": []
}

/**
 * This class mimics a state machine and handles a list of commands by
 * executing them synchronously.
 *
 * @constructor
 * @param {object} framework
 *        A back reference to the framework which makes use of the class. It's
 *        getting passed in as parameter to each command callback.
 * @param {Array[]} [commandList=[]]
 *        Default commands to set during initialization
 */
function CommandChain(framework, commandList) {
  this._framework = framework;

  this._commands = commandList || [ ];
  this._current = 0;

  this.onFinished = null;
}

CommandChain.prototype = {

  /**
   * Returns the index of the current command of the chain
   *
   * @returns {number} Index of the current command
   */
  get current() {
    return this._current;
  },

  /**
   * Checks if the chain has already processed all the commands
   *
   * @returns {boolean} True, if all commands have been processed
   */
  get finished() {
    return this._current === this._commands.length;
  },

  /**
   * Returns the assigned commands of the chain.
   *
   * @returns {Array[]} Commands of the chain
   */
  get commands() {
    return this._commands;
  },

  /**
   * Sets new commands for the chain. All existing commands will be replaced.
   *
   * @param {Array[]} commands
   *        List of commands
   */
  set commands(commands) {
    this._commands = commands;
  },

  /**
   * Execute the next command in the chain.
   */
  executeNext : function () {
    var self = this;

    function _executeNext() {
      if (!self.finished) {
        var step = self._commands[self._current];
        self._current++;

        self.currentStepLabel = step[0];
        info("Run step: " + self.currentStepLabel);
        step[1](self._framework);      // Execute step
      }
      else if (typeof(self.onFinished) === 'function') {
        self.onFinished();
      }
    }

    // To prevent building up the stack we have to execute the next
    // step asynchronously
    window.setTimeout(_executeNext, 0);
  },

  /**
   * Add new commands to the end of the chain
   *
   * @param {Array[]} commands
   *        List of commands
   */
  append: function (commands) {
    this._commands = this._commands.concat(commands);
  },

  /**
   * Returns the index of the specified command in the chain.
   *
   * @param {string} id
   *        Identifier of the command
   * @returns {number} Index of the command
   */
  indexOf: function (id) {
    for (var i = 0; i < this._commands.length; i++) {
      if (this._commands[i][0] === id) {
        return i;
      }
    }

    return -1;
  },

  /**
   * Inserts the new commands after the specified command.
   *
   * @param {string} id
   *        Identifier of the command
   * @param {Array[]} commands
   *        List of commands
   */
  insertAfter: function (id, commands) {
    var index = this.indexOf(id);

    if (index > -1) {
      var tail = this.removeAfter(id);

      this.append(commands);
      this.append(tail);
    }
  },

  /**
   * Inserts the new commands before the specified command.
   *
   * @param {string} id
   *        Identifier of the command
   * @param {Array[]} commands
   *        List of commands
   */
  insertBefore: function (id, commands) {
    var index = this.indexOf(id);

    if (index > -1) {
      var tail = this.removeAfter(id);
      var object = this.remove(id);

      this.append(commands);
      this.append(object);
      this.append(tail);
    }
  },

  /**
   * Removes the specified command
   *
   * @param {string} id
   *        Identifier of the command
   * @returns {object[]} Removed command
   */
  remove : function (id) {
    return this._commands.splice(this.indexOf(id), 1);
  },

  /**
   * Removes all commands after the specified one.
   *
   * @param {string} id
   *        Identifier of the command
   * @returns {object[]} Removed commands
   */
  removeAfter : function (id) {
    var index = this.indexOf(id);

    if (index > -1) {
      return this._commands.splice(index + 1);
    }

    return null;
  },

  /**
   * Removes all commands before the specified one.
   *
   * @param {string} id
   *        Identifier of the command
   * @returns {object[]} Removed commands
   */
  removeBefore : function (id) {
    var index = this.indexOf(id);

    if (index > -1) {
      return this._commands.splice(0, index);
    }

    return null;
  },

  /**
   * Replaces a single command.
   *
   * @param {string} id
   *        Identifier of the command to be replaced
   * @param {Array[]} commands
   *        List of commands
   * @returns {object[]} Removed commands
   */
  replace : function (id, commands) {
    this.insertBefore(id, commands);
    return this.remove(id);
  },

  /**
   * Replaces all commands after the specified one.
   *
   * @param {string} id
   *        Identifier of the command
   * @returns {object[]} Removed commands
   */
  replaceAfter : function (id, commands) {
    var oldCommands = this.removeAfter(id);
    this.append(commands);

    return oldCommands;
  },

  /**
   * Replaces all commands before the specified one.
   *
   * @param {string} id
   *        Identifier of the command
   * @returns {object[]} Removed commands
   */
  replaceBefore : function (id, commands) {
    var oldCommands = this.removeBefore(id);
    this.insertBefore(id, commands);

    return oldCommands;
  },

  /**
   * Remove all commands whose identifiers match the specified regex.
   *
   * @param {regex} id_match
   *        Regular expression to match command identifiers.
   */
  filterOut : function (id_match) {
    for (var i = this._commands.length - 1; i >= 0; i--) {
      if (id_match.test(this._commands[i][0])) {
        this._commands.splice(i, 1);
      }
    }
  }
};

/**
 * This class provides a state checker for media elements which store
 * a media stream to check for media attribute state and events fired.
 * When constructed by a caller, an object instance is created with
 * a media element, event state checkers for canplaythrough, timeupdate, and
 * time changing on the media element and stream.
 *
 * @param {HTMLMediaElement} element the media element being analyzed
 */
function MediaElementChecker(element) {
  this.element = element;
  this.canPlayThroughFired = false;
  this.timeUpdateFired = false;
  this.timePassed = false;

  var self = this;
  var elementId = self.element.getAttribute('id');

  // When canplaythrough fires, we track that it's fired and remove the
  // event listener.
  var canPlayThroughCallback = function() {
    info('canplaythrough fired for media element ' + elementId);
    self.canPlayThroughFired = true;
    self.element.removeEventListener('canplaythrough', canPlayThroughCallback,
                                     false);
  };

  // When timeupdate fires, we track that it's fired and check if time
  // has passed on the media stream and media element.
  var timeUpdateCallback = function() {
    self.timeUpdateFired = true;
    info('timeupdate fired for media element ' + elementId);

    // If time has passed, then track that and remove the timeupdate event
    // listener.
    if(element.mozSrcObject && element.mozSrcObject.currentTime > 0 &&
       element.currentTime > 0) {
      info('time passed for media element ' + elementId);
      self.timePassed = true;
      self.element.removeEventListener('timeupdate', timeUpdateCallback,
                                       false);
    }
  };

  element.addEventListener('canplaythrough', canPlayThroughCallback, false);
  element.addEventListener('timeupdate', timeUpdateCallback, false);
}

MediaElementChecker.prototype = {

  /**
   * Waits until the canplaythrough & timeupdate events to fire along with
   * ensuring time has passed on the stream and media element.
   *
   * @param {Function} onSuccess the success callback when media flow is
   *                             established
   */
  waitForMediaFlow : function MEC_WaitForMediaFlow(onSuccess) {
    var self = this;
    var elementId = self.element.getAttribute('id');
    info('Analyzing element: ' + elementId);

    if(self.canPlayThroughFired && self.timeUpdateFired && self.timePassed) {
      ok(true, 'Media flowing for ' + elementId);
      onSuccess();
    } else {
      setTimeout(function() {
        self.waitForMediaFlow(onSuccess);
      }, 100);
    }
  },

  /**
   * Checks if there is no media flow present by checking that the ready
   * state of the media element is HAVE_METADATA.
   */
  checkForNoMediaFlow : function MEC_CheckForNoMediaFlow() {
    ok(this.element.readyState === HTMLMediaElement.HAVE_METADATA,
       'Media element has a ready state of HAVE_METADATA');
  }
};

/**
 * Only calls info() if SimpleTest.info() is available
 */
function safeInfo(message) {
  if (typeof(info) === "function") {
    info(message);
  }
}

// Also remove mode 0 if it's offered
// Note, we don't bother removing the fmtp lines, which makes a good test
// for some SDP parsing issues.
function removeVP8(sdp) {
  var updated_sdp = sdp.replace("a=rtpmap:120 VP8/90000\r\n","");
  updated_sdp = updated_sdp.replace("RTP/SAVPF 120 126 97\r\n","RTP/SAVPF 126 97\r\n");
  updated_sdp = updated_sdp.replace("RTP/SAVPF 120 126\r\n","RTP/SAVPF 126\r\n");
  updated_sdp = updated_sdp.replace("a=rtcp-fb:120 nack\r\n","");
  updated_sdp = updated_sdp.replace("a=rtcp-fb:120 nack pli\r\n","");
  updated_sdp = updated_sdp.replace("a=rtcp-fb:120 ccm fir\r\n","");
  return updated_sdp;
}

/**
 * Query function for determining if any IP address is available for
 * generating SDP.
 *
 * @return false if required additional network setup.
 */
function isNetworkReady() {
  // for gonk platform
  if ("nsINetworkInterfaceListService" in SpecialPowers.Ci) {
    var listService = SpecialPowers.Cc["@mozilla.org/network/interface-list-service;1"]
                        .getService(SpecialPowers.Ci.nsINetworkInterfaceListService);
    var itfList = listService.getDataInterfaceList(
          SpecialPowers.Ci.nsINetworkInterfaceListService.LIST_NOT_INCLUDE_MMS_INTERFACES |
          SpecialPowers.Ci.nsINetworkInterfaceListService.LIST_NOT_INCLUDE_SUPL_INTERFACES |
          SpecialPowers.Ci.nsINetworkInterfaceListService.LIST_NOT_INCLUDE_IMS_INTERFACES |
          SpecialPowers.Ci.nsINetworkInterfaceListService.LIST_NOT_INCLUDE_DUN_INTERFACES);
    var num = itfList.getNumberOfInterface();
    for (var i = 0; i < num; i++) {
      var ips = {};
      var prefixLengths = {};
      var length = itfList.getInterface(i).getAddresses(ips, prefixLengths);

      for (var j = 0; j < length; j++) {
        var ip = ips.value[j];
        // skip IPv6 address until bug 797262 is implemented
        if (ip.indexOf(":") < 0) {
          safeInfo("Network interface is ready with address: " + ip);
          return true;
        }
      }
    }
    // ip address is not available
    safeInfo("Network interface is not ready, required additional network setup");
    return false;
  }
  safeInfo("Network setup is not required");
  return true;
}

/**
 * Network setup utils for Gonk
 *
 * @return {object} providing functions for setup/teardown data connection
 */
function getNetworkUtils() {
  var url = SimpleTest.getTestFileURL("NetworkPreparationChromeScript.js");
  var script = SpecialPowers.loadChromeScript(url);

  var utils = {
    /**
     * Utility for setting up data connection.
     *
     * @param aCallback callback after data connection is ready.
     */
    prepareNetwork: function(onSuccess) {
      script.addMessageListener('network-ready', function (message) {
        info("Network interface is ready");
        onSuccess();
      });
      info("Setting up network interface");
      script.sendAsyncMessage("prepare-network", true);
    },
    /**
     * Utility for tearing down data connection.
     *
     * @param aCallback callback after data connection is closed.
     */
    tearDownNetwork: function(onSuccess, onFailure) {
      if (isNetworkReady()) {
        script.addMessageListener('network-disabled', function (message) {
          info("Network interface torn down");
          script.destroy();
          onSuccess();
        });
        info("Tearing down network interface");
        script.sendAsyncMessage("network-cleanup", true);
      } else {
        info("No network to tear down");
        onFailure();
      }
    }
  };

  return utils;
}

/**
 * Setup network on Gonk if needed and execute test once network is up
 *
 */
function startNetworkAndTest(onSuccess) {
  if (!isNetworkReady()) {
    SimpleTest.waitForExplicitFinish();
    var utils = getNetworkUtils();
    // Trigger network setup to obtain IP address before creating any PeerConnection.
    utils.prepareNetwork(onSuccess);
  } else {
    onSuccess();
  }
}

/**
 * A wrapper around SimpleTest.finish() to handle B2G network teardown
 */
function networkTestFinished() {
  if ("nsINetworkInterfaceListService" in SpecialPowers.Ci) {
    var utils = getNetworkUtils();
    utils.tearDownNetwork(SimpleTest.finish, SimpleTest.finish);
  } else {
    SimpleTest.finish();
  }
}

/**
 * A wrapper around runTest() which handles B2G network setup and teardown
 */
function runNetworkTest(testFunction) {
  startNetworkAndTest(function() {
    runTest(testFunction);
  });
}

/**
 * This class handles tests for peer connections.
 *
 * @constructor
 * @param {object} [options={}]
 *        Optional options for the peer connection test
 * @param {object} [options.commands=commandsPeerConnection]
 *        Commands to run for the test
 * @param {bool}   [options.is_local=true]
 *        true if this test should run the tests for the "local" side.
 * @param {bool}   [options.is_remote=true]
 *        true if this test should run the tests for the "remote" side.
 * @param {object} [options.config_local=undefined]
 *        Configuration for the local peer connection instance
 * @param {object} [options.config_remote=undefined]
 *        Configuration for the remote peer connection instance. If not defined
 *        the configuration from the local instance will be used
 */
function PeerConnectionTest(options) {
  // If no options are specified make it an empty object
  options = options || { };
  options.commands = options.commands || commandsPeerConnection;
  options.is_local = "is_local" in options ? options.is_local : true;
  options.is_remote = "is_remote" in options ? options.is_remote : true;

  if (typeof turnServers !== "undefined") {
    if ((!options.turn_disabled_local) && (turnServers.local)) {
      if (!options.hasOwnProperty("config_local")) {
        options.config_local = {};
      }
      if (!options.config_local.hasOwnProperty("iceServers")) {
        options.config_local.iceServers = turnServers.local.iceServers;
      }
    }
    if ((!options.turn_disabled_remote) && (turnServers.remote)) {
      if (!options.hasOwnProperty("config_remote")) {
        options.config_remote = {};
      }
      if (!options.config_remote.hasOwnProperty("iceServers")) {
        options.config_remote.iceServers = turnServers.remote.iceServers;
      }
    }
  }

  if (options.is_local)
    this.pcLocal = new PeerConnectionWrapper('pcLocal', options.config_local, options.h264);
  else
    this.pcLocal = null;

  if (options.is_remote)
    this.pcRemote = new PeerConnectionWrapper('pcRemote', options.config_remote || options.config_local, options.h264);
  else
    this.pcRemote = null;

  this.steeplechase = this.pcLocal === null || this.pcRemote === null;

  // Create command chain instance and assign default commands
  this.chain = new CommandChain(this, options.commands);
  if (!options.is_local) {
    this.chain.filterOut(/^PC_LOCAL/);
  }
  if (!options.is_remote) {
    this.chain.filterOut(/^PC_REMOTE/);
  }

  var self = this;
  this.chain.onFinished = function () {
    self.teardown();
  };
}

/**
 * Closes the peer connection if it is active
 *
 * @param {Function} onSuccess
 *        Callback to execute when the peer connection has been closed successfully
 */
PeerConnectionTest.prototype.close = function PCT_close(onSuccess) {
  info("Closing peer connections");

  var self = this;
  var closeTimeout = null;
  var waitingForLocal = false;
  var waitingForRemote = false;
  var everythingClosed = false;

  function verifyClosed() {
    if ((self.waitingForLocal || self.waitingForRemote) ||
      (self.pcLocal && (self.pcLocal.signalingState !== "closed")) ||
      (self.pcRemote && (self.pcRemote.signalingState !== "closed"))) {
      info("still waiting for closure");
    }
    else if (!everythingClosed) {
      info("No closure pending");
      if (self.pcLocal) {
        is(self.pcLocal.signalingState, "closed", "pcLocal is in 'closed' state");
      }
      if (self.pcRemote) {
        is(self.pcRemote.signalingState, "closed", "pcRemote is in 'closed' state");
      }
      clearTimeout(closeTimeout);
      everythingClosed = true;
      onSuccess();
    }
  }

  function signalingstatechangeLocalClose(state) {
    info("'onsignalingstatechange' event '" + state + "' received");
    is(state, "closed", "onsignalingstatechange event is closed");
    self.waitingForLocal = false;
    verifyClosed();
  }

  function signalingstatechangeRemoteClose(state) {
    info("'onsignalingstatechange' event '" + state + "' received");
    is(state, "closed", "onsignalingstatechange event is closed");
    self.waitingForRemote = false;
    verifyClosed();
  }

  function closeEverything() {
    if ((self.pcLocal) && (self.pcLocal.signalingState !== "closed")) {
      info("Closing pcLocal");
      self.pcLocal.onsignalingstatechange = signalingstatechangeLocalClose;
      self.waitingForLocal = true;
      self.pcLocal.close();
    }
    if ((self.pcRemote) && (self.pcRemote.signalingState !== "closed")) {
      info("Closing pcRemote");
      self.pcRemote.onsignalingstatechange = signalingstatechangeRemoteClose;
      self.waitingForRemote = true;
      self.pcRemote.close();
    }
    // give the signals handlers time to fire
    setTimeout(verifyClosed, 1000);
  }

  closeTimeout = setTimeout(function() {
    var closed = ((self.pcLocal && (self.pcLocal.signalingState === "closed")) &&
      (self.pcRemote && (self.pcRemote.signalingState === "closed")));
    ok(closed, "Closing PeerConnections timed out");
    // it is not a success, but the show must go on
    onSuccess();
  }, 60000);

  closeEverything();
};

/**
 * Executes the next command.
 */
PeerConnectionTest.prototype.next = function PCT_next() {
  if (this._stepTimeout) {
    clearTimeout(this._stepTimeout);
    this._stepTimeout = null;
  }
  this.chain.executeNext();
};

/**
 * Set a timeout for the current step.
 * @param {long] ms the number of milliseconds to allow for this step
 */
PeerConnectionTest.prototype.setStepTimeout = function(ms) {
  this._stepTimeout = setTimeout(function() {
    ok(false, "Step timed out: " + this.chain.currentStepLabel);
    this.next();
  }.bind(this), ms);
};

/**
 * Creates an answer for the specified peer connection instance
 * and automatically handles the failure case.
 *
 * @param {PeerConnectionWrapper} peer
 *        The peer connection wrapper to run the command on
 * @param {function} onSuccess
 *        Callback to execute if the offer was created successfully
 */
PeerConnectionTest.prototype.createAnswer =
function PCT_createAnswer(peer, onSuccess) {
  var self = this;

  peer.createAnswer(function (answer) {
    // make a copy so this does not get updated with ICE candidates
    self.originalAnswer = new mozRTCSessionDescription(JSON.parse(JSON.stringify(answer)));
    onSuccess(answer);
  });
};

/**
 * Creates an offer for the specified peer connection instance
 * and automatically handles the failure case.
 *
 * @param {PeerConnectionWrapper} peer
 *        The peer connection wrapper to run the command on
 * @param {function} onSuccess
 *        Callback to execute if the offer was created successfully
 */
PeerConnectionTest.prototype.createOffer =
function PCT_createOffer(peer, onSuccess) {
  var self = this;

  peer.createOffer(function (offer) {
    // make a copy so this does not get updated with ICE candidates
    self.originalOffer = new mozRTCSessionDescription(JSON.parse(JSON.stringify(offer)));
    onSuccess(offer);
  });
};

PeerConnectionTest.prototype.setIdentityProvider =
function(peer, provider, protocol, identity) {
  peer.setIdentityProvider(provider, protocol, identity);
};

/**
 * Sets the local description for the specified peer connection instance
 * and automatically handles the failure case.
 *
 * @param {PeerConnectionWrapper} peer
          The peer connection wrapper to run the command on
 * @param {mozRTCSessionDescription} desc
 *        Session description for the local description request
 * @param {function} onSuccess
 *        Callback to execute if the local description was set successfully
 */
PeerConnectionTest.prototype.setLocalDescription =
function PCT_setLocalDescription(peer, desc, stateExpected, onSuccess) {
  var eventFired = false;
  var stateChanged = false;

  function check_next_test() {
    if (eventFired && stateChanged) {
      onSuccess();
    }
  }

  peer.onsignalingstatechange = function (state) {
    info(peer + ": 'onsignalingstatechange' event '" + state + "' received");
    if(stateExpected === state && eventFired == false) {
      eventFired = true;
      peer.setLocalDescStableEventDate = new Date();
      check_next_test();
    } else {
      ok(false, "This event has either already fired or there has been a " +
                "mismatch between event received " + state +
                " and event expected " + stateExpected);
    }
  };

  peer.setLocalDescription(desc, function () {
    stateChanged = true;
    peer.setLocalDescDate = new Date();
    check_next_test();
  });
};

/**
 * Sets the media constraints for both peer connection instances.
 *
 * @param {object} constraintsLocal
 *        Media constrains for the local peer connection instance
 * @param constraintsRemote
 */
PeerConnectionTest.prototype.setMediaConstraints =
function PCT_setMediaConstraints(constraintsLocal, constraintsRemote) {
  if (this.pcLocal)
    this.pcLocal.constraints = constraintsLocal;
  if (this.pcRemote)
    this.pcRemote.constraints = constraintsRemote;
};

/**
 * Sets the media options used on a createOffer call in the test.
 *
 * @param {object} options the media constraints to use on createOffer
 */
PeerConnectionTest.prototype.setOfferOptions =
function PCT_setOfferOptions(options) {
  if (this.pcLocal)
    this.pcLocal.offerOptions = options;
};

/**
 * Sets the remote description for the specified peer connection instance
 * and automatically handles the failure case.
 *
 * @param {PeerConnectionWrapper} peer
          The peer connection wrapper to run the command on
 * @param {mozRTCSessionDescription} desc
 *        Session description for the remote description request
 * @param {function} onSuccess
 *        Callback to execute if the local description was set successfully
 */
PeerConnectionTest.prototype.setRemoteDescription =
function PCT_setRemoteDescription(peer, desc, stateExpected, onSuccess) {
  var eventFired = false;
  var stateChanged = false;

  function check_next_test() {
    if (eventFired && stateChanged) {
      onSuccess();
    }
  }

  peer.onsignalingstatechange = function (state) {
    info(peer + ": 'onsignalingstatechange' event '" + state + "' received");
    if(stateExpected === state && eventFired == false) {
      eventFired = true;
      peer.setRemoteDescStableEventDate = new Date();
      check_next_test();
    } else {
      ok(false, "This event has either already fired or there has been a " +
                "mismatch between event received " + state +
                " and event expected " + stateExpected);
    }
  };

  peer.setRemoteDescription(desc, function () {
    stateChanged = true;
    peer.setRemoteDescDate = new Date();
    check_next_test();
  });
};

/**
 * Start running the tests as assigned to the command chain.
 */
PeerConnectionTest.prototype.run = function PCT_run() {
  this.next();
};

/**
 * Clean up the objects used by the test
 */
PeerConnectionTest.prototype.teardown = function PCT_teardown() {
  this.close(function () {
    info("Test finished");
    if (window.SimpleTest)
      networkTestFinished();
    else
      finish();
  });
};

/**
 * Routes ice candidates from one PCW to the other PCW
 */
PeerConnectionTest.prototype.iceCandidateHandler = function
PCT_iceCandidateHandler(caller, candidate) {
  var self = this;

  info("Received: " + JSON.stringify(candidate) + " from " + caller);

  var target = null;
  if (caller.contains("pcLocal")) {
    if (self.pcRemote) {
      target = self.pcRemote;
    }
  } else if (caller.contains("pcRemote")) {
    if (self.pcLocal) {
      target = self.pcLocal;
    }
  } else {
    ok(false, "received event from unknown caller: " + caller);
    return;
  }

  if (target) {
    target.storeOrAddIceCandidate(candidate);
  } else {
    info("sending ice candidate to signaling server");
    send_message({"type": "ice_candidate", "ice_candidate": candidate});
  }
};

/**
 * Installs a polling function for the socket.io client to read
 * all messages from the chat room into a message queue.
 */
PeerConnectionTest.prototype.setupSignalingClient = function
PCT_setupSignalingClient() {
  var self = this;

  self.signalingMessageQueue = [];
  self.signalingCallbacks = {};
  self.signalingLoopRun = true;

  function queueMessage(message) {
    info("Received signaling message: " + JSON.stringify(message));
    var fired = false;
    Object.keys(self.signalingCallbacks).forEach(function(name) {
      if (name === message.type) {
        info("Invoking callback for message type: " + name);
        self.signalingCallbacks[name](message);
        fired = true;
      }
    });
    if (!fired) {
      self.signalingMessageQueue.push(message);
      info("signalingMessageQueue.length: " + self.signalingMessageQueue.length);
    }
    if (self.signalingLoopRun) {
      wait_for_message().then(queueMessage);
    } else {
      info("Exiting signaling message event loop");
    }
  }

  wait_for_message().then(queueMessage);
}

/**
 * Sets a flag to stop reading further messages from the chat room.
 */
PeerConnectionTest.prototype.signalingMessagesFinished = function
PCT_signalingMessagesFinished() {
  this.signalingLoopRun = false;
}

/**
 * Callback to stop reading message from chat room once trickle ICE
 * on the far end is over.
 *
 * @param {string} caller
 *        The lable of the caller of the function
 */
PeerConnectionTest.prototype.signalEndOfTrickleIce = function
PCT_signalEndOfTrickleIce(caller) {
  if (this.steeplechase) {
    send_message({"type": "end_of_trickle_ice"});
  }
};

/**
 * Register a callback function to deliver messages from the chat room
 * directly instead of storing them in the message queue.
 *
 * @param {string} messageType
 *        For which message types should the callback get invoked.
 *
 * @param {function} onMessage
 *        The function which gets invoked if a message of the messageType
 *        has been received from the chat room.
 */
PeerConnectionTest.prototype.registerSignalingCallback = function
PCT_registerSignalingCallback(messageType, onMessage) {
  this.signalingCallbacks[messageType] = onMessage;
}

/**
 * Searches the message queue for the first message of a given type
 * and invokes the given callback function, or registers the callback
 * function for future messages if the queue contains no such message.
 *
 * @param {string} messageType
 *        The type of message to search and register for.
 *
 * @param {function} onMessage
 *        The callback function which gets invoked with the messages
 *        of the given mesage type.
 */
PeerConnectionTest.prototype.getSignalingMessage = function
PCT_getSignalingMessage(messageType, onMessage) {
  for(var i=0; i < this.signalingMessageQueue.length; i++) {
    if (messageType === this.signalingMessageQueue[i].type) {
      //FIXME
      info("invoking callback on message " + i + " from message queue, for message type:" + messageType);
      onMessage(this.signalingMessageQueue.splice(i, 1)[0]);
      return;
    }
  }
  this.registerSignalingCallback(messageType, onMessage);
}

/**
 * This class handles tests for data channels.
 *
 * @constructor
 * @param {object} [options={}]
 *        Optional options for the peer connection test
 * @param {object} [options.commands=commandsDataChannel]
 *        Commands to run for the test
 * @param {object} [options.config_local=undefined]
 *        Configuration for the local peer connection instance
 * @param {object} [options.config_remote=undefined]
 *        Configuration for the remote peer connection instance. If not defined
 *        the configuration from the local instance will be used
 */
function DataChannelTest(options) {
  options = options || { };
  options.commands = options.commands || commandsDataChannel;

  PeerConnectionTest.call(this, options);
}

DataChannelTest.prototype = Object.create(PeerConnectionTest.prototype, {
  close : {
    /**
     * Close the open data channels, followed by the underlying peer connection
     *
     * @param {Function} onSuccess
     *        Callback to execute when all connections have been closed
     */
    value : function DCT_close(onSuccess) {
      var self = this;
      var pendingDcClose = []
      var closeTimeout = null;

      info("DataChannelTest.close() called");

      function _closePeerConnection() {
        info("DataChannelTest closing PeerConnection");
        PeerConnectionTest.prototype.close.call(self, onSuccess);
      }

      function _closePeerConnectionCallback(index) {
        info("_closePeerConnection called with index " + index);
        var pos = pendingDcClose.indexOf(index);
        if (pos != -1) {
          pendingDcClose.splice(pos, 1);
        }
        else {
          info("_closePeerConnection index " + index + " is missing from pendingDcClose: " + pendingDcClose);
        }
        if (pendingDcClose.length === 0) {
          clearTimeout(closeTimeout);
          _closePeerConnection();
        }
      }

      var myDataChannels = null;
      if (self.pcLocal) {
        myDataChannels = self.pcLocal.dataChannels;
      }
      else if (self.pcRemote) {
        myDataChannels = self.pcRemote.dataChannels;
      }
      var length = myDataChannels.length;
      for (var i = 0; i < length; i++) {
        var dataChannel = myDataChannels[i];
        if (dataChannel.readyState !== "closed") {
          pendingDcClose.push(i);
          self.closeDataChannels(i, _closePeerConnectionCallback);
        }
      }
      if (pendingDcClose.length === 0) {
        _closePeerConnection();
      }
      else {
        closeTimeout = setTimeout(function() {
          ok(false, "Failed to properly close data channels: " +
            pendingDcClose);
          _closePeerConnection();
        }, 60000);
      }
    }
  },

  closeDataChannels : {
    /**
     * Close the specified data channels
     *
     * @param {Number} index
     *        Index of the data channels to close on both sides
     * @param {Function} onSuccess
     *        Callback to execute when the data channels has been closed
     */
    value : function DCT_closeDataChannels(index, onSuccess) {
      info("_closeDataChannels called with index: " + index);
      var localChannel = null;
      if (this.pcLocal) {
        localChannel = this.pcLocal.dataChannels[index];
      }
      var remoteChannel = null;
      if (this.pcRemote) {
        remoteChannel = this.pcRemote.dataChannels[index];
      }

      var self = this;
      var wait = false;
      var pollingMode = false;
      var everythingClosed = false;
      var verifyInterval = null;
      var remoteCloseTimer = null;

      function _allChannelsAreClosed() {
        var ret = null;
        if (localChannel) {
          ret = (localChannel.readyState === "closed");
        }
        if (remoteChannel) {
          if (ret !== null) {
            ret = (ret && (remoteChannel.readyState === "closed"));
          }
          else {
            ret = (remoteChannel.readyState === "closed");
          }
        }
        return ret;
      }

      function verifyClosedChannels() {
        if (everythingClosed) {
          // safety protection against events firing late
          return;
        }
        if (_allChannelsAreClosed) {
          ok(true, "DataChannel(s) have reached 'closed' state for data channel " + index);
          if (remoteCloseTimer !== null) {
            clearTimeout(remoteCloseTimer);
          }
          if (verifyInterval !== null) {
            clearInterval(verifyInterval);
          }
          everythingClosed = true;
          onSuccess(index);
        }
        else {
          info("Still waiting for DataChannel closure");
        }
      }

      if ((localChannel) && (localChannel.readyState !== "closed")) {
        // in case of steeplechase there is no far end, so we can only poll
        if (remoteChannel) {
          remoteChannel.onclose = function () {
            is(remoteChannel.readyState, "closed", "remoteChannel is in state 'closed'");
            verifyClosedChannels();
          };
        }
        else {
          pollingMode = true;
          verifyInterval = setInterval(verifyClosedChannels, 1000);
        }

        localChannel.close();
        wait = true;
      }
      if ((remoteChannel) && (remoteChannel.readyState !== "closed")) {
        if (localChannel) {
          localChannel.onclose = function () {
            is(localChannel.readyState, "closed", "localChannel is in state 'closed'");
            verifyClosedChannels();
          };

          // Apparently we are running a local test which has both ends of the
          // data channel locally available, so by default lets wait for the
          // remoteChannel.onclose handler from above to confirm closure on both
          // ends.
          remoteCloseTimer = setTimeout(function() {
            todo(false, "localChannel.close() did not resulted in close signal on remote side");
            remoteChannel.close();
            verifyClosedChannels();
          }, 30000);
        }
        else {
          pollingMode = true;
          verifyTimer = setInterval(verifyClosedChannels, 1000);

          remoteChannel.close();
        }

        wait = true;
      }

      if (!wait) {
        onSuccess(index);
      }
    }
  },

  createDataChannel : {
    /**
     * Create a data channel
     *
     * @param {Dict} options
     *        Options for the data channel (see nsIPeerConnection)
     * @param {Function} onSuccess
     *        Callback when the creation was successful
     */
    value : function DCT_createDataChannel(options, onSuccess) {
      var localChannel = null;
      var remoteChannel = null;
      var self = this;

      // Method to synchronize all asynchronous events.
      function check_next_test() {
        if (localChannel && remoteChannel) {
          onSuccess(localChannel, remoteChannel);
        }
      }

      if (!options.negotiated) {
        // Register handlers for the remote peer
        this.pcRemote.registerDataChannelOpenEvents(function (channel) {
          remoteChannel = channel;
          check_next_test();
        });
      }

      // Create the datachannel and handle the local 'onopen' event
      this.pcLocal.createDataChannel(options, function (channel) {
        localChannel = channel;

        if (options.negotiated) {
          // externally negotiated - we need to open from both ends
          options.id = options.id || channel.id;  // allow for no id to let the impl choose
          self.pcRemote.createDataChannel(options, function (channel) {
            remoteChannel = channel;
            check_next_test();
          });
        } else {
          check_next_test();
        }
      });
    }
  },

  send : {
    /**
     * Send data (message or blob) to the other peer
     *
     * @param {String|Blob} data
     *        Data to send to the other peer. For Blobs the MIME type will be lost.
     * @param {Function} onSuccess
     *        Callback to execute when data has been sent
     * @param {Object} [options={ }]
     *        Options to specify the data channels to be used
     * @param {DataChannelWrapper} [options.sourceChannel=pcLocal.dataChannels[length - 1]]
     *        Data channel to use for sending the message
     * @param {DataChannelWrapper} [options.targetChannel=pcRemote.dataChannels[length - 1]]
     *        Data channel to use for receiving the message
     */
    value : function DCT_send(data, onSuccess, options) {
      options = options || { };
      var source = options.sourceChannel ||
               this.pcLocal.dataChannels[this.pcLocal.dataChannels.length - 1];
      var target = options.targetChannel ||
               this.pcRemote.dataChannels[this.pcRemote.dataChannels.length - 1];

      // Register event handler for the target channel
      target.onmessage = function (recv_data) {
        onSuccess(target, recv_data);
      };

      source.send(data);
    }
  },

  createOffer : {
    value : function DCT_createOffer(peer, onSuccess) {
      PeerConnectionTest.prototype.createOffer.call(this, peer, onSuccess);
    }
  },

  setLocalDescription : {
    /**
     * Sets the local description for the specified peer connection instance
     * and automatically handles the failure case.
     *
     * @param {PeerConnectionWrapper} peer
              The peer connection wrapper to run the command on
     * @param {mozRTCSessionDescription} desc
     *        Session description for the local description request
     * @param {function} onSuccess
     *        Callback to execute if the local description was set successfully
     */
    value : function DCT_setLocalDescription(peer, desc, state, onSuccess) {
      PeerConnectionTest.prototype.setLocalDescription.call(this, peer,
                                                              desc, state, onSuccess);

    }
  },

  waitForInitialDataChannel : {
    /**
     * Wait for the initial data channel to get into the open state
     *
     * @param {PeerConnectionWrapper} peer
     *        The peer connection wrapper to run the command on
     * @param {Function} onSuccess
     *        Callback when the creation was successful
     */
    value : function DCT_waitForInitialDataChannel(peer, onSuccess, onFailure) {
      var dcConnectionTimeout = null;
      var dcOpened = false;

      function dataChannelConnected(channel) {
        // in case the switch statement below had called onSuccess already we
        // don't want to call it again
        if (!dcOpened) {
          clearTimeout(dcConnectionTimeout);
          is(channel.readyState, "open", peer + " dataChannels[0] switched to state: 'open'");
          dcOpened = true;
          onSuccess();
        }
      }

      // TODO: drno: convert dataChannels into an object and make
      //             registerDataChannelOpenEvent a generic function
      if (peer == this.pcLocal) {
        peer.dataChannels[0].onopen = dataChannelConnected;
      } else {
        peer.registerDataChannelOpenEvents(dataChannelConnected);
      }

      if (peer.dataChannels.length >= 1) {
        // snapshot of the live value as it might change during test execution
        const readyState = peer.dataChannels[0].readyState;
        switch (readyState) {
          case "open": {
            is(readyState, "open", peer + " dataChannels[0] is already in state: 'open'");
            dcOpened = true;
            onSuccess();
            break;
          }
          case "connecting": {
            is(readyState, "connecting", peer + " dataChannels[0] is in state: 'connecting'");
            if (onFailure) {
              dcConnectionTimeout = setTimeout(function () {
                is(peer.dataChannels[0].readyState, "open", peer + " timed out while waiting for dataChannels[0] to open");
                onFailure();
              }, 60000);
            }
            break;
          }
          default: {
            ok(false, "dataChannels[0] is in unexpected state " + readyState);
            if (onFailure) {
              onFailure()
            }
          }
        }
      }
    }
  }

});

/**
 * This class acts as a wrapper around a DataChannel instance.
 *
 * @param dataChannel
 * @param peerConnectionWrapper
 * @constructor
 */
function DataChannelWrapper(dataChannel, peerConnectionWrapper) {
  this._channel = dataChannel;
  this._pc = peerConnectionWrapper;

  info("Creating " + this);

  /**
   * Setup appropriate callbacks
   */

  this.onclose = unexpectedEventAndFinish(this, 'onclose');
  this.onerror = unexpectedEventAndFinish(this, 'onerror');
  this.onmessage = unexpectedEventAndFinish(this, 'onmessage');
  this.onopen = unexpectedEventAndFinish(this, 'onopen');

  var self = this;

  /**
   * Callback for native data channel 'onclose' events. If no custom handler
   * has been specified via 'this.onclose', a failure will be raised if an
   * event of this type gets caught.
   */
  this._channel.onclose = function () {
    info(self + ": 'onclose' event fired");

    self.onclose(self);
    self.onclose = unexpectedEventAndFinish(self, 'onclose');
  };

  /**
   * Callback for native data channel 'onmessage' events. If no custom handler
   * has been specified via 'this.onmessage', a failure will be raised if an
   * event of this type gets caught.
   *
   * @param {Object} event
   *        Event data which includes the sent message
   */
  this._channel.onmessage = function (event) {
    info(self + ": 'onmessage' event fired for '" + event.data + "'");

    self.onmessage(event.data);
    self.onmessage = unexpectedEventAndFinish(self, 'onmessage');
  };

  /**
   * Callback for native data channel 'onopen' events. If no custom handler
   * has been specified via 'this.onopen', a failure will be raised if an
   * event of this type gets caught.
   */
  this._channel.onopen = function () {
    info(self + ": 'onopen' event fired");

    self.onopen(self);
    self.onopen = unexpectedEventAndFinish(self, 'onopen');
  };
}

DataChannelWrapper.prototype = {
  /**
   * Returns the binary type of the channel
   *
   * @returns {String} The binary type
   */
  get binaryType() {
    return this._channel.binaryType;
  },

  /**
   * Sets the binary type of the channel
   *
   * @param {String} type
   *        The new binary type of the channel
   */
  set binaryType(type) {
    this._channel.binaryType = type;
  },

  /**
   * Returns the label of the underlying data channel
   *
   * @returns {String} The label
   */
  get label() {
    return this._channel.label;
  },

  /**
   * Returns the protocol of the underlying data channel
   *
   * @returns {String} The protocol
   */
  get protocol() {
    return this._channel.protocol;
  },

  /**
   * Returns the id of the underlying data channel
   *
   * @returns {number} The stream id
   */
  get id() {
    return this._channel.id;
  },

  /**
   * Returns the reliable state of the underlying data channel
   *
   * @returns {bool} The stream's reliable state
   */
  get reliable() {
    return this._channel.reliable;
  },

  // ordered, maxRetransmits and maxRetransmitTime not exposed yet

  /**
   * Returns the readyState bit of the data channel
   *
   * @returns {String} The state of the channel
   */
  get readyState() {
    return this._channel.readyState;
  },

  /**
   * Close the data channel
   */
  close : function () {
    info(this + ": Closing channel");
    this._channel.close();
  },

  /**
   * Send data through the data channel
   *
   * @param {String|Object} data
   *        Data which has to be sent through the data channel
   */
  send: function DCW_send(data) {
    info(this + ": Sending data '" + data + "'");
    this._channel.send(data);
  },

  /**
   * Returns the string representation of the class
   *
   * @returns {String} The string representation
   */
  toString: function DCW_toString() {
    return "DataChannelWrapper (" + this._pc.label + '_' + this._channel.label + ")";
  }
};


/**
 * This class acts as a wrapper around a PeerConnection instance.
 *
 * @constructor
 * @param {string} label
 *        Description for the peer connection instance
 * @param {object} configuration
 *        Configuration for the peer connection instance
 */
function PeerConnectionWrapper(label, configuration, h264) {
  this.configuration = configuration;
  this.label = label;
  this.whenCreated = Date.now();

  this.constraints = [ ];
  this.offerOptions = {};
  this.streams = [ ];
  this.mediaCheckers = [ ];

  this.dataChannels = [ ];

  this.onAddStreamFired = false;
  this.addStreamCallbacks = {};

  this.remoteDescriptionSet = false;
  this.endOfTrickleIce = false;
  this.localRequiresTrickleIce = false;
  this.remoteRequiresTrickleIce  = false;

  this.h264 = typeof h264 !== "undefined" ? true : false;

  info("Creating " + this);
  this._pc = new mozRTCPeerConnection(this.configuration);

  /**
   * Setup callback handlers
   */
  var self = this;
  // This enables tests to validate that the next ice state is the one they expect to happen
  this.next_ice_state = ""; // in most cases, the next state will be "checking", but in some tests "closed"
  // This allows test to register their own callbacks for ICE connection state changes
  this.ice_connection_callbacks = {};

  this._pc.oniceconnectionstatechange = function() {
    ok(self._pc.iceConnectionState !== undefined, "iceConnectionState should not be undefined");
    info(self + ": oniceconnectionstatechange fired, new state is: " + self._pc.iceConnectionState);
    Object.keys(self.ice_connection_callbacks).forEach(function(name) {
      self.ice_connection_callbacks[name]();
    });
    if (self.next_ice_state !== "") {
      is(self._pc.iceConnectionState, self.next_ice_state, "iceConnectionState changed to '" +
         self.next_ice_state + "'");
      self.next_ice_state = "";
    }
  };

  /**
   * Callback for native peer connection 'onaddstream' events.
   *
   * @param {Object} event
   *        Event data which includes the stream to be added
   */
  this._pc.onaddstream = function (event) {
    info(self + ": 'onaddstream' event fired for " + JSON.stringify(event.stream));
    // TODO: remove this once Bugs 998552 and 998546 are closed
    self.onAddStreamFired = true;

    var type = '';
    if (event.stream.getAudioTracks().length > 0) {
      type = 'audio';
    }
    if (event.stream.getVideoTracks().length > 0) {
      type += 'video';
    }
    self.attachMedia(event.stream, type, 'remote');

    Object.keys(self.addStreamCallbacks).forEach(function(name) {
      info(self + " calling addStreamCallback " + name);
      self.addStreamCallbacks[name]();
    });
   };

  this.ondatachannel = unexpectedEventAndFinish(this, 'ondatachannel');

  /**
   * Callback for native peer connection 'ondatachannel' events. If no custom handler
   * has been specified via 'this.ondatachannel', a failure will be raised if an
   * event of this type gets caught.
   *
   * @param {Object} event
   *        Event data which includes the newly created data channel
   */
  this._pc.ondatachannel = function (event) {
    info(self + ": 'ondatachannel' event fired for " + event.channel.label);

    self.ondatachannel(new DataChannelWrapper(event.channel, self));
    self.ondatachannel = unexpectedEventAndFinish(self, 'ondatachannel');
  };

  this.onsignalingstatechange = unexpectedEventAndFinish(this, 'onsignalingstatechange');
  this.signalingStateCallbacks = {};

  /**
   * Callback for native peer connection 'onsignalingstatechange' events. If no
   * custom handler has been specified via 'this.onsignalingstatechange', a
   * failure will be raised if an event of this type is caught.
   *
   * @param {Object} aEvent
   *        Event data which includes the newly created data channel
   */
  this._pc.onsignalingstatechange = function (anEvent) {
    info(self + ": 'onsignalingstatechange' event fired");

    Object.keys(self.signalingStateCallbacks).forEach(function(name) {
      self.signalingStateCallbacks[name](anEvent);
    });
    // this calls the eventhandler only once and then overwrites it with the
    // default unexpectedEvent handler
    self.onsignalingstatechange(anEvent);
    self.onsignalingstatechange = unexpectedEventAndFinish(self, 'onsignalingstatechange');
  };
}

PeerConnectionWrapper.prototype = {

  /**
   * Returns the local description.
   *
   * @returns {object} The local description
   */
  get localDescription() {
    return this._pc.localDescription;
  },

  /**
   * Sets the local description.
   *
   * @param {object} desc
   *        The new local description
   */
  set localDescription(desc) {
    this._pc.localDescription = desc;
  },

  /**
   * Returns the remote description.
   *
   * @returns {object} The remote description
   */
  get remoteDescription() {
    return this._pc.remoteDescription;
  },

  /**
   * Sets the remote description.
   *
   * @param {object} desc
   *        The new remote description
   */
  set remoteDescription(desc) {
    this._pc.remoteDescription = desc;
  },

  /**
   * Returns the signaling state.
   *
   * @returns {object} The local description
   */
  get signalingState() {
    return this._pc.signalingState;
  },
  /**
   * Returns the ICE connection state.
   *
   * @returns {object} The local description
   */
  get iceConnectionState() {
    return this._pc.iceConnectionState;
  },

  setIdentityProvider: function(provider, protocol, identity) {
      this._pc.setIdentityProvider(provider, protocol, identity);
  },

  /**
   * Callback when we get media from either side. Also an appropriate
   * HTML media element will be created.
   *
   * @param {MediaStream} stream
   *        Media stream to handle
   * @param {string} type
   *        The type of media stream ('audio' or 'video')
   * @param {string} side
   *        The location the stream is coming from ('local' or 'remote')
   */
  attachMedia : function PCW_attachMedia(stream, type, side) {
    function isSenderOfTrack(sender) {
      return sender.track == this;
    }

    info("Got media stream: " + type + " (" + side + ")");
    this.streams.push(stream);

    if (side === 'local') {
      // In order to test both the addStream and addTrack APIs, we do video one
      // way and audio + audiovideo the other.
      if (type == "video") {
        this._pc.addStream(stream);
        ok(this._pc.getSenders().find(isSenderOfTrack,
                                      stream.getVideoTracks()[0]),
           "addStream adds sender");
      } else {
        stream.getTracks().forEach(function(track) {
          var sender = this._pc.addTrack(track, stream);
          is(sender.track, track, "addTrack returns sender");
        }.bind(this));
      }
    }

    var element = createMediaElement(type, this.label + '_' + side);
    this.mediaCheckers.push(new MediaElementChecker(element));
    element.mozSrcObject = stream;
    element.play();
  },

  /**
   * Requests all the media streams as specified in the constrains property.
   *
   * @param {function} onSuccess
   *        Callback to execute if all media has been requested successfully
   */
  getAllUserMedia : function PCW_GetAllUserMedia(onSuccess) {
    var self = this;

    function _getAllUserMedia(constraintsList, index) {
      if (index < constraintsList.length) {
        var constraints = constraintsList[index];

        getUserMedia(constraints, function (stream) {
          var type = '';

          if (constraints.audio) {
            type = 'audio';
          }

          if (constraints.video) {
            type += 'video';
          }

          self.attachMedia(stream, type, 'local');

          _getAllUserMedia(constraintsList, index + 1);
        }, generateErrorCallback());
      } else {
        onSuccess();
      }
    }

    if (this.constraints.length === 0) {
      info("Skipping GUM: no UserMedia requested");
      onSuccess();
    }
    else {
      info("Get " + this.constraints.length + " local streams");
      _getAllUserMedia(this.constraints, 0);
    }
  },

  /**
   * Create a new data channel instance
   *
   * @param {Object} options
   *        Options which get forwarded to nsIPeerConnection.createDataChannel
   * @param {function} [onCreation=undefined]
   *        Callback to execute when the local data channel has been created
   * @returns {DataChannelWrapper} The created data channel
   */
  createDataChannel : function PCW_createDataChannel(options, onCreation) {
    var label = 'channel_' + this.dataChannels.length;
    info(this + ": Create data channel '" + label);

    var channel = this._pc.createDataChannel(label, options);
    var wrapper = new DataChannelWrapper(channel, this);

    if (onCreation) {
      wrapper.onopen = function () {
        onCreation(wrapper);
      };
    }

    this.dataChannels.push(wrapper);
    return wrapper;
  },

  /**
   * Creates an offer and automatically handles the failure case.
   *
   * @param {function} onSuccess
   *        Callback to execute if the offer was created successfully
   */
  createOffer : function PCW_createOffer(onSuccess) {
    var self = this;

    this._pc.createOffer(function (offer) {
      info("Got offer: " + JSON.stringify(offer));
      // note: this might get updated through ICE gathering
      self._latest_offer = offer;
      if (self.h264) {
        isnot(offer.sdp.search("H264/90000"), -1, "H.264 should be present in the SDP offer");
        offer.sdp = removeVP8(offer.sdp);
      }
      onSuccess(offer);
    }, generateErrorCallback(), this.offerOptions);
  },

  /**
   * Creates an answer and automatically handles the failure case.
   *
   * @param {function} onSuccess
   *        Callback to execute if the answer was created successfully
   */
  createAnswer : function PCW_createAnswer(onSuccess) {
    var self = this;

    this._pc.createAnswer(function (answer) {
      info(self + ": Got answer: " + JSON.stringify(answer));
      self._last_answer = answer;
      onSuccess(answer);
    }, generateErrorCallback());
  },

  /**
   * Sets the local description and automatically handles the failure case.
   *
   * @param {object} desc
   *        mozRTCSessionDescription for the local description request
   * @param {function} onSuccess
   *        Callback to execute if the local description was set successfully
   */
  setLocalDescription : function PCW_setLocalDescription(desc, onSuccess) {
    var self = this;
    this._pc.setLocalDescription(desc, function () {
      info(self + ": Successfully set the local description");
      onSuccess();
    }, generateErrorCallback());
  },

  /**
   * Tries to set the local description and expect failure. Automatically
   * causes the test case to fail if the call succeeds.
   *
   * @param {object} desc
   *        mozRTCSessionDescription for the local description request
   * @param {function} onFailure
   *        Callback to execute if the call fails.
   */
  setLocalDescriptionAndFail : function PCW_setLocalDescriptionAndFail(desc, onFailure) {
    var self = this;
    this._pc.setLocalDescription(desc,
      generateErrorCallback("setLocalDescription should have failed."),
      function (err) {
        info(self + ": As expected, failed to set the local description");
        onFailure(err);
    });
  },

  /**
   * Sets the remote description and automatically handles the failure case.
   *
   * @param {object} desc
   *        mozRTCSessionDescription for the remote description request
   * @param {function} onSuccess
   *        Callback to execute if the remote description was set successfully
   */
  setRemoteDescription : function PCW_setRemoteDescription(desc, onSuccess) {
    var self = this;
    this._pc.setRemoteDescription(desc, function () {
      info(self + ": Successfully set remote description");
      self.remoteDescriptionSet = true;
      if ((self._ice_candidates_to_add) &&
          (self._ice_candidates_to_add.length > 0)) {
        info("adding stored ice candidates");
        for (var i = 0; i < self._ice_candidates_to_add.length; i++) {
          self.addIceCandidate(self._ice_candidates_to_add[i]);
        }
        self._ice_candidates_to_add = [];
      }
      onSuccess();
    }, generateErrorCallback());
  },

  /**
   * Tries to set the remote description and expect failure. Automatically
   * causes the test case to fail if the call succeeds.
   *
   * @param {object} desc
   *        mozRTCSessionDescription for the remote description request
   * @param {function} onFailure
   *        Callback to execute if the call fails.
   */
  setRemoteDescriptionAndFail : function PCW_setRemoteDescriptionAndFail(desc, onFailure) {
    var self = this;
    this._pc.setRemoteDescription(desc,
      generateErrorCallback("setRemoteDescription should have failed."),
      function (err) {
        info(self + ": As expected, failed to set the remote description");
        onFailure(err);
    });
  },

  /**
   * Registers a callback for the signaling state change and
   * appends the new state to an array for logging it later.
   */
  logSignalingState: function PCW_logSignalingState() {
    var self = this;

    function _logSignalingState(state) {
      var newstate = self._pc.signalingState;
      var oldstate = self.signalingStateLog[self.signalingStateLog.length - 1]
      if (Object.keys(signalingStateTransitions).indexOf(oldstate) != -1) {
        ok(signalingStateTransitions[oldstate].indexOf(newstate) != -1, self + ": legal signaling state transition from " + oldstate + " to " + newstate);
      } else {
        ok(false, self + ": old signaling state " + oldstate + " missing in signaling transition array");
      }
      self.signalingStateLog.push(newstate);
    }

    self.signalingStateLog = [self._pc.signalingState];
    self.signalingStateCallbacks.logSignalingStatus = _logSignalingState;
  },

  /**
   * Either adds a given ICE candidate right away or stores it to be added
   * later, depending on the state of the PeerConnection.
   *
   * @param {object} candidate
   *        The mozRTCIceCandidate to be added or stored
   */
  storeOrAddIceCandidate : function PCW_storeOrAddIceCandidate(candidate) {
    var self = this;

    self._remote_ice_candidates.push(candidate);
    if (self.remoteDescriptionSet) {
      self.addIceCandidate(candidate);
    } else {
      self._ice_candidates_to_add.push(candidate);
    }
  },

  /**
   * Adds an ICE candidate and automatically handles the failure case.
   *
   * @param {object} candidate
   *        SDP candidate
   * @param {function} onSuccess
   *        Callback to execute if the local description was set successfully
   */
  addIceCandidate : function PCW_addIceCandidate(candidate, onSuccess) {
    var self = this;

    info(self + ": adding ICE candidate " + JSON.stringify(candidate));
    this._pc.addIceCandidate(candidate, function () {
      info(self + ": Successfully added an ICE candidate");
      if (onSuccess) {
        onSuccess();
      }
    }, generateErrorCallback());
  },

  /**
   * Tries to add an ICE candidate and expects failure. Automatically
   * causes the test case to fail if the call succeeds.
   *
   * @param {object} candidate
   *        SDP candidate
   * @param {function} onFailure
   *        Callback to execute if the call fails.
   */
  addIceCandidateAndFail : function PCW_addIceCandidateAndFail(candidate, onFailure) {
    var self = this;

    this._pc.addIceCandidate(candidate,
      generateErrorCallback("addIceCandidate should have failed."),
      function (err) {
        info(self + ": As expected, failed to add an ICE candidate");
        onFailure(err);
    }) ;
  },

  /**
   * Returns if the ICE the connection state is "connected".
   *
   * @returns {boolean} True if the connection state is "connected", otherwise false.
   */
  isIceConnected : function PCW_isIceConnected() {
    info(this + ": iceConnectionState = " + this.iceConnectionState);
    return this.iceConnectionState === "connected";
  },

  /**
   * Returns if the ICE the connection state is "checking".
   *
   * @returns {boolean} True if the connection state is "checking", otherwise false.
   */
  isIceChecking : function PCW_isIceChecking() {
    return this.iceConnectionState === "checking";
  },

  /**
   * Returns if the ICE the connection state is "new".
   *
   * @returns {boolean} True if the connection state is "new", otherwise false.
   */
  isIceNew : function PCW_isIceNew() {
    return this.iceConnectionState === "new";
  },

  /**
   * Checks if the ICE connection state still waits for a connection to get
   * established.
   *
   * @returns {boolean} True if the connection state is "checking" or "new",
   *  otherwise false.
   */
  isIceConnectionPending : function PCW_isIceConnectionPending() {
    return (this.isIceChecking() || this.isIceNew());
  },

  /**
   * Registers a callback for the ICE connection state change and
   * appends the new state to an array for logging it later.
   */
  logIceConnectionState: function PCW_logIceConnectionState() {
    var self = this;

    function logIceConState () {
      var newstate = self._pc.iceConnectionState;
      var oldstate = self.iceConnectionLog[self.iceConnectionLog.length - 1]
      if (Object.keys(iceStateTransitions).indexOf(oldstate) != -1) {
        ok(iceStateTransitions[oldstate].indexOf(newstate) != -1, self + ": legal ICE state transition from " + oldstate + " to " + newstate);
      } else {
        ok(false, self + ": old ICE state " + oldstate + " missing in ICE transition array");
      }
      self.iceConnectionLog.push(newstate);
    }

    self.iceConnectionLog = [self._pc.iceConnectionState];
    self.ice_connection_callbacks.logIceStatus = logIceConState;
  },

  /**
   * Registers a callback for the ICE connection state change and
   * reports success (=connected) or failure via the callbacks.
   * States "new" and "checking" are ignored.
   *
   * @param {function} onSuccess
   *        Callback if ICE connection status is "connected".
   * @param {function} onFailure
   *        Callback if ICE connection reaches a different state than
   *        "new", "checking" or "connected".
   */
  waitForIceConnected : function PCW_waitForIceConnected(onSuccess, onFailure) {
    var self = this;
    var mySuccess = onSuccess;
    var myFailure = onFailure;

    function iceConnectedChanged () {
      if (self.isIceConnected()) {
        delete self.ice_connection_callbacks.waitForIceConnected;
        mySuccess();
      } else if (! self.isIceConnectionPending()) {
        delete self.ice_connection_callbacks.waitForIceConnected;
        myFailure();
      }
    }

    self.ice_connection_callbacks.waitForIceConnected = iceConnectedChanged;
  },

  /**
   * Setup a onicecandidate handler
   *
   * @param {object} test
   *        A PeerConnectionTest object to which the ice candidates gets
   *        forwarded.
   */
  setupIceCandidateHandler : function
    PCW_setupIceCandidateHandler(test, candidateHandler, endHandler) {
    var self = this;
    self._local_ice_candidates = [];
    self._remote_ice_candidates = [];
    self._ice_candidates_to_add = [];

    candidateHandler = candidateHandler || test.iceCandidateHandler.bind(test);
    endHandler = endHandler || test.signalEndOfTrickleIce.bind(test);

    function iceCandidateCallback (anEvent) {
      info(self.label + ": received iceCandidateEvent");
      if (!anEvent.candidate) {
        info(self.label + ": received end of trickle ICE event");
        self.endOfTrickleIce = true;
        endHandler(self.label);
      } else {
        if (self.endOfTrickleIce) {
          ok(false, "received ICE candidate after end of trickle");
        }
        info(self.label + ": iceCandidate = " + JSON.stringify(anEvent.candidate));
        ok(anEvent.candidate.candidate.length > 0, "ICE candidate contains candidate");
        // we don't support SDP MID's yet
        ok(anEvent.candidate.sdpMid.length === 0, "SDP MID has length zero");
        ok(typeof anEvent.candidate.sdpMLineIndex === 'number', "SDP MLine Index needs to exist");
        self._local_ice_candidates.push(anEvent.candidate);
        candidateHandler(self.label, anEvent.candidate);
      }
    }

    self._pc.onicecandidate = iceCandidateCallback;
  },

  /**
   * Counts the amount of audio tracks in a given media constraint.
   *
   * @param constraints
   *        The contraint to be examined.
   */
  countAudioTracksInMediaConstraint : function
    PCW_countAudioTracksInMediaConstraint(constraints) {
    if ((!constraints) || (constraints.length === 0)) {
      return 0;
    }
    var audioTracks = 0;
    for (var i = 0; i < constraints.length; i++) {
      if (constraints[i].audio) {
        audioTracks++;
      }
    }
    return audioTracks;
  },

  /**
   * Checks for audio in given offer options.
   *
   * @param options
   *        The options to be examined.
   */
  audioInOfferOptions : function
    PCW_audioInOfferOptions(options) {
    if (!options) {
      return 0;
    }

    var offerToReceiveAudio = options.offerToReceiveAudio;

    // TODO: Remove tests of old constraint-like RTCOptions soon (Bug 1064223).
    if (options.mandatory && options.mandatory.OfferToReceiveAudio !== undefined) {
      offerToReceiveAudio = options.mandatory.OfferToReceiveAudio;
    } else if (options.optional && options.optional[0] &&
               options.optional[0].OfferToReceiveAudio !== undefined) {
      offerToReceiveAudio = options.optional[0].OfferToReceiveAudio;
    }

    if (offerToReceiveAudio) {
      return 1;
    } else {
      return 0;
    }
  },

  /**
   * Counts the amount of video tracks in a given media constraint.
   *
   * @param constraint
   *        The contraint to be examined.
   */
  countVideoTracksInMediaConstraint : function
    PCW_countVideoTracksInMediaConstraint(constraints) {
    if ((!constraints) || (constraints.length === 0)) {
      return 0;
    }
    var videoTracks = 0;
    for (var i = 0; i < constraints.length; i++) {
      if (constraints[i].video) {
        videoTracks++;
      }
    }
    return videoTracks;
  },

  /**
   * Checks for video in given offer options.
   *
   * @param options
   *        The options to be examined.
   */
  videoInOfferOptions : function
    PCW_videoInOfferOptions(options) {
    if (!options) {
      return 0;
    }

    var offerToReceiveVideo = options.offerToReceiveVideo;

    // TODO: Remove tests of old constraint-like RTCOptions soon (Bug 1064223).
    if (options.mandatory && options.mandatory.OfferToReceiveVideo !== undefined) {
      offerToReceiveVideo = options.mandatory.OfferToReceiveVideo;
    } else if (options.optional && options.optional[0] &&
               options.optional[0].OfferToReceiveVideo !== undefined) {
      offerToReceiveVideo = options.optional[0].OfferToReceiveVideo;
    }

    if (offerToReceiveVideo) {
      return 1;
    } else {
      return 0;
    }
  },

  /*
   * Counts the amount of audio tracks in a given set of streams.
   *
   * @param streams
   *        An array of streams (as returned by getLocalStreams()) to be
   *        examined.
   */
  countAudioTracksInStreams : function PCW_countAudioTracksInStreams(streams) {
    if (!streams || (streams.length === 0)) {
      return 0;
    }
    var audioTracks = 0;
    streams.forEach(function(st) {
      audioTracks += st.getAudioTracks().length;
    });
    return audioTracks;
  },

  /*
   * Counts the amount of video tracks in a given set of streams.
   *
   * @param streams
   *        An array of streams (as returned by getLocalStreams()) to be
   *        examined.
   */
  countVideoTracksInStreams: function PCW_countVideoTracksInStreams(streams) {
    if (!streams || (streams.length === 0)) {
      return 0;
    }
    var videoTracks = 0;
    streams.forEach(function(st) {
      videoTracks += st.getVideoTracks().length;
    });
    return videoTracks;
  },

  /**
   * Checks that we are getting the media tracks we expect.
   *
   * @param {object} constraintsRemote
   *        The media constraints of the local and remote peer connection object
   */
  checkMediaTracks : function PCW_checkMediaTracks(constraintsRemote, onSuccess) {
    var self = this;
    var addStreamTimeout = null;

    function _checkMediaTracks(constraintsRemote, onSuccess) {
      if (addStreamTimeout !== null) {
        clearTimeout(addStreamTimeout);
      }

      var localConstraintAudioTracks =
        self.countAudioTracksInMediaConstraint(self.constraints);
      var localStreams = self._pc.getLocalStreams();
      var localAudioTracks = self.countAudioTracksInStreams(localStreams, false);
      is(localAudioTracks, localConstraintAudioTracks, self + ' has ' +
        localAudioTracks + ' local audio tracks');

      var localConstraintVideoTracks =
        self.countVideoTracksInMediaConstraint(self.constraints);
      var localVideoTracks = self.countVideoTracksInStreams(localStreams, false);
      is(localVideoTracks, localConstraintVideoTracks, self + ' has ' +
        localVideoTracks + ' local video tracks');

      var remoteConstraintAudioTracks =
        self.countAudioTracksInMediaConstraint(constraintsRemote);
      var remoteStreams = self._pc.getRemoteStreams();
      var remoteAudioTracks = self.countAudioTracksInStreams(remoteStreams, false);
      is(remoteAudioTracks, remoteConstraintAudioTracks, self + ' has ' +
        remoteAudioTracks + ' remote audio tracks');

      var remoteConstraintVideoTracks =
        self.countVideoTracksInMediaConstraint(constraintsRemote);
      var remoteVideoTracks = self.countVideoTracksInStreams(remoteStreams, false);
      is(remoteVideoTracks, remoteConstraintVideoTracks, self + ' has ' +
        remoteVideoTracks + ' remote video tracks');

      onSuccess();
    }

    // we have to do this check as the onaddstream never fires if the remote
    // stream has no track at all!
    var expectedRemoteTracks =
      self.countAudioTracksInMediaConstraint(constraintsRemote) +
      self.countVideoTracksInMediaConstraint(constraintsRemote);

    // TODO: remove this once Bugs 998552 and 998546 are closed
    if ((self.onAddStreamFired) || (expectedRemoteTracks == 0)) {
      _checkMediaTracks(constraintsRemote, onSuccess);
    } else {
      info(self + " checkMediaTracks() got called before onAddStream fired");
      // we rely on the outer mochitest timeout to catch the case where
      // onaddstream never fires
      self.addStreamCallbacks.checkMediaTracks = function() {
        _checkMediaTracks(constraintsRemote, onSuccess);
      };
      addStreamTimeout = setTimeout(function () {
        ok(self.onAddStreamFired, self + " checkMediaTracks() timed out waiting for onaddstream event to fire");
        if (!self.onAddStreamFired) {
          onSuccess();
        }
      }, 60000);
    }
  },

  verifySdp : function PCW_verifySdp(desc, expectedType, constraints,
      offerOptions, trickleIceCallback) {
    info("Examining this SessionDescription: " + JSON.stringify(desc));
    info("constraints: " + JSON.stringify(constraints));
    info("offerOptions: " + JSON.stringify(offerOptions));
    ok(desc, "SessionDescription is not null");
    is(desc.type, expectedType, "SessionDescription type is " + expectedType);
    ok(desc.sdp.length > 10, "SessionDescription body length is plausible");
    ok(desc.sdp.contains("a=ice-ufrag"), "ICE username is present in SDP");
    ok(desc.sdp.contains("a=ice-pwd"), "ICE password is present in SDP");
    ok(desc.sdp.contains("a=fingerprint"), "ICE fingerprint is present in SDP");
    //TODO: update this for loopback support bug 1027350
    ok(!desc.sdp.contains(LOOPBACK_ADDR), "loopback interface is absent from SDP");
    if (desc.sdp.contains("a=candidate")) {
      ok(true, "at least one ICE candidate is present in SDP");
      trickleIceCallback(false);
    } else {
      info("No ICE candidate in SDP -> requiring trickle ICE");
      trickleIceCallback(true);
    }
    //TODO: how can we check for absence/presence of m=application?

    //TODO: how to handle media contraints + offer options
    var audioTracks = this.countAudioTracksInMediaConstraint(constraints);
    if (constraints.length === 0) {
      audioTracks = this.audioInOfferOptions(offerOptions);
    }
    info("expected audio tracks: " + audioTracks);
    if (audioTracks == 0) {
      ok(!desc.sdp.contains("m=audio"), "audio m-line is absent from SDP");
    } else {
      ok(desc.sdp.contains("m=audio"), "audio m-line is present in SDP");
      ok(desc.sdp.contains("a=rtpmap:109 opus/48000/2"), "OPUS codec is present in SDP");
      //TODO: ideally the rtcp-mux should be for the m=audio, and not just
      //      anywhere in the SDP (JS SDP parser bug 1045429)
      ok(desc.sdp.contains("a=rtcp-mux"), "RTCP Mux is offered in SDP");

    }

    //TODO: how to handle media contraints + offer options
    var videoTracks = this.countVideoTracksInMediaConstraint(constraints);
    if (constraints.length === 0) {
      videoTracks = this.videoInOfferOptions(offerOptions);
    }
    info("expected video tracks: " + videoTracks);
    if (videoTracks == 0) {
      ok(!desc.sdp.contains("m=video"), "video m-line is absent from SDP");
    } else {
      ok(desc.sdp.contains("m=video"), "video m-line is present in SDP");
      if (this.h264) {
        ok(desc.sdp.contains("a=rtpmap:126 H264/90000"), "H.264 codec is present in SDP");
      } else {
        ok(desc.sdp.contains("a=rtpmap:120 VP8/90000"), "VP8 codec is present in SDP");
      }
      ok(desc.sdp.contains("a=rtcp-mux"), "RTCP Mux is offered in SDP");
    }

  },

  /**
   * Check that media flow is present on all media elements involved in this
   * test by waiting for confirmation that media flow is present.
   *
   * @param {Function} onSuccess the success callback when media flow
   *                             is confirmed on all media elements
   */
  checkMediaFlowPresent : function PCW_checkMediaFlowPresent(onSuccess) {
    var self = this;

    function _checkMediaFlowPresent(index, onSuccess) {
      if(index >= self.mediaCheckers.length) {
        onSuccess();
      } else {
        var mediaChecker = self.mediaCheckers[index];
        mediaChecker.waitForMediaFlow(function() {
          _checkMediaFlowPresent(index + 1, onSuccess);
        });
      }
    }

    _checkMediaFlowPresent(0, onSuccess);
  },

  /**
   * Check that stats are present by checking for known stats.
   *
   * @param {Function} onSuccess the success callback to return stats to
   */
  getStats : function PCW_getStats(selector, onSuccess) {
    var self = this;

    this._pc.getStats(selector, function(stats) {
      info(self + ": Got stats: " + JSON.stringify(stats));
      self._last_stats = stats;
      onSuccess(stats);
    }, generateErrorCallback());
  },

  /**
   * Checks that we are getting the media streams we expect.
   *
   * @param {object} stats
   *        The stats to check from this PeerConnectionWrapper
   */
  checkStats : function PCW_checkStats(stats) {
    function toNum(obj) {
      return obj? obj : 0;
    }
    function numTracks(streams) {
      var n = 0;
      streams.forEach(function(stream) {
          n += stream.getAudioTracks().length + stream.getVideoTracks().length;
        });
      return n;
    }

    const isWinXP = navigator.userAgent.indexOf("Windows NT 5.1") != -1;

    // Use spec way of enumerating stats
    var counters = {};
    for (var key in stats) {
      if (stats.hasOwnProperty(key)) {
        var res = stats[key];
        // validate stats
        ok(res.id == key, "Coherent stats id");
        var nowish = Date.now() + 1000;        // TODO: clock drift observed
        var minimum = this.whenCreated - 1000; // on Windows XP (Bug 979649)
        if (isWinXP) {
          todo(false, "Can't reliably test rtcp timestamps on WinXP (Bug 979649)");
        } else {
          ok(res.timestamp >= minimum,
             "Valid " + (res.isRemote? "rtcp" : "rtp") + " timestamp " +
                 res.timestamp + " >= " + minimum + " (" +
                 (res.timestamp - minimum) + " ms)");
          ok(res.timestamp <= nowish,
             "Valid " + (res.isRemote? "rtcp" : "rtp") + " timestamp " +
                 res.timestamp + " <= " + nowish + " (" +
                 (res.timestamp - nowish) + " ms)");
        }
        if (!res.isRemote) {
          counters[res.type] = toNum(counters[res.type]) + 1;

          switch (res.type) {
            case "inboundrtp":
            case "outboundrtp": {
              // ssrc is a 32 bit number returned as a string by spec
              ok(res.ssrc.length > 0, "Ssrc has length");
              ok(res.ssrc.length < 11, "Ssrc not lengthy");
              ok(!/[^0-9]/.test(res.ssrc), "Ssrc numeric");
              ok(parseInt(res.ssrc) < Math.pow(2,32), "Ssrc within limits");

              if (res.type == "outboundrtp") {
                ok(res.packetsSent !== undefined, "Rtp packetsSent");
                // We assume minimum payload to be 1 byte (guess from RFC 3550)
                ok(res.bytesSent >= res.packetsSent, "Rtp bytesSent");
              } else {
                ok(res.packetsReceived !== undefined, "Rtp packetsReceived");
                ok(res.bytesReceived >= res.packetsReceived, "Rtp bytesReceived");
              }
              if (res.remoteId) {
                var rem = stats[res.remoteId];
                ok(rem.isRemote, "Remote is rtcp");
                ok(rem.remoteId == res.id, "Remote backlink match");
                if(res.type == "outboundrtp") {
                  ok(rem.type == "inboundrtp", "Rtcp is inbound");
                  ok(rem.packetsReceived !== undefined, "Rtcp packetsReceived");
                  ok(rem.packetsReceived <= res.packetsSent, "No more than sent");
                  ok(rem.packetsLost !== undefined, "Rtcp packetsLost");
                  ok(rem.bytesReceived >= rem.packetsReceived, "Rtcp bytesReceived");
                  ok(rem.bytesReceived <= res.bytesSent, "No more than sent bytes");
                  ok(rem.jitter !== undefined, "Rtcp jitter");
                  ok(rem.mozRtt !== undefined, "Rtcp rtt");
                  ok(rem.mozRtt >= 0, "Rtcp rtt " + rem.mozRtt + " >= 0");
                  ok(rem.mozRtt < 60000, "Rtcp rtt " + rem.mozRtt + " < 1 min");
                } else {
                  ok(rem.type == "outboundrtp", "Rtcp is outbound");
                  ok(rem.packetsSent !== undefined, "Rtcp packetsSent");
                  // We may have received more than outdated Rtcp packetsSent
                  ok(rem.bytesSent >= rem.packetsSent, "Rtcp bytesSent");
                }
                ok(rem.ssrc == res.ssrc, "Remote ssrc match");
              } else {
                info("No rtcp info received yet");
              }
            }
            break;
          }
        }
      }
    }

    // Use MapClass way of enumerating stats
    var counters2 = {};
    stats.forEach(function(res) {
        if (!res.isRemote) {
          counters2[res.type] = toNum(counters2[res.type]) + 1;
        }
      });
    is(JSON.stringify(counters), JSON.stringify(counters2),
       "Spec and MapClass variant of RTCStatsReport enumeration agree");
    var nin = numTracks(this._pc.getRemoteStreams());
    var nout = numTracks(this._pc.getLocalStreams());

    // TODO(Bug 957145): Restore stronger inboundrtp test once Bug 948249 is fixed
    //is(toNum(counters["inboundrtp"]), nin, "Have " + nin + " inboundrtp stat(s)");
    ok(toNum(counters.inboundrtp) >= nin, "Have at least " + nin + " inboundrtp stat(s) *");

    is(toNum(counters.outboundrtp), nout, "Have " + nout + " outboundrtp stat(s)");

    var numLocalCandidates  = toNum(counters.localcandidate);
    var numRemoteCandidates = toNum(counters.remotecandidate);
    // If there are no tracks, there will be no stats either.
    if (nin + nout > 0) {
      ok(numLocalCandidates, "Have localcandidate stat(s)");
      ok(numRemoteCandidates, "Have remotecandidate stat(s)");
    } else {
      is(numLocalCandidates, 0, "Have no localcandidate stats");
      is(numRemoteCandidates, 0, "Have no remotecandidate stats");
    }
  },

  /**
   * Property-matching function for finding a certain stat in passed-in stats
   *
   * @param {object} stats
   *        The stats to check from this PeerConnectionWrapper
   * @param {object} props
   *        The properties to look for
   * @returns {boolean} Whether an entry containing all match-props was found.
   */
  hasStat : function PCW_hasStat(stats, props) {
    for (var key in stats) {
      if (stats.hasOwnProperty(key)) {
        var res = stats[key];
        var match = true;
        for (var prop in props) {
          if (res[prop] !== props[prop]) {
            match = false;
            break;
          }
        }
        if (match) {
          return true;
        }
      }
    }
    return false;
  },

  /**
   * Closes the connection
   */
  close : function PCW_close() {
    // It might be that a test has already closed the pc. In those cases
    // we should not fail.
    try {
      this._pc.close();
      info(this + ": Closed connection.");
    }
    catch (e) {
      info(this + ": Failure in closing connection - " + e.message);
    }
  },

  /**
   * Register all events during the setup of the data channel
   *
   * @param {Function} onDataChannelOpened
   *        Callback to execute when the data channel has been opened
   */
  registerDataChannelOpenEvents : function (onDataChannelOpened) {
    info(this + ": Register callbacks for 'ondatachannel' and 'onopen'");

    this.ondatachannel = function (targetChannel) {
      targetChannel.onopen = onDataChannelOpened;
      this.dataChannels.push(targetChannel);
    };
  },

  /**
   * Returns the string representation of the class
   *
   * @returns {String} The string representation
   */
  toString : function PCW_toString() {
    return "PeerConnectionWrapper (" + this.label + ")";
  }
};
