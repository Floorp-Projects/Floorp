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

var makeDefaultCommands = () => {
  return [].concat(commandsPeerConnectionInitial,
                   commandsGetUserMedia,
                   commandsPeerConnectionOfferAnswer);
};

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
  options.commands = options.commands || makeDefaultCommands();
  options.is_local = "is_local" in options ? options.is_local : true;
  options.is_remote = "is_remote" in options ? options.is_remote : true;

  options.h264 = "h264" in options ? options.h264 : false;
  options.bundle = "bundle" in options ? options.bundle : true;
  options.rtcpmux = "rtcpmux" in options ? options.rtcpmux : true;
  options.opus = "opus" in options ? options.opus : true;
  options.ssrc = "ssrc" in options ? options.ssrc : true;

  options.config_local = options.config_local || {}
  options.config_remote = options.config_remote || {}

  if (!options.bundle) {
    // Make sure neither end tries to use bundle-only!
    options.config_local.bundlePolicy = "max-compat";
    options.config_remote.bundlePolicy = "max-compat";
  }

  if (iceServersArray.length) {
    if (!options.turn_disabled_local) {
      options.config_local.iceServers = iceServersArray;
    }
    if (!options.turn_disabled_remote) {
      options.config_remote.iceServers = iceServersArray;
    }
  }
  else if (typeof turnServers !== "undefined") {
    if ((!options.turn_disabled_local) && (turnServers.local)) {
      if (!options.config_local.hasOwnProperty("iceServers")) {
        options.config_local.iceServers = turnServers.local.iceServers;
      }
    }
    if ((!options.turn_disabled_remote) && (turnServers.remote)) {
      if (!options.config_remote.hasOwnProperty("iceServers")) {
        options.config_remote.iceServers = turnServers.remote.iceServers;
      }
    }
  }

  if (options.is_local) {
    this.pcLocal = new PeerConnectionWrapper('pcLocal', options.config_local);
  } else {
    this.pcLocal = null;
  }

  if (options.is_remote) {
    this.pcRemote = new PeerConnectionWrapper('pcRemote', options.config_remote || options.config_local);
  } else {
    this.pcRemote = null;
  }

  options.steeplechase = !options.is_local || !options.is_remote;

  // Create command chain instance and assign default commands
  this.chain = new CommandChain(this, options.commands);

  this.testOptions = options;
}

/** TODO: consider removing this dependency on timeouts */
function timerGuard(p, time, message) {
  return Promise.race([
    p,
    wait(time).then(() => {
      throw new Error('timeout after ' + (time / 1000) + 's: ' + message);
    })
  ]);
}

/**
 * Closes the peer connection if it is active
 */
PeerConnectionTest.prototype.closePC = function() {
  info("Closing peer connections");

  var closeIt = pc => {
    if (!pc || pc.signalingState === "closed") {
      return Promise.resolve();
    }

    var promise = Promise.all([
      new Promise(resolve => {
        pc.onsignalingstatechange = e => {
          is(e.target.signalingState, "closed", "signalingState is closed");
          resolve();
        };
      }),
      Promise.all(pc._pc.getReceivers()
        .filter(receiver => receiver.track.readyState == "live")
        .map(receiver => {
          info("Waiting for track " + receiver.track.id + " (" +
               receiver.track.kind + ") to end.");
          return haveEvent(receiver.track, "ended", wait(50000))
            .then(event => {
              is(event.target, receiver.track, "Event target should be the correct track");
              info(pc + " ended fired for track " + receiver.track.id);
            }, e => e ? Promise.reject(e)
                      : ok(false, "ended never fired for track " +
                                    receiver.track.id));
        }))
    ]);
    pc.close();
    return promise;
  };

  return timerGuard(Promise.all([
    closeIt(this.pcLocal),
    closeIt(this.pcRemote)
  ]), 60000, "failed to close peer connection");
};

/**
 * Close the open data channels, followed by the underlying peer connection
 */
PeerConnectionTest.prototype.close = function() {
  var allChannels = (this.pcLocal || this.pcRemote).dataChannels;
  return timerGuard(
    Promise.all(allChannels.map((channel, i) => this.closeDataChannels(i))),
    120000, "failed to close data channels")
    .then(() => this.closePC());
};

/**
 * Close the specified data channels
 *
 * @param {Number} index
 *        Index of the data channels to close on both sides
 */
PeerConnectionTest.prototype.closeDataChannels = function(index) {
  info("closeDataChannels called with index: " + index);
  var localChannel = null;
  if (this.pcLocal) {
    localChannel = this.pcLocal.dataChannels[index];
  }
  var remoteChannel = null;
  if (this.pcRemote) {
    remoteChannel = this.pcRemote.dataChannels[index];
  }

  // We need to setup all the close listeners before calling close
  var setupClosePromise = channel => {
    if (!channel) {
      return Promise.resolve();
    }
    return new Promise(resolve => {
      channel.onclose = () => {
        is(channel.readyState, "closed", name + " channel " + index + " closed");
        resolve();
      };
    });
  };

  // make sure to setup close listeners before triggering any actions
  var allClosed = Promise.all([
    setupClosePromise(localChannel),
    setupClosePromise(remoteChannel)
  ]);
  var complete = timerGuard(allClosed, 120000, "failed to close data channel pair");

  // triggering close on one side should suffice
  if (remoteChannel) {
    remoteChannel.close();
  } else if (localChannel) {
    localChannel.close();
  }

  return complete;
};

/**
 * Send data (message or blob) to the other peer
 *
 * @param {String|Blob} data
 *        Data to send to the other peer. For Blobs the MIME type will be lost.
 * @param {Object} [options={ }]
 *        Options to specify the data channels to be used
 * @param {DataChannelWrapper} [options.sourceChannel=pcLocal.dataChannels[length - 1]]
 *        Data channel to use for sending the message
 * @param {DataChannelWrapper} [options.targetChannel=pcRemote.dataChannels[length - 1]]
 *        Data channel to use for receiving the message
 */
PeerConnectionTest.prototype.send = async function(data, options) {
  options = options || { };
  const source = options.sourceChannel ||
           this.pcLocal.dataChannels[this.pcLocal.dataChannels.length - 1];
  const target = options.targetChannel ||
           this.pcRemote.dataChannels[this.pcRemote.dataChannels.length - 1];
  source.bufferedAmountLowThreshold = options.bufferedAmountLowThreshold || 0;

  const getSizeInBytes = d => {
    if (d instanceof Blob) {
      return d.size;
    } else if (d instanceof ArrayBuffer) {
      return d.byteLength;
    } else if (d instanceof String || typeof d === "string") {
      return (new TextEncoder('utf-8')).encode(d).length;
    } else {
      ok(false);
    }
  };

  const expectedSizeInBytes = getSizeInBytes(data);
  const bufferedAmount = source.bufferedAmount;

  source.send(data);
  is(source.bufferedAmount, expectedSizeInBytes + bufferedAmount,
    `Buffered amount should be ${expectedSizeInBytes}`);

  await new Promise(resolve => source.onbufferedamountlow = resolve);

  return new Promise(resolve => {
    // Register event handler for the target channel
      target.onmessage = e => {
        is(getSizeInBytes(e.data), expectedSizeInBytes,
          `Expected to receive the same number of bytes as we sent (${expectedSizeInBytes})`);
	resolve({ channel: target, data: e.data });
    };
  });;
};

/**
 * Create a data channel
 *
 * @param {Dict} options
 *        Options for the data channel (see nsIPeerConnection)
 */
PeerConnectionTest.prototype.createDataChannel = function(options) {
  var remotePromise;
  if (!options.negotiated) {
    this.pcRemote.expectDataChannel("pcRemote expected data channel");
    remotePromise = this.pcRemote.nextDataChannel;
  }

  // Create the datachannel
  var localChannel = this.pcLocal.createDataChannel(options)
  var localPromise = localChannel.opened;

  if (options.negotiated) {
    remotePromise = localPromise.then(localChannel => {
      // externally negotiated - we need to open from both ends
      options.id = options.id || channel.id;  // allow for no id on options
      var remoteChannel = this.pcRemote.createDataChannel(options);
      return remoteChannel.opened;
    });
  }

  // pcRemote.observedNegotiationNeeded might be undefined if
  // !options.negotiated, which means we just wait on pcLocal
  return Promise.all([this.pcLocal.observedNegotiationNeeded,
                      this.pcRemote.observedNegotiationNeeded]).then(() => {
    return Promise.all([localPromise, remotePromise]).then(result => {
      return { local: result[0], remote: result[1] };
    });
  });
};

