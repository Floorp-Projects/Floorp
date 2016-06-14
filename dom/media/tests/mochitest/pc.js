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

  if (iceServersArray.length) {
    if (!options.turn_disabled_local) {
      options.config_local = options.config_local || {}
      options.config_local.iceServers = iceServersArray;
    }
    if (!options.turn_disabled_remote) {
      options.config_remote = options.config_remote || {}
      options.config_remote.iceServers = iceServersArray;
    }
  }
  else if (typeof turnServers !== "undefined") {
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

    return new Promise(resolve => {
      pc.onsignalingstatechange = e => {
        is(e.target.signalingState, "closed", "signalingState is closed");
        resolve();
      };
      pc.close();
    });
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
    60000, "failed to close data channels")
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
  var complete = timerGuard(allClosed, 60000, "failed to close data channel pair");

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
PeerConnectionTest.prototype.send = function(data, options) {
  options = options || { };
  var source = options.sourceChannel ||
           this.pcLocal.dataChannels[this.pcLocal.dataChannels.length - 1];
  var target = options.targetChannel ||
           this.pcRemote.dataChannels[this.pcRemote.dataChannels.length - 1];
  var bufferedamount = options.bufferedAmountLowThreshold || 0;
  var bufferlow_fired = true; // to make testing later easier
  if (bufferedamount != 0) {
    source.bufferedAmountLowThreshold = bufferedamount;
    bufferlow_fired = false;
    source.onbufferedamountlow = function() {
      bufferlow_fired = true;
    };
  }

  return new Promise(resolve => {
    // Register event handler for the target channel
      target.onmessage = e => {
        ok(bufferlow_fired, "bufferedamountlow event fired");
	resolve({ channel: target, data: e.data });
    };

    source.send(data);
  });
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
    this.pcRemote.expectDataChannel();
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
 * @param {RTCSessionDescription} desc
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
 * @param {RTCSessionDescription} desc
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
      [PC_LOCAL_REMOVE_VP8_FROM_OFFER]);
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
  return this.chain.execute()
    .then(() => this.close())
    .then(() => {
      if (window.SimpleTest) {
        networkTestFinished();
      } else {
        finish();
      }
    })
    .catch(e =>
           ok(false, 'Error in test execution: ' + e +
              ((typeof e.stack === 'string') ?
               (' ' + e.stack.split('\n').join(' ... ')) : '')));
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

  this.expectedLocalTrackInfoById = {};
  this.expectedRemoteTrackInfoById = {};
  this.observedRemoteTrackInfoById = {};

  this.disableRtpCountChecking = false;

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
    info(this + ": oniceconnectionstatechange fired, new state is: " + this._pc.iceConnectionState);
    Object.keys(this.ice_connection_callbacks).forEach(name => {
      this.ice_connection_callbacks[name]();
    });
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

  ensureMediaElement : function(track, stream, direction) {
    var element = getMediaElement(this.label, direction, stream.id);

    if (!element) {
      element = createMediaElement(this.label, direction, stream.id,
                                   this.audioElementsOnly);
      if (direction == "local") {
        this.localMediaElements.push(element);
      } else if (direction == "remote") {
        this.remoteMediaElements.push(element);
      }
    }

    // We do this regardless, because sometimes we end up with a new stream with
    // an old id (ie; the rollback tests cause the same stream to be added
    // twice)
    element.srcObject = stream;
    element.play();
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

    ok(track.id, "track has id");
    ok(track.kind, "track has kind");
    ok(stream.id, "stream has id");
    this.expectedLocalTrackInfoById[track.id] = {
      type: track.kind,
      streamId: stream.id,
    };

    // This will create one media element per track, which might not be how
    // we set up things with the RTCPeerConnection. It's the only way
    // we can ensure all sent tracks are flowing however.
    this.ensureMediaElement(track, new MediaStream([track]), "local");

    return this.observedNegotiationNeeded;
  },

  /**
   * Callback when we get local media. Also an appropriate HTML media element
   * will be created, which may be obtained later with |getMediaElement|.
   *
   * @param {MediaStream} stream
   *        Media stream to handle
   */
  attachLocalStream : function(stream) {
    info("Got local media stream: (" + stream.id + ")");

    this.expectNegotiationNeeded();
    // In order to test both the addStream and addTrack APIs, we do half one
    // way, half the other, at random.
    if (Math.random() < 0.5) {
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

    stream.getTracks().forEach(track => {
      ok(track.id, "track has id");
      ok(track.kind, "track has kind");
      this.expectedLocalTrackInfoById[track.id] = {
          type: track.kind,
          streamId: stream.id
        };
      this.ensureMediaElement(track, stream, "local");
    });
  },

  removeSender : function(index) {
    var sender = this._pc.getSenders()[index];
    delete this.expectedLocalTrackInfoById[sender.track.id];
    this.expectNegotiationNeeded();
    this._pc.removeTrack(sender);
    return this.observedNegotiationNeeded;
  },

  senderReplaceTrack : function(index, withTrack, withStreamId) {
    var sender = this._pc.getSenders()[index];
    delete this.expectedLocalTrackInfoById[sender.track.id];
    this.expectedLocalTrackInfoById[withTrack.id] = {
        type: withTrack.kind,
        streamId: withStreamId
      };
    return sender.replaceTrack(withTrack);
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
    return Promise.all(constraintsList.map(constraints => {
      return getUserMedia(constraints).then(stream => {
        if (constraints.audio) {
          stream.getAudioTracks().map(track => {
            info(this + " gUM local stream " + stream.id +
              " with audio track " + track.id);
          });
        }
        if (constraints.video) {
          stream.getVideoTracks().map(track => {
            info(this + " gUM local stream " + stream.id +
              " with video track " + track.id);
          });
        }
        return this.attachLocalStream(stream);
      });
    }));
  },

  /**
   * Create a new data channel instance.  Also creates a promise called
   * `this.nextDataChannel` that resolves when the next data channel arrives.
   */
  expectDataChannel: function(message) {
    this.nextDataChannel = new Promise(resolve => {
      this.ondatachannel = e => {
        ok(e.channel, message);
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
   *        RTCSessionDescription for the local description request
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
   *        RTCSessionDescription for the local description request
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
   *        RTCSessionDescription for the remote description request
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
   *        RTCSessionDescription for the remote description request
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
      if (Object.keys(signalingStateTransitions).indexOf(oldstate) >= 0) {
        ok(signalingStateTransitions[oldstate].indexOf(newstate) >= 0, this + ": legal signaling state transition from " + oldstate + " to " + newstate);
      } else {
        ok(false, this + ": old signaling state " + oldstate + " missing in signaling transition array");
      }
      this.signalingStateLog.push(newstate);
    });
  },

  /**
   * Checks whether a given track is expected, has not been observed yet, and
   * is of the correct type. Then, moves the track from
   * |expectedTrackInfoById| to |observedTrackInfoById|.
   */
  checkTrackIsExpected : function(track,
                                  expectedTrackInfoById,
                                  observedTrackInfoById) {
    ok(expectedTrackInfoById[track.id], "track id " + track.id + " was expected");
    ok(!observedTrackInfoById[track.id], "track id " + track.id + " was not yet observed");
    var observedKind = track.kind;
    var expectedKind = expectedTrackInfoById[track.id].type;
    is(observedKind, expectedKind,
        "track id " + track.id + " was of kind " +
        observedKind + ", which matches " + expectedKind);
    observedTrackInfoById[track.id] = expectedTrackInfoById[track.id];
  },

  isTrackOnPC: function(track) {
    return this._pc.getRemoteStreams().some(s => !!s.getTrackById(track.id));
  },

  allExpectedTracksAreObserved: function(expected, observed) {
    return Object.keys(expected).every(trackId => observed[trackId]);
  },

  setupTrackEventHandler: function() {
    this._pc.addEventListener('track', event => {
      info(this + ": 'ontrack' event fired for " + JSON.stringify(event.track));

      this.checkTrackIsExpected(event.track,
                                this.expectedRemoteTrackInfoById,
                                this.observedRemoteTrackInfoById);
      ok(this.isTrackOnPC(event.track), "Found track " + event.track.id);

      this.ensureMediaElement(event.track, event.streams[0], 'remote');
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
   * Returns if the ICE the connection state is "connected".
   *
   * @returns {boolean} True if the connection state is "connected", otherwise false.
   */
  isIceConnected : function() {
    info(this + ": iceConnectionState = " + this.iceConnectionState);
    return this.iceConnectionState === "connected";
  },

  /**
   * Returns if the ICE the connection state is "checking".
   *
   * @returns {boolean} True if the connection state is "checking", otherwise false.
   */
  isIceChecking : function() {
    return this.iceConnectionState === "checking";
  },

  /**
   * Returns if the ICE the connection state is "new".
   *
   * @returns {boolean} True if the connection state is "new", otherwise false.
   */
  isIceNew : function() {
    return this.iceConnectionState === "new";
  },

  /**
   * Checks if the ICE connection state still waits for a connection to get
   * established.
   *
   * @returns {boolean} True if the connection state is "checking" or "new",
   *  otherwise false.
   */
  isIceConnectionPending : function() {
    return (this.isIceChecking() || this.isIceNew());
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
      if (Object.keys(iceStateTransitions).indexOf(oldstate) != -1) {
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
          ok(iceStateTransitions[oldstate].indexOf(newstate) != -1, this + ": legal ICE state transition from " + oldstate + " to " + newstate);
        }
      } else {
        ok(false, this + ": old ICE state " + oldstate + " missing in ICE transition array");
      }
      this.iceConnectionLog.push(newstate);
    };
  },

  /**
   * Registers a callback for the ICE connection state change and
   * reports success (=connected) or failure via the callbacks.
   * States "new" and "checking" are ignored.
   *
   * @returns {Promise}
   *          resolves when connected, rejects on failure
   */
  waitForIceConnected : function() {
    return new Promise((resolve, reject) =>
        this.ice_connection_callbacks.waitForIceConnected = () => {
      if (this.isIceConnected()) {
        delete this.ice_connection_callbacks.waitForIceConnected;
        resolve();
      } else if (!this.isIceConnectionPending()) {
        delete this.ice_connection_callbacks.waitForIceConnected;
        reject(new Error('ICE failed'));
      }
    });
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
        /* Bug 1193731. Accroding to WebRTC spec 4.3.1 the ICE Agent first sets
         * the gathering state to completed (step 3.) before sending out the
         * null newCandidate in step 4. */
        todo(this._pc.iceGatheringState === 'completed',
           "ICE gathering state has reached completed");
        resolveEndOfTrickle(this.label);
        return;
      }

      info(this.label + ": iceCandidate = " + JSON.stringify(anEvent.candidate));
      ok(anEvent.candidate.candidate.length > 0, "ICE candidate contains candidate");
      ok(anEvent.candidate.sdpMid.length > 0, "SDP mid not empty");

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
    var observed = {};
    info(this + " Checking local tracks " + JSON.stringify(this.expectedLocalTrackInfoById));
    this._pc.getSenders().forEach(sender => {
      this.checkTrackIsExpected(sender.track, this.expectedLocalTrackInfoById, observed);
    });

    Object.keys(this.expectedLocalTrackInfoById).forEach(
        id => ok(observed[id], this + " local id " + id + " was observed"));
  },

  /**
   * Checks that we are getting the media tracks we expect.
   */
  checkMediaTracks : function() {
    this.checkLocalMediaTracks();

    info(this + " Checking remote tracks " +
         JSON.stringify(this.expectedRemoteTrackInfoById));

    ok(this.allExpectedTracksAreObserved(this.expectedRemoteTrackInfoById,
                                         this.observedRemoteTrackInfoById),
       "All expected tracks have been observed"
       + "\nexpected: " + JSON.stringify(this.expectedRemoteTrackInfoById)
       + "\nobserved: " + JSON.stringify(this.observedRemoteTrackInfoById));
  },

  checkMsids: function() {
    var checkSdpForMsids = (desc, expectedTrackInfo, side) => {
      Object.keys(expectedTrackInfo).forEach(trackId => {
        var streamId = expectedTrackInfo[trackId].streamId;
        ok(desc.sdp.match(new RegExp("a=msid:" + streamId + " " + trackId)),
           this + ": " + side + " SDP contains stream " + streamId +
           " and track " + trackId );
      });
    };

    checkSdpForMsids(this.localDescription, this.expectedLocalTrackInfoById,
                     "local");
    checkSdpForMsids(this.remoteDescription, this.expectedRemoteTrackInfoById,
                     "remote");
  },

  markRemoteTracksAsNegotiated: function() {
    Object.values(this.observedRemoteTrackInfoById).forEach(
        trackInfo => trackInfo.negotiated = true);
  },

  rollbackRemoteTracksIfNotNegotiated: function() {
    Object.keys(this.observedRemoteTrackInfoById).forEach(
        id => {
          if (!this.observedRemoteTrackInfoById[id].negotiated) {
            delete this.observedRemoteTrackInfoById[id];
          }
        });
  },

  /**
   * Check that media flow is present on the given media element by waiting for
   * it to reach ready state HAVE_ENOUGH_DATA and progress time further than
   * the start of the check.
   *
   * This ensures, that the stream being played is producing
   * data and that at least one video frame has been displayed.
   *
   * @param {object} element
   *        A media element to wait for data flow on.
   * @returns {Promise}
   *        A promise that resolves when media is flowing.
   */
  waitForMediaElementFlow : function(element) {
    return new Promise(resolve => {
      info("Checking data flow to element: " + element.id);
      if (element.ended && element.readyState >= element.HAVE_CURRENT_DATA) {
        resolve();
        return;
      }
      var haveEnoughData = false;
      var oncanplay = () => {
        info("Element " + element.id + " saw 'canplay', " +
             "meaning HAVE_ENOUGH_DATA was just reached.");
        haveEnoughData = true;
        element.removeEventListener("canplay", oncanplay);
      };
      var ontimeupdate = () => {
        info("Element " + element.id + " saw 'timeupdate'" +
             ", currentTime=" + element.currentTime +
             "s, readyState=" + element.readyState);
        if (haveEnoughData || element.readyState == element.HAVE_ENOUGH_DATA) {
          element.removeEventListener("timeupdate", ontimeupdate);
          ok(true, "Media flowing for element: " + element.id);
          resolve();
        }
      };
      element.addEventListener("canplay", oncanplay);
      element.addEventListener("timeupdate", ontimeupdate);
    });
  },

  /**
   * Wait for RTP packet flow for the given MediaStreamTrack.
   *
   * @param {object} track
   *        A MediaStreamTrack to wait for data flow on.
   * @returns {Promise}
   *        A promise that resolves when media is flowing.
   */
  waitForRtpFlow(track) {
    var hasFlow = stats => {
      var rtp = stats.get([...stats.keys()].find(key =>
        !stats.get(key).isRemote && stats.get(key).type.endsWith("boundrtp")));
      ok(rtp, "Should have RTP stats for track " + track.id);
      if (!rtp) {
        return false;
      }
      var nrPackets = rtp[rtp.type == "outboundrtp" ? "packetsSent"
                                                    : "packetsReceived"];
      info("Track " + track.id + " has " + nrPackets + " " +
           rtp.type + " RTP packets.");
      return nrPackets > 0;
    };

    info("Checking RTP packet flow for track " + track.id);

    var retry = () => this._pc.getStats(track)
      .then(stats => hasFlow(stats)? ok(true, "RTP flowing for track " + track.id) :
                                     wait(200).then(retry));
    return retry();
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
      this.remoteMediaElements.map(element => this.waitForMediaElementFlow(element)),
      this._pc.getSenders().map(sender => this.waitForRtpFlow(sender.track)),
      this._pc.getReceivers().map(receiver => this.waitForRtpFlow(receiver.track))));
  },

  /**
   * Check that correct audio (typically a flat tone) is flowing to this
   * PeerConnection. Uses WebAudio AnalyserNodes to compare input and output
   * audio data in the frequency domain.
   *
   * @param {object} from
   *        A PeerConnectionWrapper whose audio RTPSender we use as source for
   *        the audio flow check.
   * @returns {Promise}
   *        A promise that resolves when we're receiving the tone from |from|.
   */
  checkReceivingToneFrom : function(audiocontext, from) {
    var inputElem = from.localMediaElements[0];

    // As input we use the stream of |from|'s first available audio sender.
    var inputSenderTracks = from._pc.getSenders().map(sn => sn.track);
    var inputAudioStream = from._pc.getLocalStreams()
      .find(s => inputSenderTracks.some(t => t.kind == "audio" && s.getTrackById(t.id)));
    var inputAnalyser = new AudioStreamAnalyser(audiocontext, inputAudioStream);

    // It would have been nice to have a working getReceivers() here, but until
    // we do, let's use what remote streams we have.
    var outputAudioStream = this._pc.getRemoteStreams()
      .find(s => s.getAudioTracks().length > 0);
    var outputAnalyser = new AudioStreamAnalyser(audiocontext, outputAudioStream);

    var maxWithIndex = (a, b, i) => (b >= a.value) ? { value: b, index: i } : a;
    var initial = { value: -1, index: -1 };

    return new Promise((resolve, reject) => inputElem.ontimeupdate = () => {
      var inputData = inputAnalyser.getByteFrequencyData();
      var outputData = outputAnalyser.getByteFrequencyData();

      var inputMax = inputData.reduce(maxWithIndex, initial);
      var outputMax = outputData.reduce(maxWithIndex, initial);
      info("Comparing maxima; input[" + inputMax.index + "] = " + inputMax.value +
           ", output[" + outputMax.index + "] = " + outputMax.value);
      if (!inputMax.value || !outputMax.value) {
        return;
      }

      // When the input and output maxima are within reasonable distance
      // from each other, we can be sure that the input tone has made it
      // through the peer connection.
      if (Math.abs(inputMax.index - outputMax.index) < 10) {
        ok(true, "input and output audio data matches");
        inputElem.ontimeupdate = null;
        resolve();
      }
    });
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
    const isWinXP = navigator.userAgent.indexOf("Windows NT 5.1") != -1;

    // Use spec way of enumerating stats
    var counters = {};
    for (let [key, res] of stats) {
      // validate stats
      ok(res.id == key, "Coherent stats id");
      var nowish = Date.now() + 1000;        // TODO: clock drift observed
      var minimum = this.whenCreated - 1000; // on Windows XP (Bug 979649)
      if (isWinXP) {
        todo(false, "Can't reliably test rtcp timestamps on WinXP (Bug 979649)");
      } else if (!twoMachines) {
        // Bug 1225729: On android, sometimes the first RTCP of the first
        // test run gets this value, likely because no RTP has been sent yet.
        if (res.timestamp != 2085978496000) {
          ok(res.timestamp >= minimum,
             "Valid " + (res.isRemote? "rtcp" : "rtp") + " timestamp " +
                 res.timestamp + " >= " + minimum + " (" +
                 (res.timestamp - minimum) + " ms)");
          ok(res.timestamp <= nowish,
             "Valid " + (res.isRemote? "rtcp" : "rtp") + " timestamp " +
                 res.timestamp + " <= " + nowish + " (" +
                 (res.timestamp - nowish) + " ms)");
        } else {
          info("Bug 1225729: Uninitialized timestamp (" + res.timestamp +
                "), should be >=" + minimum + " and <= " + nowish);
        }
      }
      if (res.isRemote) {
        continue;
      }
      counters[res.type] = (counters[res.type] || 0) + 1;

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
              ok(rem.packetsLost !== undefined, "Rtcp packetsLost");
              ok(rem.bytesReceived >= rem.packetsReceived, "Rtcp bytesReceived");
              if (!this.disableRtpCountChecking) {
                ok(rem.packetsReceived <= res.packetsSent, "No more than sent packets");
                ok(rem.bytesReceived <= res.bytesSent, "No more than sent bytes");
              }
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

    // Use legacy way of enumerating stats
    var counters2 = {};
    for (let key in stats) {
      if (!stats.hasOwnProperty(key)) {
        continue;
      }
      var res = stats[key];
      if (!res.isRemote) {
        counters2[res.type] = (counters2[res.type] || 0) + 1;
      }
    }
    is(JSON.stringify(counters), JSON.stringify(counters2),
       "Spec and legacy variant of RTCStatsReport enumeration agree");
    var nin = Object.keys(this.expectedRemoteTrackInfoById).length;
    var nout = Object.keys(this.expectedLocalTrackInfoById).length;
    var ndata = this.dataChannels.length;

    // TODO(Bug 957145): Restore stronger inboundrtp test once Bug 948249 is fixed
    //is((counters["inboundrtp"] || 0), nin, "Have " + nin + " inboundrtp stat(s)");
    ok((counters.inboundrtp || 0) >= nin, "Have at least " + nin + " inboundrtp stat(s) *");

    is(counters.outboundrtp || 0, nout, "Have " + nout + " outboundrtp stat(s)");

    var numLocalCandidates  = counters.localcandidate || 0;
    var numRemoteCandidates = counters.remotecandidate || 0;
    // If there are no tracks, there will be no stats either.
    if (nin + nout + ndata > 0) {
      ok(numLocalCandidates, "Have localcandidate stat(s)");
      ok(numRemoteCandidates, "Have remotecandidate stat(s)");
    } else {
      is(numLocalCandidates, 0, "Have no localcandidate stats");
      is(numRemoteCandidates, 0, "Have no remotecandidate stats");
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
      if (stat.type == "candidatepair" && stat.selected) {
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
    if ((lCand.mozLocalTransport === "tcp") && (candidateType === "relayed")) {
      candidateType = "relayed-tcp";
    }

    if ((expectedLocalCandidateType === "serverreflexive") &&
        (candidateType === "peerreflexive")) {
      // Be forgiving of prflx when expecting srflx, since that can happen due
      // to timing.
      candidateType = "serverreflexive";
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
   * @param {object} counters
   *        The counters for media and data tracks based on constraints
   * @param {object} testOptions
   *        The test options object from the PeerConnectionTest
   */
  checkStatsIceConnections : function(stats,
      offerConstraintsList, offerOptions, testOptions) {
    var numIceConnections = 0;
    Object.keys(stats).forEach(key => {
      if ((stats[key].type === "candidatepair") && stats[key].selected) {
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
      // This code assumes that no media sections have been rejected due to
      // codec mismatch or other unrecoverable negotiation failures.
      var numAudioTracks =
          sdputils.countTracksInConstraint('audio', offerConstraintsList) ||
          ((offerOptions && offerOptions.offerToReceiveAudio) ? 1 : 0);

      var numVideoTracks =
          sdputils.countTracksInConstraint('video', offerConstraintsList) ||
          ((offerOptions && offerOptions.offerToReceiveVideo) ? 1 : 0);

      var numExpectedTransports = numAudioTracks + numVideoTracks;
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

var scriptsReady = Promise.all([
  "/tests/SimpleTest/SimpleTest.js",
  "head.js",
  "templates.js",
  "turnConfig.js",
  "dataChannel.js",
  "network.js",
  "sdpUtils.js"
].map(script  => {
  var el = document.createElement("script");
  if (typeof scriptRelativePath === 'string' && script.charAt(0) !== '/') {
    script = scriptRelativePath + script;
  }
  el.src = script;
  document.head.appendChild(el);
  return new Promise(r => { el.onload = r; el.onerror = r; });
}));

function createHTML(options) {
  return scriptsReady.then(() => realCreateHTML(options));
}

var iceServerWebsocket;
var iceServersArray = [];

var setupIceServerConfig = useIceServer => {
  // We disable ICE support for HTTP proxy when using a TURN server, because
  // mochitest uses a fake HTTP proxy to serve content, which will eat our STUN
  // packets for TURN TCP.
  var enableHttpProxy = enable => new Promise(resolve => {
    SpecialPowers.pushPrefEnv(
        {'set': [['media.peerconnection.disable_http_proxy', !enable]]},
        resolve);
  });

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
    .then(iceServersStr => { iceServersArray = JSON.parse(iceServersStr); });
};

function runNetworkTest(testFunction, fixtureOptions) {
  fixtureOptions = fixtureOptions || {}
  return scriptsReady.then(() =>
    runTestWhenReady(options =>
      startNetworkAndTest()
        .then(() => setupIceServerConfig(fixtureOptions.useIceServer))
        .then(() => testFunction(options))
    )
  );
}
