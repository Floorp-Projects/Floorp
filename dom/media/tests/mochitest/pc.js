/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 * This class mimics a state machine and handles a list of commands by
 * executing them synchronously.
 *
 * @constructor
 * @param {object} framework
 *        A back reference to the framework which makes use of the class. It's
 *        getting passed in as parameter to each command callback.
 */
function CommandChain(framework) {
  this._framework = framework;

  this._commands = [ ];
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

        info("Run step: " + step[0]);  // Label
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
  }
};


/**
 * Default list of commands to execute for a PeerConnection test.
 */
var commandsPeerConnection = [
  [
    'PC_LOCAL_GUM',
    function (test) {
      test.pcLocal.getAllUserMedia(function () {
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_GUM',
    function (test) {
      test.pcRemote.getAllUserMedia(function () {
        test.next();
      });
    }
  ],
  [
    'PC_CHECK_INITIAL_SIGNALINGSTATE',
    function (test) {
      is(test.pcLocal.signalingState, "stable",
         "Initial local signalingState is 'stable'");
      is(test.pcRemote.signalingState, "stable",
         "Initial remote signalingState is 'stable'");
      test.next();
    }
  ],
  [
    'PC_LOCAL_CREATE_OFFER',
    function (test) {
      test.createOffer(test.pcLocal, function () {
        is(test.pcLocal.signalingState, "stable",
           "Local create offer does not change signaling state");
        test.next();
      });
    }
  ],
  [
    'PC_LOCAL_SET_LOCAL_DESCRIPTION',
    function (test) {
      test.setLocalDescription(test.pcLocal, test.pcLocal._last_offer, function () {
        is(test.pcLocal.signalingState, "have-local-offer",
           "signalingState after local setLocalDescription is 'have-local-offer'");
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_SET_REMOTE_DESCRIPTION',
    function (test) {
      test.setRemoteDescription(test.pcRemote, test.pcLocal._last_offer, function () {
        is(test.pcRemote.signalingState, "have-remote-offer",
           "signalingState after remote setRemoteDescription is 'have-remote-offer'");
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_CREATE_ANSWER',
    function (test) {
      test.createAnswer(test.pcRemote, function () {
        is(test.pcRemote.signalingState, "have-remote-offer",
           "Remote createAnswer does not change signaling state");
        test.next();
      });
    }
  ],
  [
    'PC_LOCAL_SET_REMOTE_DESCRIPTION',
    function (test) {
      test.setRemoteDescription(test.pcLocal, test.pcRemote._last_answer, function () {
        is(test.pcLocal.signalingState, "stable",
           "signalingState after local setRemoteDescription is 'stable'");
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_SET_LOCAL_DESCRIPTION',
    function (test) {
      test.setLocalDescription(test.pcRemote, test.pcRemote._last_answer, function () {
        is(test.pcRemote.signalingState, "stable",
           "signalingState after remote setLocalDescription is 'stable'");
        test.next();
      });
    }
  ],
  [
    'PC_LOCAL_CHECK_MEDIA',
    function (test) {
      test.pcLocal.checkMedia(test.pcRemote.constraints);
      test.next();
    }
  ],
  [
    'PC_REMOTE_CHECK_MEDIA',
    function (test) {
      test.pcRemote.checkMedia(test.pcLocal.constraints);
      test.next();
    }
  ]
];

/**
 * This class handles tests for peer connections.
 *
 * @constructor
 * @param {object} [options={}]
 *        Optional options for the peer connection test
 * @param {object} [options.config_pc1=undefined]
 *        Configuration for the local peer connection instance
 * @param {object} [options.config_pc2=undefined]
 *        Configuration for the remote peer connection instance. If not defined
 *        the configuration from the local instance will be used
 */
function PeerConnectionTest(options) {
  // If no options are specified make it an empty object
  options = options || { };

  this.pcLocal = new PeerConnectionWrapper('pcLocal', options.config_pc1);
  this.pcRemote = new PeerConnectionWrapper('pcRemote', options.config_pc2 || options.config_pc1);

  // Create command chain instance and assign default commands
  this.chain = new CommandChain(this);
  this.chain.commands = commandsPeerConnection;

  var self = this;
  this.chain.onFinished = function () {
    self.teardown();
  }
}

/**
 * Executes the next command.
 */
PeerConnectionTest.prototype.next = function PCT_next() {
  this.chain.executeNext();
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
  peer.createAnswer(function (answer) {
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
  peer.createOffer(function (offer) {
    onSuccess(offer);
  });
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
function PCT_setLocalDescription(peer, desc, onSuccess) {
  var eventFired = false;
  var stateChanged = false;

  function check_next_test() {
    if (eventFired && stateChanged) {
      onSuccess();
    }
  }

  peer.onsignalingstatechange = function () {
    info(peer + ": 'onsignalingstatechange' event registered for async check");

    eventFired = true;
    check_next_test();
  };

  peer.setLocalDescription(desc, function () {
    stateChanged = true;
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
  this.pcLocal.constraints = constraintsLocal;
  this.pcRemote.constraints = constraintsRemote;
};

/**
 * Sets the media constraints used on a createOffer call in the test.
 *
 * @param {object} constraints the media constraints to use on createOffer
 */
PeerConnectionTest.prototype.setOfferConstraints =
function PCT_setOfferConstraints(constraints) {
  this.pcLocal.offerConstraints = constraints;
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
function PCT_setRemoteDescription(peer, desc, onSuccess) {
  var eventFired = false;
  var stateChanged = false;

  function check_next_test() {
    if (eventFired && stateChanged) {
      onSuccess();
    }
  }

  peer.onsignalingstatechange = function () {
    info(peer + ": 'onsignalingstatechange' event registered for async check");

    eventFired = true;
    check_next_test();
  };

  peer.setRemoteDescription(desc, function () {
    stateChanged = true;
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
  if (this.pcLocal) {
    this.pcLocal.close();
    this.pcLocal = null;
  }

  if (this.pcRemote) {
    this.pcRemote.close();
    this.pcRemote = null;
  }

  info("Test finished");
  SimpleTest.finish();
};


/**
 * This class handles acts as a wrapper around a PeerConnection instance.
 *
 * @constructor
 * @param {string} label
 *        Description for the peer connection instance
 * @param {object} configuration
 *        Configuration for the peer connection instance
 */
function PeerConnectionWrapper(label, configuration) {
  this.configuration = configuration;
  this.label = label;

  this.constraints = [ ];
  this.offerConstraints = {};
  this.streams = [ ];

  info("Creating new PeerConnectionWrapper: " + this);
  this._pc = new mozRTCPeerConnection(this.configuration);

  /**
   * Setup callback handlers
   */

  this.onsignalingstatechange = unexpectedEventAndFinish(this, 'onsignalingstatechange');


  var self = this;
  this._pc.onaddstream = function (event) {
    // Bug 834835: Assume type is video until we get get{Audio,Video}Tracks.
    self.attachMedia(event.stream, 'video', 'remote');
  };

  /**
   * Callback for native peer connection 'onsignalingstatechange' events. If no
   * custom handler has been specified via 'this.onsignalingstatechange', a
   * failure will be raised if an event of this type is caught.
   *
   * @param {Object} aEvent
   *        Event data which includes the newly created data channel
   */
  this._pc.onsignalingstatechange = function (aEvent) {
    info(self + ": 'onsignalingstatechange' event fired");

    self.onsignalingstatechange();
    self.onsignalingstatechange = unexpectedEventAndFinish(self, 'onsignalingstatechange');
  }
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
   * Returns the remote signaling state.
   *
   * @returns {object} The local description
   */
  get signalingState() {
    return this._pc.signalingState;
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
    info("Got media stream: " + type + " (" + side + ")");
    this.streams.push(stream);

    if (side === 'local') {
      this._pc.addStream(stream);
    }

    var element = createMediaElement(type, this._label + '_' + side);
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
          var type = constraints;

          if (constraints.audio) {
            type = 'audio';
          }

          if (constraints.video) {
            type += 'video';
          }

          self.attachMedia(stream, type, 'local');

          _getAllUserMedia(constraintsList, index + 1);
        }, unexpectedCallbackAndFinish());
      } else {
        onSuccess();
      }
    }

    info("Get " + this.constraints.length + " local streams");
    _getAllUserMedia(this.constraints, 0);
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
      self._last_offer = offer;
      onSuccess(offer);
    }, unexpectedCallbackAndFinish(), this.offerConstraints);
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
    }, unexpectedCallbackAndFinish());
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
    }, unexpectedCallbackAndFinish());
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
      unexpectedCallbackAndFinish("setLocalDescription should have failed."),
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
      onSuccess();
    }, unexpectedCallbackAndFinish());
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
      unexpectedCallbackAndFinish("setRemoteDescription should have failed."),
      function (err) {
        info(self + ": As expected, failed to set the remote description");
        onFailure(err);
    });
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

    this._pc.addIceCandidate(candidate, function () {
      info(self + ": Successfully added an ICE candidate");
      onSuccess();
    }, unexpectedCallbackAndFinish());
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
      unexpectedCallbackAndFinish("addIceCandidate should have failed."),
      function (err) {
        info(self + ": As expected, failed to add an ICE candidate");
        onFailure(err);
    }) ;
  },

  /**
   * Checks that we are getting the media we expect.
   *
   * @param {object} constraintsRemote
   *        The media constraints of the remote peer connection object
   */
  checkMedia : function PCW_checkMedia(constraintsRemote) {
    is(this._pc.localStreams.length, this.constraints.length,
       this + ' has ' + this.constraints.length + ' local streams');

    // TODO: change this when multiple incoming streams are allowed.
    is(this._pc.remoteStreams.length, 1,
       this + ' has ' + 1 + ' remote streams');
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
    } catch (e) {
      info(this + ": Failure in closing connection - " + e.message);
    }
  },

  /**
   * Returns the string representation of the object
   */
  toString : function PCW_toString() {
    return "PeerConnectionWrapper (" + this.label + ")";
  }
};