/**
 * Creates an answer for the specified peer connection instance
 * and automatically handles the failure case.
 *
 * @param {PeerConnectionWrapper} peer
 *        The peer connection wrapper to run the command on
 */
PeerConnectionTest.prototype.createAnswer = function(peer) {
  return peer.createAnswer().then(answer => {
    // make a copy so this does not get updated with ICE candidates
    this.originalAnswer = new RTCSessionDescription(JSON.parse(JSON.stringify(answer)));
    return answer;
  });
};

/**
 * Creates an offer for the specified peer connection instance
 * and automatically handles the failure case.
 *
 * @param {PeerConnectionWrapper} peer
 *        The peer connection wrapper to run the command on
 */
PeerConnectionTest.prototype.createOffer = function(peer) {
  return peer.createOffer().then(offer => {
    // make a copy so this does not get updated with ICE candidates
    this.originalOffer = new RTCSessionDescription(JSON.parse(JSON.stringify(offer)));
    return offer;
  });
};

/**
 * Sets the local description for the specified peer connection instance
 * and automatically handles the failure case.
 *
 * @param {PeerConnectionWrapper} peer
          The peer connection wrapper to run the command on
 * @param {RTCSessionDescriptionInit} desc
 *        Session description for the local description request
 */
PeerConnectionTest.prototype.setLocalDescription =
function(peer, desc, stateExpected) {
  var eventFired = new Promise(resolve => {
    peer.onsignalingstatechange = e => {
      info(peer + ": 'signalingstatechange' event received");
      var state = e.target.signalingState;
      if (stateExpected === state) {
        peer.setLocalDescStableEventDate = new Date();
        resolve();
      } else {
        ok(false, "This event has either already fired or there has been a " +
           "mismatch between event received " + state +
           " and event expected " + stateExpected);
      }
    };
  });

  var stateChanged = peer.setLocalDescription(desc).then(() => {
    peer.setLocalDescDate = new Date();
  });

  peer.endOfTrickleSdp = peer.endOfTrickleIce.then(() => {
    if (this.testOptions.steeplechase) {
      send_message({"type": "end_of_trickle_ice"});
    }
    return peer._pc.localDescription;
  })
  .catch(e => ok(false, "Sending EOC message failed: " + e));

  return Promise.all([eventFired, stateChanged]);
};

/**
 * Sets the media constraints for both peer connection instances.
 *
 * @param {object} constraintsLocal
 *        Media constrains for the local peer connection instance
 * @param constraintsRemote
 */
PeerConnectionTest.prototype.setMediaConstraints =
function(constraintsLocal, constraintsRemote) {
  if (this.pcLocal) {
    this.pcLocal.constraints = constraintsLocal;
  }
  if (this.pcRemote) {
    this.pcRemote.constraints = constraintsRemote;
  }
};

/**
 * Sets the media options used on a createOffer call in the test.
 *
 * @param {object} options the media constraints to use on createOffer
 */
PeerConnectionTest.prototype.setOfferOptions = function(options) {
  if (this.pcLocal) {
    this.pcLocal.offerOptions = options;
  }
};

/**
 * Sets the remote description for the specified peer connection instance
 * and automatically handles the failure case.
 *
 * @param {PeerConnectionWrapper} peer
          The peer connection wrapper to run the command on
 * @param {RTCSessionDescriptionInit} desc
 *        Session description for the remote description request
 */
PeerConnectionTest.prototype.setRemoteDescription =
function(peer, desc, stateExpected) {
  var eventFired = new Promise(resolve => {
    peer.onsignalingstatechange = e => {
      info(peer + ": 'signalingstatechange' event received");
      var state = e.target.signalingState;
      if (stateExpected === state) {
        peer.setRemoteDescStableEventDate = new Date();
        resolve();
      } else {
        ok(false, "This event has either already fired or there has been a " +
           "mismatch between event received " + state +
           " and event expected " + stateExpected);
      }
    };
  });

  var stateChanged = peer.setRemoteDescription(desc).then(() => {
    peer.setRemoteDescDate = new Date();
    peer.checkMediaTracks();
  });

  return Promise.all([eventFired, stateChanged]);
};

/**
 * Adds and removes steps to/from the execution chain based on the configured
 * testOptions.
 */
PeerConnectionTest.prototype.updateChainSteps = function() {
  if (this.testOptions.h264) {
    this.chain.insertAfterEach(
      'PC_LOCAL_CREATE_OFFER',
      [PC_LOCAL_REMOVE_ALL_BUT_H264_FROM_OFFER]);
  }
  if (!this.testOptions.bundle) {
    this.chain.insertAfterEach(
      'PC_LOCAL_CREATE_OFFER',
      [PC_LOCAL_REMOVE_BUNDLE_FROM_OFFER]);
  }
  if (!this.testOptions.rtcpmux) {
    this.chain.insertAfterEach(
      'PC_LOCAL_CREATE_OFFER',
      [PC_LOCAL_REMOVE_RTCPMUX_FROM_OFFER]);
  }
  if (!this.testOptions.ssrc) {
    this.chain.insertAfterEach(
      'PC_LOCAL_CREATE_OFFER',
      [PC_LOCAL_REMOVE_SSRC_FROM_OFFER]);
    this.chain.insertAfterEach(
      'PC_REMOTE_CREATE_ANSWER',
      [PC_REMOTE_REMOVE_SSRC_FROM_ANSWER]);
  }
  if (!this.testOptions.is_local) {
    this.chain.filterOut(/^PC_LOCAL/);
  }
  if (!this.testOptions.is_remote) {
    this.chain.filterOut(/^PC_REMOTE/);
  }
};

/**
 * Start running the tests as assigned to the command chain.
 */
PeerConnectionTest.prototype.run = function() {
  /* We have to modify the chain here to allow tests which modify the default
   * test chain instantiating a PeerConnectionTest() */
  this.updateChainSteps();
  var finished = () => {
    if (window.SimpleTest) {
      networkTestFinished();
    } else {
      finish();
    }
  };
  return this.chain.execute()
    .then(() => this.close())
    .catch(e =>
      ok(false, 'Error in test execution: ' + e +
         ((typeof e.stack === 'string') ?
          (' ' + e.stack.split('\n').join(' ... ')) : '')))
    .then(() => finished())
    .catch(e =>
      ok(false, "Error in finished()"));
};

/**
 * Routes ice candidates from one PCW to the other PCW
 */
PeerConnectionTest.prototype.iceCandidateHandler = function(caller, candidate) {
  info("Received: " + JSON.stringify(candidate) + " from " + caller);

  var target = null;
  if (caller.includes("pcLocal")) {
    if (this.pcRemote) {
      target = this.pcRemote;
    }
  } else if (caller.includes("pcRemote")) {
    if (this.pcLocal) {
      target = this.pcLocal;
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
PeerConnectionTest.prototype.setupSignalingClient = function() {
  this.signalingMessageQueue = [];
  this.signalingCallbacks = {};
  this.signalingLoopRun = true;

  var queueMessage = message => {
    info("Received signaling message: " + JSON.stringify(message));
    var fired = false;
    Object.keys(this.signalingCallbacks).forEach(name => {
      if (name === message.type) {
        info("Invoking callback for message type: " + name);
        this.signalingCallbacks[name](message);
        fired = true;
      }
    });
    if (!fired) {
      this.signalingMessageQueue.push(message);
      info("signalingMessageQueue.length: " + this.signalingMessageQueue.length);
    }
    if (this.signalingLoopRun) {
      wait_for_message().then(queueMessage);
    } else {
      info("Exiting signaling message event loop");
    }
  };
  wait_for_message().then(queueMessage);
}

/**
 * Sets a flag to stop reading further messages from the chat room.
 */
PeerConnectionTest.prototype.signalingMessagesFinished = function() {
  this.signalingLoopRun = false;
}

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
PeerConnectionTest.prototype.registerSignalingCallback = function(messageType, onMessage) {
  this.signalingCallbacks[messageType] = onMessage;
};

/**
 * Searches the message queue for the first message of a given type
 * and invokes the given callback function, or registers the callback
 * function for future messages if the queue contains no such message.
 *
 * @param {string} messageType
 *        The type of message to search and register for.
 */
PeerConnectionTest.prototype.getSignalingMessage = function(messageType) {
    var i = this.signalingMessageQueue.findIndex(m => m.type === messageType);
  if (i >= 0) {
    info("invoking callback on message " + i + " from message queue, for message type:" + messageType);
    return Promise.resolve(this.signalingMessageQueue.splice(i, 1)[0]);
  }
  return new Promise(resolve =>
                     this.registerSignalingCallback(messageType, resolve));
};


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
  createOneShotEventWrapper(this, this._channel, 'close');
  createOneShotEventWrapper(this, this._channel, 'error');
  createOneShotEventWrapper(this, this._channel, 'message');
  createOneShotEventWrapper(this, this._channel, 'bufferedamountlow');

  this.opened = timerGuard(new Promise(resolve => {
    this._channel.onopen = () => {
      this._channel.onopen = unexpectedEvent(this, 'onopen');
      is(this.readyState, "open", "data channel is 'open' after 'onopen'");
      resolve(this);
    };
  }), 180000, "channel didn't open in time");
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

  /**
   * Returns the ordered attribute of the data channel
   *
   * @returns {bool} The ordered attribute
   */
  get ordered() {
    return this._channel.ordered;
  },

  /**
   * Returns the maxPacketLifeTime attribute of the data channel
   *
   * @returns {number} The maxPacketLifeTime attribute
   */
  get maxPacketLifeTime() {
    return this._channel.maxPacketLifeTime;
  },

  /**
   * Returns the maxRetransmits attribute of the data channel
   *
   * @returns {number} The maxRetransmits attribute
   */
  get maxRetransmits() {
    return this._channel.maxRetransmits;
  },

  /**
   * Returns the readyState bit of the data channel
   *
   * @returns {String} The state of the channel
   */
  get readyState() {
    return this._channel.readyState;
  },

  get bufferedAmount() {
    return this._channel.bufferedAmount;
  },

  /**
   * Sets the bufferlowthreshold of the channel
   *
   * @param {integer} amoutn
   *        The new threshold for the chanel
   */
  set bufferedAmountLowThreshold(amount) {
    this._channel.bufferedAmountLowThreshold = amount;
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
  send: function(data) {
    info(this + ": Sending data '" + data + "'");
    this._channel.send(data);
  },

  /**
   * Returns the string representation of the class
   *
   * @returns {String} The string representation
   */
  toString: function() {
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
function PeerConnectionWrapper(label, configuration) {
  this.configuration = configuration;
  if (configuration && configuration.label_suffix) {
    label = label + "_" + configuration.label_suffix;
  }
  this.label = label;
  this.whenCreated = Date.now();

  this.constraints = [ ];
  this.offerOptions = {};

  this.dataChannels = [ ];

  this._local_ice_candidates = [];
  this._remote_ice_candidates = [];
  this.localRequiresTrickleIce = false;
  this.remoteRequiresTrickleIce = false;
  this.localMediaElements = [];
  this.remoteMediaElements = [];
  this.audioElementsOnly = false;

  this._sendStreams = [];

  this.expectedLocalTrackInfo = [];
  this.remoteStreamsByTrackId = new Map();

  this.disableRtpCountChecking = false;

  this.iceConnectedResolve;
  this.iceConnectedReject;
  this.iceConnected = new Promise((resolve, reject) => {
    this.iceConnectedResolve = resolve;
    this.iceConnectedReject = reject;
  });
  this.iceCheckingRestartExpected = false;
  this.iceCheckingIceRollbackExpected = false;

  info("Creating " + this);
  this._pc = new RTCPeerConnection(this.configuration);

  /**
   * Setup callback handlers
   */
  // This allows test to register their own callbacks for ICE connection state changes
  this.ice_connection_callbacks = {};

  this._pc.oniceconnectionstatechange = e => {
    isnot(typeof this._pc.iceConnectionState, "undefined",
          "iceConnectionState should not be undefined");
    var iceState = this._pc.iceConnectionState;
    info(this + ": oniceconnectionstatechange fired, new state is: " + iceState);
    Object.keys(this.ice_connection_callbacks).forEach(name => {
      this.ice_connection_callbacks[name]();
    });
    if (iceState === "connected") {
      this.iceConnectedResolve();
    } else if (iceState === "failed") {
      this.iceConnectedReject(new Error("ICE failed"));
    }
  };

  this._pc.onicegatheringstatechange = e => {
    isnot(typeof this._pc.iceGatheringState, "undefined",
          "iceGetheringState should not be undefined");
    var gatheringState = this._pc.iceGatheringState;
    info(this + ": onicegatheringstatechange fired, new state is: " + gatheringState);
  };

  createOneShotEventWrapper(this, this._pc, 'datachannel');
  this._pc.addEventListener('datachannel', e => {
    var wrapper = new DataChannelWrapper(e.channel, this);
    this.dataChannels.push(wrapper);
  });

  createOneShotEventWrapper(this, this._pc, 'signalingstatechange');
  createOneShotEventWrapper(this, this._pc, 'negotiationneeded');
}

PeerConnectionWrapper.prototype = {
  /**
   * Returns the senders
   *
   * @returns {sequence<RTCRtpSender>} the senders
   */
  getSenders: function() {
    return this._pc.getSenders();
  },

  /**
   * Returns the getters
   *
   * @returns {sequence<RTCRtpReceiver>} the receivers
   */
  getReceivers: function() {
    return this._pc.getReceivers();
  },

  /**
   * Returns the local description.
   *
   * @returns {object} The local description
   */
  get localDescription() {
    return this._pc.localDescription;
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

  setIdentityProvider: function(provider, options) {
    this._pc.setIdentityProvider(provider, options);
  },

  elementPrefix : direction =>
  {
    return [this.label, direction].join('_');
  },

  getMediaElementForTrack : function (track, direction)
  {
    var prefix = this.elementPrefix(direction);
    return getMediaElementForTrack(track, prefix);
  },

  createMediaElementForTrack : function(track, direction)
  {
    var prefix = this.elementPrefix(direction);
    return createMediaElementForTrack(track, prefix);
  },

  ensureMediaElement : function(track, direction) {
    var prefix = this.elementPrefix(direction);
    var element = this.getMediaElementForTrack(track, direction);
    if (!element) {
      element = this.createMediaElementForTrack(track, direction);
      if (direction == "local") {
        this.localMediaElements.push(element);
      } else if (direction == "remote") {
        this.remoteMediaElements.push(element);
      }
    }

    // We do this regardless, because sometimes we end up with a new stream with
    // an old id (ie; the rollback tests cause the same stream to be added
    // twice)
    element.srcObject = new MediaStream([track]);
    element.play();
  },

  addSendStream : function(stream)
  {
    // The PeerConnection will not necessarily know about this stream
    // automatically, because replaceTrack is not told about any streams the
    // new track might be associated with. Only content really knows.
    this._sendStreams.push(stream);
  },

  getStreamForSendTrack : function(track)
  {
    return this._sendStreams.find(str => str.getTrackById(track.id));
  },

  getStreamForRecvTrack : function(track)
  {
    return this._pc.getRemoteStreams().find(s => !!s.getTrackById(track.id));
  },

  /**
   * Attaches a local track to this RTCPeerConnection using
   * RTCPeerConnection.addTrack().
   *
   * Also creates a media element playing a MediaStream containing all
   * tracks that have been added to `stream` using `attachLocalTrack()`.
   *
   * @param {MediaStreamTrack} track
   *        MediaStreamTrack to handle
   * @param {MediaStream} stream
   *        MediaStream to use as container for `track` on remote side
   */
  attachLocalTrack : function(track, stream) {
    info("Got a local " + track.kind + " track");

    this.expectNegotiationNeeded();
    var sender = this._pc.addTrack(track, stream);
    is(sender.track, track, "addTrack returns sender");
    is(this._pc.getSenders().pop(), sender, "Sender should be the last element in getSenders()");

    ok(track.id, "track has id");
    ok(track.kind, "track has kind");
    ok(stream.id, "stream has id");
    this.expectedLocalTrackInfo.push({track, sender, streamId: stream.id});
    this.addSendStream(stream);

    // This will create one media element per track, which might not be how
    // we set up things with the RTCPeerConnection. It's the only way
    // we can ensure all sent tracks are flowing however.
    this.ensureMediaElement(track, "local");

    return this.observedNegotiationNeeded;
  },

  /**
   * Callback when we get local media. Also an appropriate HTML media element
   * will be created and added to the content node.
   *
   * @param {MediaStream} stream
   *        Media stream to handle
   */
  attachLocalStream : function(stream, useAddTransceiver) {
    info("Got local media stream: (" + stream.id + ")");

    this.expectNegotiationNeeded();
    if (useAddTransceiver) {
      info("Using addTransceiver (on PC).");
      stream.getTracks().forEach(track => {
        var transceiver = this._pc.addTransceiver(track, {streams: [stream]});
        is(transceiver.sender.track, track, "addTransceiver returns sender");
      });
    }
    // In order to test both the addStream and addTrack APIs, we do half one
    // way, half the other, at random.
    else if (Math.random() < 0.5) {
      info("Using addStream.");
      this._pc.addStream(stream);
      ok(this._pc.getSenders().find(sender => sender.track == stream.getTracks()[0]),
         "addStream returns sender");
    } else {
      info("Using addTrack (on PC).");
      stream.getTracks().forEach(track => {
        var sender = this._pc.addTrack(track, stream);
        is(sender.track, track, "addTrack returns sender");
      });
    }

    this.addSendStream(stream);

    stream.getTracks().forEach(track => {
      ok(track.id, "track has id");
      ok(track.kind, "track has kind");
      const sender = this._pc.getSenders().find(s => s.track == track);
      ok(sender, "track has a sender");
      this.expectedLocalTrackInfo.push({track, sender, streamId: stream.id});
      this.ensureMediaElement(track, "local");
    });

    return this.observedNegotiationNeeded;
  },

  removeSender : function(index) {
    var sender = this._pc.getSenders()[index];
    this.expectedLocalTrackInfo =
      this.expectedLocalTrackInfo.filter(i => i.sender != sender);
    this.expectNegotiationNeeded();
    this._pc.removeTrack(sender);
    return this.observedNegotiationNeeded;
  },

  senderReplaceTrack : function(sender, withTrack, stream) {
    const info = this.expectedLocalTrackInfo.find(i => i.sender == sender);
    if (!info) {
      return; // replaceTrack on a null track, probably
    }
    info.track = withTrack;
    this.addSendStream(stream);
    this.ensureMediaElement(withTrack, 'local');
    return sender.replaceTrack(withTrack);
  },

  getUserMedia : async function(constraints) {
    var stream = await getUserMedia(constraints);
    if (constraints.audio) {
      stream.getAudioTracks().forEach(track => {
        info(this + " gUM local stream " + stream.id +
          " with audio track " + track.id);
      });
    }
    if (constraints.video) {
      stream.getVideoTracks().forEach(track => {
        info(this + " gUM local stream " + stream.id +
          " with video track " + track.id);
      });
    }
    return stream;
  },

  /**
   * Requests all the media streams as specified in the constrains property.
   *
   * @param {array} constraintsList
   *        Array of constraints for GUM calls
   */
  getAllUserMedia : function(constraintsList) {
    if (constraintsList.length === 0) {
      info("Skipping GUM: no UserMedia requested");
      return Promise.resolve();
    }

    info("Get " + constraintsList.length + " local streams");
    return Promise.all(
      constraintsList.map(constraints => this.getUserMedia(constraints))
    );
  },

  getAllUserMediaAndAddStreams : async function(constraintsList) {
    var streams = await this.getAllUserMedia(constraintsList);
    if (!streams) {
      return;
    }
    return Promise.all(streams.map(stream => this.attachLocalStream(stream)));
  },

  getAllUserMediaAndAddTransceivers : async function(constraintsList) {
    var streams = await this.getAllUserMedia(constraintsList);
    if (!streams) {
      return;
    }
    return Promise.all(streams.map(stream => this.attachLocalStream(stream, true)));
  },

  /**
   * Create a new data channel instance.  Also creates a promise called
   * `this.nextDataChannel` that resolves when the next data channel arrives.
   */
  expectDataChannel: function(message) {
    this.nextDataChannel = new Promise(resolve => {
      this.ondatachannel = e => {
        ok(e.channel, message);
        is(e.channel.readyState, "connecting", "data channel in 'connecting after 'ondatachannel''");
        resolve(e.channel);
      };
    });
  },

  /**
   * Create a new data channel instance
   *
   * @param {Object} options
   *        Options which get forwarded to nsIPeerConnection.createDataChannel
   * @returns {DataChannelWrapper} The created data channel
   */
  createDataChannel : function(options) {
    var label = 'channel_' + this.dataChannels.length;
    info(this + ": Create data channel '" + label);

    if (!this.dataChannels.length) {
      this.expectNegotiationNeeded();
    }
    var channel = this._pc.createDataChannel(label, options);
    if (!this.dataChannels.length) {
      is(channel.readyState, "connecting", "initial readyState is 'connecting'");
    } else {
      // TODO: Bug 1441723 Update firefox to spec.
      is(channel.readyState, "open", "subsequent readyState is 'open'");
    }
    var wrapper = new DataChannelWrapper(channel, this);
    this.dataChannels.push(wrapper);
    return wrapper;
  },

  /**
   * Creates an offer and automatically handles the failure case.
   */
  createOffer : function() {
    return this._pc.createOffer(this.offerOptions).then(offer => {
      info("Got offer: " + JSON.stringify(offer));
      // note: this might get updated through ICE gathering
      this._latest_offer = offer;
      return offer;
    });
  },

  /**
   * Creates an answer and automatically handles the failure case.
   */
  createAnswer : function() {
    return this._pc.createAnswer().then(answer => {
      info(this + ": Got answer: " + JSON.stringify(answer));
      this._last_answer = answer;
      return answer;
    });
  },

  /**
   * Sets the local description and automatically handles the failure case.
   *
   * @param {object} desc
   *        RTCSessionDescriptionInit for the local description request
   */
  setLocalDescription : function(desc) {
    this.observedNegotiationNeeded = undefined;
    return this._pc.setLocalDescription(desc).then(() => {
      info(this + ": Successfully set the local description");
    });
  },

  /**
   * Tries to set the local description and expect failure. Automatically
   * causes the test case to fail if the call succeeds.
   *
   * @param {object} desc
   *        RTCSessionDescriptionInit for the local description request
   * @returns {Promise}
   *        A promise that resolves to the expected error
   */
  setLocalDescriptionAndFail : function(desc) {
    return this._pc.setLocalDescription(desc).then(
      generateErrorCallback("setLocalDescription should have failed."),
      err => {
        info(this + ": As expected, failed to set the local description");
        return err;
      });
  },

  /**
   * Sets the remote description and automatically handles the failure case.
   *
   * @param {object} desc
   *        RTCSessionDescriptionInit for the remote description request
   */
  setRemoteDescription : function(desc) {
    this.observedNegotiationNeeded = undefined;
    return this._pc.setRemoteDescription(desc).then(() => {
      info(this + ": Successfully set remote description");
      if (desc.type == "rollback") {
        this.holdIceCandidates = new Promise(r => this.releaseIceCandidates = r);

      } else {
        this.releaseIceCandidates();
      }
    });
  },

  /**
   * Tries to set the remote description and expect failure. Automatically
   * causes the test case to fail if the call succeeds.
   *
   * @param {object} desc
   *        RTCSessionDescriptionInit for the remote description request
   * @returns {Promise}
   *        a promise that resolve to the returned error
   */
  setRemoteDescriptionAndFail : function(desc) {
    return this._pc.setRemoteDescription(desc).then(
      generateErrorCallback("setRemoteDescription should have failed."),
      err => {
        info(this + ": As expected, failed to set the remote description");
        return err;
    });
  },

  /**
   * Registers a callback for the signaling state change and
   * appends the new state to an array for logging it later.
   */
  logSignalingState: function() {
    this.signalingStateLog = [this._pc.signalingState];
    this._pc.addEventListener('signalingstatechange', e => {
      var newstate = this._pc.signalingState;
      var oldstate = this.signalingStateLog[this.signalingStateLog.length - 1]
      if (Object.keys(signalingStateTransitions).includes(oldstate)) {
        ok(signalingStateTransitions[oldstate].includes(newstate), this + ": legal signaling state transition from " + oldstate + " to " + newstate);
      } else {
        ok(false, this + ": old signaling state " + oldstate + " missing in signaling transition array");
      }
      this.signalingStateLog.push(newstate);
    });
  },

  isTrackOnPC: function(track) {
    return !!this.getStreamForRecvTrack(track);
  },

  allExpectedTracksAreObserved: function(expected, observed) {
    return Object.keys(expected).every(trackId => observed[trackId]);
  },

  setupStreamEventHandlers: function(stream) {
    const myTrackIds = new Set(stream.getTracks().map(t => t.id));

    stream.addEventListener('addtrack', ({track}) => {
      ok(!myTrackIds.has(track.id), "Duplicate addtrack callback: "
        + `stream id=${stream.id} track id=${track.id}`);
      myTrackIds.add(track.id);
      // addtrack events happen before track events, so the track callback hasn't
      // heard about this yet.
      let streams = this.remoteStreamsByTrackId.get(track.id);
      ok(!streams || !streams.has(stream.id),
        `In addtrack for stream id=${stream.id}` +
        `there should not have been a track event for track id=${track.id} ` +
        " containing this stream yet.");
      ok(stream.getTracks().includes(track), "In addtrack, stream id=" +
        `${stream.id} should already contain track id=${track.id}`);
    });

    stream.addEventListener('removetrack', ({track}) => {
      ok(myTrackIds.has(track.id), "Duplicate removetrack callback: "
        + `stream id=${stream.id} track id=${track.id}`);
      myTrackIds.delete(track.id);
      // Also remove the association from remoteStreamsByTrackId
      const streams = this.remoteStreamsByTrackId.get(track.id);
      ok(streams, `In removetrack for stream id=${stream.id}, track id=` +
        `${track.id} should have had a track callback for the stream.`);
      streams.delete(stream.id);
      ok(!stream.getTracks().includes(track), "In removetrack, stream id=" +
        `${stream.id} should not contain track id=${track.id}`);
    });
  },

  setupTrackEventHandler: function() {
    this._pc.addEventListener('track', ({track, streams}) => {
      info(`${this}: 'ontrack' event fired for ${track.id}`);
      ok(this.isTrackOnPC(track), `Found track ${track.id}`);

      let gratuitousEvent = true;
      let streamsContainingTrack = this.remoteStreamsByTrackId.get(track.id);
      if (!streamsContainingTrack) {
        gratuitousEvent = false; // Told us about a new track
        this.remoteStreamsByTrackId.set(track.id, new Set());
        streamsContainingTrack = this.remoteStreamsByTrackId.get(track.id);
      }

      for (const stream of streams) {
        ok(stream.getTracks().includes(track),
          `In track event, track id=${track.id}` +
          ` should already be in stream id=${stream.id}`);

        if (!streamsContainingTrack.has(stream.id)) {
          gratuitousEvent = false; // Told us about a new stream
          streamsContainingTrack.add(stream.id);
          this.setupStreamEventHandlers(stream);
        }
      }

      ok(!gratuitousEvent, "track event told us something new")

      // So far, we've verified consistency between the current state of the
      // streams, addtrack/removetrack events on the streams, and track events
      // on the peerconnection. We have also verified that we have not gotten
      // any gratuitous events. We have not done anything to verify that the
      // current state of affairs matches what we were expecting it to.

      this.ensureMediaElement(track, 'remote');
    });
  },

  /**
   * Either adds a given ICE candidate right away or stores it to be added
   * later, depending on the state of the PeerConnection.
   *
   * @param {object} candidate
   *        The RTCIceCandidate to be added or stored
   */
  storeOrAddIceCandidate : function(candidate) {
    this._remote_ice_candidates.push(candidate);
    if (this.signalingState === 'closed') {
      info("Received ICE candidate for closed PeerConnection - discarding");
      return;
    }
    this.holdIceCandidates.then(() => {
      info(this + ": adding ICE candidate " + JSON.stringify(candidate));
      return this._pc.addIceCandidate(candidate);
    })
    .then(() => ok(true, this + " successfully added an ICE candidate"))
    .catch(e =>
      // The onicecandidate callback runs independent of the test steps
      // and therefore errors thrown from in there don't get caught by the
      // race of the Promises around our test steps.
      // Note: as long as we are queuing ICE candidates until the success
      //       of sRD() this should never ever happen.
      ok(false, this + " adding ICE candidate failed with: " + e.message)
    );
  },

  /**
   * Registers a callback for the ICE connection state change and
   * appends the new state to an array for logging it later.
   */
  logIceConnectionState: function() {
    this.iceConnectionLog = [this._pc.iceConnectionState];
    this.ice_connection_callbacks.logIceStatus = () => {
      var newstate = this._pc.iceConnectionState;
      var oldstate = this.iceConnectionLog[this.iceConnectionLog.length - 1]
      if (Object.keys(iceStateTransitions).includes(oldstate)) {
        if (this.iceCheckingRestartExpected) {
          is(newstate, "checking",
             "iceconnectionstate event \'" + newstate +
             "\' matches expected state \'checking\'");
          this.iceCheckingRestartExpected = false;
        } else if (this.iceCheckingIceRollbackExpected) {
          is(newstate, "connected",
             "iceconnectionstate event \'" + newstate +
             "\' matches expected state \'connected\'");
          this.iceCheckingIceRollbackExpected = false;
        } else {
          ok(iceStateTransitions[oldstate].includes(newstate), this + ": legal ICE state transition from " + oldstate + " to " + newstate);
        }
      } else {
        ok(false, this + ": old ICE state " + oldstate + " missing in ICE transition array");
      }
      this.iceConnectionLog.push(newstate);
    };
  },

  /**
   * Resets the ICE connected Promise and allows ICE connection state monitoring
   * to go backwards to 'checking'.
   */
  expectIceChecking : function() {
    this.iceCheckingRestartExpected = true;
    this.iceConnected = new Promise((resolve, reject) => {
      this.iceConnectedResolve = resolve;
      this.iceConnectedReject = reject;
    });
  },

  /**
   * Waits for ICE to either connect or fail.
   *
   * @returns {Promise}
   *          resolves when connected, rejects on failure
   */
  waitForIceConnected : function() {
    return this.iceConnected;
  },

  /**
   * Setup a onicecandidate handler
   *
   * @param {object} test
   *        A PeerConnectionTest object to which the ice candidates gets
   *        forwarded.
   */
  setupIceCandidateHandler : function(test, candidateHandler) {
    candidateHandler = candidateHandler || test.iceCandidateHandler.bind(test);

    var resolveEndOfTrickle;
    this.endOfTrickleIce = new Promise(r => resolveEndOfTrickle = r);
    this.holdIceCandidates = new Promise(r => this.releaseIceCandidates = r);

    this._pc.onicecandidate = anEvent => {
      if (!anEvent.candidate) {
        this._pc.onicecandidate = () =>
          ok(false, this.label + " received ICE candidate after end of trickle");
        info(this.label + ": received end of trickle ICE event");
        ok(this._pc.iceGatheringState === 'complete',
           "ICE gathering state has reached complete");
        resolveEndOfTrickle(this.label);
        return;
      }

      info(this.label + ": iceCandidate = " + JSON.stringify(anEvent.candidate));
      ok(anEvent.candidate.sdpMid.length > 0, "SDP mid not empty");
      ok(anEvent.candidate.usernameFragment.length > 0, "usernameFragment not empty");

      // only check the m-section for the updated default addr that corresponds
      // with this candidate.
      var mSections = this.localDescription.sdp.split("\r\nm=");
      sdputils.checkSdpCLineNotDefault(
        mSections[anEvent.candidate.sdpMLineIndex+1], this.label
      );

      ok(typeof anEvent.candidate.sdpMLineIndex === 'number', "SDP MLine Index needs to exist");
      this._local_ice_candidates.push(anEvent.candidate);
      candidateHandler(this.label, anEvent.candidate);
    };
  },

  checkLocalMediaTracks : function() {
    info(`${this}: Checking local tracks ${JSON.stringify(this.expectedLocalTrackInfo)}`);
    const sendersWithTrack = this._pc.getSenders().filter(({track}) => track);
    is(sendersWithTrack.length, this.expectedLocalTrackInfo.length,
      "The number of senders with a track should be equal to the number of " +
      "expected local tracks.");

    // expectedLocalTrackInfo is in the same order that the tracks were added, and
    // so should the output of getSenders.
    this.expectedLocalTrackInfo.forEach((info, i) => {
      const sender = sendersWithTrack[i];
      is(sender, info.sender, `Sender ${i} should match`);
      is(sender.track, info.track, `Track ${i} should match`);
    });
  },

  /**
   * Checks that we are getting the media tracks we expect.
   */
  checkMediaTracks : function() {
    this.checkLocalMediaTracks();
  },

  checkLocalMsids: function() {
    const sdp = this.localDescription.sdp;
    const msections = sdputils.getMSections(sdp);
    const expectedStreamIdCounts = new Map();
    for (const {track, sender, streamId} of this.expectedLocalTrackInfo) {
      const transceiver = this._pc.getTransceivers().find(t => t.sender == sender);
      ok(transceiver, "There should be a transceiver for each sender");
      if (transceiver.mid) {
        const midFinder = new RegExp(`^a=mid:${transceiver.mid}$`, "m");
        const msection = msections.find(m => m.match(midFinder));
        ok(msection, `There should be a media section for mid = ${transceiver.mid}`);
        ok(msection.startsWith(`m=${track.kind}`),
          `Media section should be of type ${track.kind}`);
        const msidFinder = new RegExp(`^a=msid:${streamId} \\S+$`, "m");
        ok(msection.match(msidFinder),
          `Should find a=msid:${streamId} in media section`
          + " (with any track id for now)");
        const count = expectedStreamIdCounts.get(streamId) || 0;
        expectedStreamIdCounts.set(streamId, count + 1);
      }
    };

    // Check for any unexpected msids.
    const allMsids = sdp.match(new RegExp("^a=msid:\\S+", "mg"));
    if (!allMsids) {
      return;
    }
    const allStreamIds =
      allMsids.map(msidAttr => msidAttr.replace('a=msid:', ''));
    allStreamIds.forEach(id => {
      const count = expectedStreamIdCounts.get(id);
      ok(count, `Unexpected stream id ${id} found in local description.`);
      if (count) {
        expectedStreamIdCounts.set(id, count - 1);
      }
    });
  },

  /**
   * Check that media flow is present for the given media element by checking
   * that it reaches ready state HAVE_ENOUGH_DATA and progresses time further
   * than the start of the check.
   *
   * This ensures, that the stream being played is producing
   * data and, in case it contains a video track, that at least one video frame
   * has been displayed.
   *
   * @param {HTMLMediaElement} track
   *        The media element to check
   * @returns {Promise}
   *        A promise that resolves when media data is flowing.
   */
  waitForMediaElementFlow : function(element) {
    info("Checking data flow for element: " + element.id);
    is(element.ended, !element.srcObject.active,
       "Element ended should be the inverse of the MediaStream's active state");
    if (element.ended) {
      is(element.readyState, element.HAVE_CURRENT_DATA,
         "Element " + element.id + " is ended and should have had data");
      return Promise.resolve();
    }

    const haveEnoughData = (element.readyState == element.HAVE_ENOUGH_DATA ?
        Promise.resolve() :
        haveEvent(element, "canplay", wait(60000,
            new Error("Timeout for element " + element.id))))
      .then(_ => info("Element " + element.id + " has enough data."));

    const startTime = element.currentTime;
    const timeProgressed = timeout(
        listenUntil(element, "timeupdate", _ => element.currentTime > startTime),
        60000, "Element " + element.id + " should progress currentTime")
      .then();

    return Promise.all([haveEnoughData, timeProgressed]);
  },

  /**
   * Wait for RTP packet flow for the given MediaStreamTrack.
   *
   * @param {object} track
   *        A MediaStreamTrack to wait for data flow on.
   * @returns {Promise}
   *        Returns a promise which yields a StatsReport object with RTP stats.
   */
  async waitForRtpFlow(track) {
    info("waitForRtpFlow("+track.id+")");
    let hasFlow = (stats, retries) => {
      const dict = JSON.stringify([...stats.entries()]);
      info(`Checking for stats in  ${dict} for ${track.kind} track ${track.id}`
           + `retry number ${retries}`);
      const rtp = [...stats.values()].find(
          ({type}) => ["inbound-rtp", "outbound-rtp"].includes(type));
      if (!rtp) {
        return false;
      }
      info("Should have RTP stats for track " + track.id);
      info("RTP stats: " + JSON.stringify(rtp));
      let nrPackets = rtp[rtp.type == "outbound-rtp" ? "packetsSent"
                                                    : "packetsReceived"];
      info("Track " + track.id + " has " + nrPackets + " " +
           rtp.type + " RTP packets.");
      return nrPackets > 0;
    };

    // Time between stats checks
    const retryInterval = 500;
    // Timeout in ms
    const timeout = 30000;
    let retry = 0;
    // Check hasFlow at a reasonable interval
    for (let remaining = timeout; remaining >= 0; remaining -= retryInterval) {
      let stats = await this._pc.getStats(track);
      if (hasFlow(stats, retry++)) {
        ok(true, "RTP flowing for " + track.kind + " track " + track.id);
        return stats;
      }
      await wait(retryInterval);
    }
    throw new Error("Timeout checking for stats for track " + track.id
                    + " after at least" + timeout + "ms");
  },

  getExpectedActiveReceiveTracks : function() {
    return this._pc.getTransceivers()
      .filter(t => {
        return !t.stopped &&
               t.currentDirection &&
               (t.currentDirection != "inactive") &&
               (t.currentDirection != "sendonly");
      })
      .map(t => {
        info("Found transceiver that should be receiving RTP: mid=" + t.mid +
             " currentDirection=" + t.currentDirection + " kind=" +
             t.receiver.track.kind + " track-id=" + t.receiver.track.id);
        return t.receiver.track;
      })
      .filter(t => t);
  },

  getExpectedSendTracks : function() {
    return this._pc.getSenders().map(s => s.track).filter(t => t);
  },

  /**
   * Wait for presence of video flow on all media elements and rtp flow on
   * all sending and receiving track involved in this test.
   *
   * @returns {Promise}
   *        A promise that resolves when media flows for all elements and tracks
   */
  waitForMediaFlow : function() {
    return Promise.all([].concat(
      this.localMediaElements.map(element => this.waitForMediaElementFlow(element)),
      this.remoteMediaElements.filter(elem =>
          this.getExpectedActiveReceiveTracks()
            .some(track => elem.srcObject.getTracks().some(t => t == track))
        )
        .map(elem => this.waitForMediaElementFlow(elem)),
      this.getExpectedActiveReceiveTracks().map(track => this.waitForRtpFlow(track)),
      this.getExpectedSendTracks().map(track => this.waitForRtpFlow(track))));
  },

  async waitForSyncedRtcp() {
    // Ensures that RTCP is present
    let ensureSyncedRtcp = async () => {
      let report = await this._pc.getStats();
      for (const v of report.values()) {
        if (v.type.endsWith("bound-rtp") && !(v.remoteId || v.localId)) {
          info(`${v.id} is missing remoteId or localId: ${JSON.stringify(v)}`);
          return null;
        }
        if (v.type == "remote-inbound-rtp" && v.roundTripTime === undefined) {
          info(`${v.id} is missing roundTripTime: ${JSON.stringify(v)}`);
          return null;
        }
      }
      return report;
    }
    let attempts = 0;
    // Time-units are MS
    const waitPeriod = 100;
    const maxTime = 20000;
    for (let totalTime = maxTime; totalTime > 0; totalTime -= waitPeriod) {
      try {
        let syncedStats = await ensureSyncedRtcp();
        if (syncedStats) {
          return syncedStats;
        }
      } catch (e) {
          info(e);
          info(e.stack);
          throw e;
      }
      attempts += 1;
      info(`waitForSyncedRtcp: no sync on attempt ${attempts}, retrying.`);
      await wait(waitPeriod);
    }
    throw Error("Waiting for synced RTCP timed out after at least "
                + maxTime + "ms");
  },

  /**
   * Check that correct audio (typically a flat tone) is flowing to this
   * PeerConnection for each transceiver that should be receiving. Uses
   * WebAudio AnalyserNodes to compare input and output audio data in the
   * frequency domain.
   *
   * @param {object} from
   *        A PeerConnectionWrapper whose audio RTPSender we use as source for
   *        the audio flow check.
   * @returns {Promise}
   *        A promise that resolves when we're receiving the tone/s from |from|.
   */
  checkReceivingToneFrom : async function(audiocontext, from,
      cancel = wait(60000, new Error("Tone not detected"))) {
    let localTransceivers = this._pc.getTransceivers()
      .filter(t => t.mid)
      .filter(t => t.receiver.track.kind == "audio")
      .sort((t1, t2) => t1.mid < t2.mid);
    let remoteTransceivers = from._pc.getTransceivers()
      .filter(t => t.mid)
      .filter(t => t.receiver.track.kind == "audio")
      .sort((t1, t2) => t1.mid < t2.mid);

    is(localTransceivers.length, remoteTransceivers.length,
       "Same number of associated audio transceivers on remote and local.");

    for (let i = 0; i < localTransceivers.length; i++) {
      is(localTransceivers[i].mid, remoteTransceivers[i].mid,
         "Transceivers at index " + i + " have the same mid.");

      if (!remoteTransceivers[i].sender.track) {
        continue;
      }

      if (remoteTransceivers[i].currentDirection == "recvonly" ||
          remoteTransceivers[i].currentDirection == "inactive") {
        continue;
      }

      let sendTrack = remoteTransceivers[i].sender.track;
      let inputElem = from.getMediaElementForTrack(sendTrack, "local");
      ok(inputElem,
         "Remote wrapper should have a media element for track id " +
         sendTrack.id);
      let inputAudioStream = from.getStreamForSendTrack(sendTrack);
      ok(inputAudioStream,
         "Remote wrapper should have a stream for track id " + sendTrack.id);
      let inputAnalyser =
        new AudioStreamAnalyser(audiocontext, inputAudioStream);

      let recvTrack = localTransceivers[i].receiver.track;
      let outputAudioStream = this.getStreamForRecvTrack(recvTrack);
      ok(outputAudioStream,
         "Local wrapper should have a stream for track id " + recvTrack.id);
      let outputAnalyser =
        new AudioStreamAnalyser(audiocontext, outputAudioStream);

      let error = null;
      cancel.then(e => error = e);

      let indexOfMax = data =>
        data.reduce((max, val, i) => (val >= data[max]) ? i : max, 0);

      await outputAnalyser.waitForAnalysisSuccess(() => {
        if (error) {
          throw error;
        }

        let inputData = inputAnalyser.getByteFrequencyData();
        let outputData = outputAnalyser.getByteFrequencyData();

        let inputMax = indexOfMax(inputData);
        let outputMax = indexOfMax(outputData);
        info(`Comparing maxima; input[${inputMax}] = ${inputData[inputMax]},`
          + ` output[${outputMax}] = ${outputData[outputMax]}`);
        if (!inputData[inputMax] || !outputData[outputMax]) {
          return false;
        }

        // When the input and output maxima are within reasonable distance (2% of
        // total length, which means ~10 for length 512) from each other, we can
        // be sure that the input tone has made it through the peer connection.
        info(`input data length: ${inputData.length}`);
        return Math.abs(inputMax - outputMax) < (inputData.length * 0.02);
      });
    }
  },

  /**
   * Check that stats are present by checking for known stats.
   */
  getStats : function(selector) {
    return this._pc.getStats(selector).then(stats => {
      info(this + ": Got stats: " + JSON.stringify(stats));
      this._last_stats = stats;
      return stats;
    });
  },

  /**
   * Checks that we are getting the media streams we expect.
   *
   * @param {object} stats
   *        The stats to check from this PeerConnectionWrapper
   */
  checkStats : function(stats, twoMachines) {
    // Allow for clock drift observed on Windows 7. (Bug 979649)
    const isWin7 = navigator.userAgent.includes("Windows NT 6.1");
    const clockDriftAllowanceMs = isWin7 ? 1000 : 250;
    const isRemote = ({type}) =>
        ["remote-outbound-rtp", "remote-inbound-rtp"].includes(type);
    var counters = {};
    for (let [key, res] of stats) {
      info("Checking stats for " + key + " : " + res);
      // validate stats
      ok(res.id == key, "Coherent stats id");
      // Bug 1430255: WebRTC uses a different timebase than JS ATM
      // so there can be differences between timestamp and Date.now().
      const nowish = Date.now() + clockDriftAllowanceMs;
      const minimum = this.whenCreated - clockDriftAllowanceMs;
      const type = isRemote(res) ? "rtcp" : "rtp";
      if (!twoMachines) {
        ok(res.timestamp >= minimum,
           `Valid ${type} timestamp ${res.timestamp} >= ${minimum} (
              ${res.timestamp - minimum} ms)`);
        ok(res.timestamp <= nowish,
           `Valid ${type} timestamp ${res.timestamp} <= ${nowish} (
              ${res.timestamp - nowish} ms)`);
      }
      if (isRemote(res)) {
        continue;
      }
      counters[res.type] = (counters[res.type] || 0) + 1;

      switch (res.type) {
        case "inbound-rtp":
        case "outbound-rtp": {
          // Inbound tracks won't have an ssrc if RTP is not flowing.
          // (eg; negotiated inactive)
          ok(res.ssrc || res.type == "inbound-rtp", "Outbound RTP stats has an ssrc.");

          if (res.ssrc) {
            // ssrc is a 32 bit number returned as an unsigned long
            ok(!/[^0-9]/.test(`${res.ssrc}`), "SSRC is numeric");
            ok(parseInt(res.ssrc) < Math.pow(2,32), "SSRC is within limits");
          }

          if (res.type == "outbound-rtp") {
            ok(res.packetsSent !== undefined, "Rtp packetsSent");
            // We assume minimum payload to be 1 byte (guess from RFC 3550)
            ok(res.bytesSent >= res.packetsSent, "Rtp bytesSent");
          } else {
            ok(res.packetsReceived !== undefined, "Rtp packetsReceived");
            ok(res.bytesReceived >= res.packetsReceived, "Rtp bytesReceived");
          }
          if (res.remoteId) {
            var rem = stats.get(res.remoteId);
            ok(isRemote(rem), "Remote is rtcp");
            ok(rem.localId == res.id, "Remote backlink match");
            if (res.type == "outbound-rtp") {
              ok(rem.type == "remote-inbound-rtp", "Rtcp is inbound");
              ok(rem.packetsLost !== undefined, "Rtcp packetsLost");
              if (!this.disableRtpCountChecking) {
                // no guarantee which one is newer!
                // Note: this must change when we add a timestamp field to remote RTCP reports
                // and make rem.timestamp be the reception time
                if (res.timestamp < rem.timestamp) {
                  info("REVERSED timestamps: rec:" +
                    rem.packetsReceived + " time:" + rem.timestamp + " sent:" + res.packetsSent + " time:" + res.timestamp);
                }
              }
              ok(rem.jitter !== undefined, "Rtcp jitter");
              if (rem.roundTripTime) {
                ok(rem.roundTripTime >= 0,
                   "Rtcp rtt " + rem.roundTripTime + " >= 0");
                ok(rem.roundTripTime < 60,
                   "Rtcp rtt " + rem.roundTripTime + " < 1 min");
              }
            } else {
              ok(rem.type == "remote-outbound-rtp", "Rtcp is outbound");
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

    var nin = this._pc.getTransceivers()
      .filter(t => {
        return !t.stopped &&
               (t.currentDirection != "inactive") &&
               (t.currentDirection != "sendonly");
      }).length;
    const nout = Object.keys(this.expectedLocalTrackInfo).length;
    var ndata = this.dataChannels.length;

    // TODO(Bug 957145): Restore stronger inbound-rtp test once Bug 948249 is fixed
    //is((counters["inbound-rtp"] || 0), nin, "Have " + nin + " inbound-rtp stat(s)");
    ok((counters["inbound-rtp"] || 0) >= nin, "Have at least " + nin + " inbound-rtp stat(s) *");

    is(counters["outbound-rtp"] || 0, nout, "Have " + nout + " outbound-rtp stat(s)");

    var numLocalCandidates  = counters["local-candidate"] || 0;
    var numRemoteCandidates = counters["remote-candidate"] || 0;
    // If there are no tracks, there will be no stats either.
    if (nin + nout + ndata > 0) {
      ok(numLocalCandidates, "Have local-candidate stat(s)");
      ok(numRemoteCandidates, "Have remote-candidate stat(s)");
    } else {
      is(numLocalCandidates, 0, "Have no local-candidate stats");
      is(numRemoteCandidates, 0, "Have no remote-candidate stats");
    }
  },

  /**
   * Compares the Ice server configured for this PeerConnectionWrapper
   * with the ICE candidates received in the RTCP stats.
   *
   * @param {object} stats
   *        The stats to be verified for relayed vs. direct connection.
   */
  checkStatsIceConnectionType : function(stats, expectedLocalCandidateType) {
    let lId;
    let rId;
    for (let stat of stats.values()) {
      if (stat.type == "candidate-pair" && stat.selected) {
        lId = stat.localCandidateId;
        rId = stat.remoteCandidateId;
        break;
      }
    }
    isnot(lId, undefined, "Got local candidate ID " + lId + " for selected pair");
    isnot(rId, undefined, "Got remote candidate ID " + rId + " for selected pair");
    let lCand = stats.get(lId);
    let rCand = stats.get(rId);
    if (!lCand || !rCand) {
      ok(false,
         "failed to find candidatepair IDs or stats for local: "+ lId +" remote: "+ rId);
      return;
    }

    info("checkStatsIceConnectionType verifying: local=" +
         JSON.stringify(lCand) + " remote=" + JSON.stringify(rCand));
    expectedLocalCandidateType = expectedLocalCandidateType || "host";
    var candidateType = lCand.candidateType;
    if ((lCand.relayProtocol === "tcp") && (candidateType === "relay")) {
      candidateType = "relay-tcp";
    }

    if ((expectedLocalCandidateType === "srflx") &&
        (candidateType === "prflx")) {
      // Be forgiving of prflx when expecting srflx, since that can happen due
      // to timing.
      candidateType = "srflx";
    }

    is(candidateType,
       expectedLocalCandidateType,
       "Local candidate type is what we expected for selected pair");
  },

  /**
   * Compares amount of established ICE connection according to ICE candidate
   * pairs in the stats reporting with the expected amount of connection based
   * on the constraints.
   *
   * @param {object} stats
   *        The stats to check for ICE candidate pairs
   * @param {object} testOptions
   *        The test options object from the PeerConnectionTest
   */
  checkStatsIceConnections : function(stats, testOptions) {
    var numIceConnections = 0;
    stats.forEach(stat => {
      if ((stat.type === "candidate-pair") && stat.selected) {
        numIceConnections += 1;
      }
    });
    info("ICE connections according to stats: " + numIceConnections);
    isnot(numIceConnections, 0, "Number of ICE connections according to stats is not zero");
    if (testOptions.bundle) {
      if (testOptions.rtcpmux) {
        is(numIceConnections, 1, "stats reports exactly 1 ICE connection");
      } else {
        is(numIceConnections, 2, "stats report exactly 2 ICE connections for media and RTCP");
      }
    } else {
      var numAudioTransceivers =
        this._pc.getTransceivers().filter((transceiver) => {
          return (!transceiver.stopped) && transceiver.receiver.track.kind == "audio";
        }).length;

      var numVideoTransceivers =
        this._pc.getTransceivers().filter((transceiver) => {
          return (!transceiver.stopped) && transceiver.receiver.track.kind == "video";
        }).length;

      var numExpectedTransports = numAudioTransceivers + numVideoTransceivers;
      if (!testOptions.rtcpmux) {
        numExpectedTransports *= 2;
      }

      if (this.dataChannels.length) {
        ++numExpectedTransports;
      }

      info("expected audio + video + data transports: " + numExpectedTransports);
      is(numIceConnections, numExpectedTransports, "stats ICE connections matches expected A/V transports");
    }
  },

  expectNegotiationNeeded : function() {
    if (!this.observedNegotiationNeeded) {
      this.observedNegotiationNeeded = new Promise((resolve) => {
        this.onnegotiationneeded = resolve;
      });
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
  hasStat : function(stats, props) {
    for (let res of stats.values()) {
      var match = true;
      for (let prop in props) {
        if (res[prop] !== props[prop]) {
          match = false;
          break;
        }
      }
      if (match) {
        return true;
      }
    }
    return false;
  },

  /**
   * Closes the connection
   */
  close : function() {
    this._pc.close();
    this.localMediaElements.forEach(e => e.pause());
    info(this + ": Closed connection.");
  },

  /**
   * Returns the string representation of the class
   *
   * @returns {String} The string representation
   */
  toString : function() {
    return "PeerConnectionWrapper (" + this.label + ")";
  }
};

// haxx to prevent SimpleTest from failing at window.onload
function addLoadEvent() {}

function loadScript(...scripts) {
  return Promise.all(scripts.map(script => {
    var el = document.createElement("script");
    if (typeof scriptRelativePath === 'string' && script.charAt(0) !== '/') {
      script = scriptRelativePath + script;
    }
    el.src = script;
    document.head.appendChild(el);
    return new Promise(r => {
      el.onload = r;
      el.onerror = r;
    });
  }));
}

// Ensure SimpleTest.js is loaded before other scripts.
var scriptsReady = loadScript("/tests/SimpleTest/SimpleTest.js").then(() => {
  return loadScript(
    "../../test/manifest.js",
    "head.js",
    "templates.js",
    "turnConfig.js",
    "dataChannel.js",
    "network.js",
    "sdpUtils.js"
  );
});

function createHTML(options) {
  return scriptsReady.then(() => realCreateHTML(options));
}

var iceServerWebsocket;
var iceServersArray = [];

var addTurnsSelfsignedCerts = () => {
  var gUrl = SimpleTest.getTestFileURL('addTurnsSelfsignedCert.js');
  var gScript = SpecialPowers.loadChromeScript(gUrl);
  var certs = [];
  // If the ICE server is running TURNS, and includes a "cert" attribute in
  // its JSON, we set up an override that will forgive things like
  // self-signed for it.
  iceServersArray.forEach(iceServer => {
    if (iceServer.hasOwnProperty("cert")) {
      iceServer.urls.forEach(url => {
        if (url.startsWith("turns:")) {
          // Assumes no port or params!
          certs.push({"cert": iceServer.cert, "hostname": url.substr(6)});
        }
      });
    }
  });

  return new Promise((resolve, reject) => {
    gScript.addMessageListener('certs-added', () => {
      resolve();
    });

    gScript.sendAsyncMessage('add-turns-certs', certs);
  });
};

var setupIceServerConfig = useIceServer => {
  // We disable ICE support for HTTP proxy when using a TURN server, because
  // mochitest uses a fake HTTP proxy to serve content, which will eat our STUN
  // packets for TURN TCP.
  var enableHttpProxy = enable =>
    SpecialPowers.pushPrefEnv(
      {'set': [['media.peerconnection.disable_http_proxy', !enable]]});

  var spawnIceServer = () => new Promise( (resolve, reject) => {
    iceServerWebsocket = new WebSocket("ws://localhost:8191/");
    iceServerWebsocket.onopen = (event) => {
      info("websocket/process bridge open, starting ICE Server...");
      iceServerWebsocket.send("iceserver");
    }

    iceServerWebsocket.onmessage = event => {
      // The first message will contain the iceServers configuration, subsequent
      // messages are just logging.
      info("ICE Server: " + event.data);
      resolve(event.data);
    }

    iceServerWebsocket.onerror = () => {
      reject("ICE Server error: Is the ICE server websocket up?");
    }

    iceServerWebsocket.onclose = () => {
      info("ICE Server websocket closed");
      reject("ICE Server gone before getting configuration");
    }
  });

  if (!useIceServer) {
    info("Skipping ICE Server for this test");
    return enableHttpProxy(true);
  }

  return enableHttpProxy(false)
    .then(spawnIceServer)
    .then(iceServersStr => { iceServersArray = JSON.parse(iceServersStr); })
    .then(addTurnsSelfsignedCerts);
};

async function runNetworkTest(testFunction, fixtureOptions = {}) {

  let {AppConstants} = SpecialPowers.Cu.import("resource://gre/modules/AppConstants.jsm", {});
  let isNightly = AppConstants.NIGHTLY_BUILD;
  let isAndroid = AppConstants.platform == "android";

  await scriptsReady;
  await runTestWhenReady(async options =>
    {
      await startNetworkAndTest();
      await setupIceServerConfig(fixtureOptions.useIceServer);

      // currently we set android hardware encoder default enabled in nightly.
      // But before QA approves the quality, we want to ensure the legacy
      // encoder is working fine.
      if (isNightly && isAndroid) {
        let value = Math.random() >= 0.5;
        await SpecialPowers.pushPrefEnv(
        {
          set: [
            ['media.navigator.hardware.vp8_encode.acceleration_enabled', value],
            ['media.navigator.hardware.vp8_encode.acceleration_remote_enabled', value]
          ]
        });
      }
      await testFunction(options);
    }
  );
}
