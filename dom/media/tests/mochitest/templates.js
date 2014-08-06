/**
 * Default list of commands to execute for a PeerConnection test.
 */

var STABLE = "stable";
var HAVE_LOCAL_OFFER = "have-local-offer";
var HAVE_REMOTE_OFFER = "have-remote-offer";
var CLOSED = "closed";

const ICE_NEW = "new";

function deltaSeconds(date1, date2) {
  return (date2.getTime() - date1.getTime())/1000;
}

function dumpSdp(test) {
  if (typeof test._local_offer !== 'undefined') {
    dump("ERROR: SDP offer: " + test._local_offer.sdp.replace(/[\r]/g, ''));
  }
  if (typeof test._remote_answer !== 'undefined') {
    dump("ERROR: SDP answer: " + test._remote_answer.sdp.replace(/[\r]/g, ''));
  }

  if (typeof test.pcLocal.iceConnectionLog !== 'undefined') {
    dump("pcLocal ICE connection state log: " + test.pcLocal.iceConnectionLog + "\n");
  }
  if (typeof test.pcRemote.iceConnectionLog !== 'undefined') {
    dump("pcRemote ICE connection state log: " + test.pcRemote.iceConnectionLog + "\n");
  }

  if ((typeof test.pcLocal.setRemoteDescDate !== 'undefined') &&
    (typeof test.pcRemote.setLocalDescDate !== 'undefined')) {
    var delta = deltaSeconds(test.pcLocal.setRemoteDescDate, test.pcRemote.setLocalDescDate);
    dump("Delay between pcLocal.setRemote <-> pcRemote.setLocal: " + delta + "\n");
  }
  if ((typeof test.pcLocal.setRemoteDescDate !== 'undefined') &&
    (typeof test.pcLocal.setRemoteDescStableEventDate !== 'undefined')) {
    var delta = deltaSeconds(test.pcLocal.setRemoteDescDate, test.pcLocal.setRemoteDescStableEventDate);
    dump("Delay between pcLocal.setRemote <-> pcLocal.signalingStateStable: " + delta + "\n");
  }
  if ((typeof test.pcRemote.setLocalDescDate !== 'undefined') &&
    (typeof test.pcRemote.setLocalDescStableEventDate !== 'undefined')) {
    var delta = deltaSeconds(test.pcRemote.setLocalDescDate, test.pcRemote.setLocalDescStableEventDate);
    dump("Delay between pcRemote.setLocal <-> pcRemote.signalingStateStable: " + delta + "\n");
  }
}

var commandsPeerConnection = [
  [
    'PC_LOCAL_SETUP_ICE_LOGGER',
    function (test) {
      test.pcLocal.logIceConnectionState();
      test.next();
    }
  ],
  [
    'PC_REMOTE_SETUP_ICE_LOGGER',
    function (test) {
      test.pcRemote.logIceConnectionState();
      test.next();
    }
  ],
  [
    'PC_LOCAL_SETUP_SIGNALING_LOGGER',
    function (test) {
      test.pcLocal.logSignalingState();
      test.next();
    }
  ],
  [
    'PC_REMOTE_SETUP_SIGNALING_LOGGER',
    function (test) {
      test.pcRemote.logSignalingState();
      test.next();
    }
  ],
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
    'PC_LOCAL_CHECK_INITIAL_SIGNALINGSTATE',
    function (test) {
      is(test.pcLocal.signalingState, STABLE,
         "Initial local signalingState is 'stable'");
      test.next();
    }
  ],
  [
    'PC_REMOTE_CHECK_INITIAL_SIGNALINGSTATE',
    function (test) {
      is(test.pcRemote.signalingState, STABLE,
         "Initial remote signalingState is 'stable'");
      test.next();
    }
  ],
  [
    'PC_LOCAL_CHECK_INITIAL_ICE_STATE',
    function (test) {
      is(test.pcLocal.iceConnectionState, ICE_NEW,
        "Initial local ICE connection state is 'new'");
      test.next();
    }
  ],
  [
    'PC_REMOTE_CHECK_INITIAL_ICE_STATE',
    function (test) {
      is(test.pcRemote.iceConnectionState, ICE_NEW,
        "Initial remote ICE connection state is 'new'");
      test.next();
    }
  ],
  [
    'PC_LOCAL_CREATE_OFFER',
    function (test) {
      test.createOffer(test.pcLocal, function (offer) {
        is(test.pcLocal.signalingState, STABLE,
           "Local create offer does not change signaling state");
        if (!test.pcRemote) {
          send_message({"offer": test.pcLocal._last_offer,
                        "offer_constraints": test.pcLocal.constraints,
                        "offer_options": test.pcLocal.offerOptions});
          test._local_offer = test.pcLocal._last_offer;
          test._offer_constraints = test.pcLocal.constraints;
          test._offer_options = test.pcLocal.offerOptions;
        }
        test.next();
      });
    }
  ],
  [
    'PC_LOCAL_SET_LOCAL_DESCRIPTION',
    function (test) {
      test.setLocalDescription(test.pcLocal, test.pcLocal._last_offer, HAVE_LOCAL_OFFER, function () {
        is(test.pcLocal.signalingState, HAVE_LOCAL_OFFER,
           "signalingState after local setLocalDescription is 'have-local-offer'");
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_GET_OFFER',
    function (test) {
      if (test.pcLocal) {
        test._local_offer = test.pcLocal._last_offer;
        test._offer_constraints = test.pcLocal.constraints;
        test._offer_options = test.pcLocal.offerOptions;
        test.next();
      } else {
        wait_for_message().then(function(message) {
          ok("offer" in message, "Got an offer message");
          test._local_offer = new mozRTCSessionDescription(message.offer);
          test._offer_constraints = message.offer_constraints;
          test._offer_options = message.offer_options;
          test.next();
        });
      }
    }
  ],
  [
    'PC_REMOTE_SET_REMOTE_DESCRIPTION',
    function (test) {
      test.setRemoteDescription(test.pcRemote, test._local_offer, HAVE_REMOTE_OFFER, function () {
        is(test.pcRemote.signalingState, HAVE_REMOTE_OFFER,
           "signalingState after remote setRemoteDescription is 'have-remote-offer'");
        test.next();
      });
    }
  ],
  [
    'PC_LOCAL_SANE_LOCAL_SDP',
    function (test) {
      test.pcLocal.verifySdp(test.pcLocal.localDescription, "offer", test._offer_constraints, test._offer_options);
      test.next();
    }
  ],
  [
    'PC_REMOTE_SANE_REMOTE_SDP',
    function (test) {
      test.pcRemote.verifySdp(test.pcRemote.remoteDescription, "offer", test._offer_constraints, test._offer_options);
      test.next();
    }
  ],
  [
    'PC_REMOTE_CREATE_ANSWER',
    function (test) {
      test.createAnswer(test.pcRemote, function (answer) {
        is(test.pcRemote.signalingState, HAVE_REMOTE_OFFER,
           "Remote createAnswer does not change signaling state");
        if (!test.pcLocal) {
          send_message({"answer": test.pcRemote._last_answer,
                        "answer_constraints": test.pcRemote.constraints});
          test._remote_answer = test.pcRemote._last_answer;
          test._answer_constraints = test.pcRemote.constraints;
        }
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_CHECK_FOR_DUPLICATED_PORTS_IN_SDP',
    function (test) {
      var re = /a=candidate.* (UDP|TCP) [\d]+ ([\d\.]+) ([\d]+) typ host/g;

      function _sdpCandidatesIntoArray(sdp) {
        var regexArray = [];
        var resultArray = [];
        while ((regexArray = re.exec(sdp)) !== null) {
          info("regexArray: " + regexArray);
          if ((regexArray[1] === "TCP") && (regexArray[3] === "9")) {
            // As both sides can advertise TCP active connection on port 9 lets
            // ignore them all together
            info("Ignoring TCP candidate on port 9");
            continue;
          }
          const triple = regexArray[1] + ":" + regexArray[2] + ":" + regexArray[3];
          info("triple: " + triple);
          if (resultArray.indexOf(triple) !== -1) {
            dump("SDP: " + sdp.replace(/[\r]/g, '') + "\n");
            ok(false, "This Transport:IP:Port " + triple + " appears twice in the SDP above!");
          }
          resultArray.push(triple);
        }
        return resultArray;
      }

      const offerTriples = _sdpCandidatesIntoArray(test._local_offer.sdp);
      info("Offer ICE host candidates: " + JSON.stringify(offerTriples));

      const answerTriples = _sdpCandidatesIntoArray(test.pcRemote._last_answer.sdp);
      info("Answer ICE host candidates: " + JSON.stringify(answerTriples));

      for (var i=0; i< offerTriples.length; i++) {
        if (answerTriples.indexOf(offerTriples[i]) !== -1) {
          dump("SDP offer: " + test._local_offer.sdp.replace(/[\r]/g, '') + "\n");
          dump("SDP answer: " + test.pcRemote._last_answer.sdp.replace(/[\r]/g, '') + "\n");
          ok(false, "This IP:Port " + offerTriples[i] + " appears in SDP offer and answer!");
        }
      }

      test.next();
    }
  ],
  [
    'PC_REMOTE_SET_LOCAL_DESCRIPTION',
    function (test) {
      test.setLocalDescription(test.pcRemote, test.pcRemote._last_answer, STABLE, function () {
        is(test.pcRemote.signalingState, STABLE,
           "signalingState after remote setLocalDescription is 'stable'");
        test.next();
      });
    }
  ],
  [
    'PC_LOCAL_GET_ANSWER',
    function (test) {
      if (test.pcRemote) {
        test._remote_answer = test.pcRemote._last_answer;
        test._answer_constraints = test.pcRemote.constraints;
        test.next();
      } else {
        wait_for_message().then(function(message) {
          ok("answer" in message, "Got an answer message");
          test._remote_answer = new mozRTCSessionDescription(message.answer);
          test._answer_constraints = message.answer_constraints;
          test.next();
        });
      }
    }
  ],
  [
    'PC_LOCAL_SET_REMOTE_DESCRIPTION',
    function (test) {
      test.setRemoteDescription(test.pcLocal, test._remote_answer, STABLE, function () {
        is(test.pcLocal.signalingState, STABLE,
           "signalingState after local setRemoteDescription is 'stable'");
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_SANE_LOCAL_SDP',
    function (test) {
      test.pcRemote.verifySdp(test.pcRemote.localDescription, "answer", test._answer_constraints, test._offer_options);
      test.next();
    }
  ],
  [
    'PC_LOCAL_SANE_REMOTE_SDP',
    function (test) {
      test.pcLocal.verifySdp(test.pcLocal.remoteDescription, "answer", test._answer_constraints, test._offer_options);
      test.next();
    }
  ],
  [
    'PC_LOCAL_WAIT_FOR_ICE_CONNECTED',
    function (test) {
      var myTest = test;
      var myPc = myTest.pcLocal;

      function onIceConnectedSuccess () {
        info("pcLocal ICE connection state log: " + test.pcLocal.iceConnectionLog);
        ok(true, "pc_local: ICE switched to 'connected' state");
        myTest.next();
      };
      function onIceConnectedFailed () {
        dumpSdp(myTest);
        ok(false, "pc_local: ICE failed to switch to 'connected' state: " + myPc.iceConnectionState);
        myTest.next();
      };

      if (myPc.isIceConnected()) {
        info("pcLocal ICE connection state log: " + test.pcLocal.iceConnectionLog);
        ok(true, "pc_local: ICE is in connected state");
        myTest.next();
      } else if (myPc.isIceConnectionPending()) {
        myPc.waitForIceConnected(onIceConnectedSuccess, onIceConnectedFailed);
      } else {
        dumpSdp(myTest);
        ok(false, "pc_local: ICE is already in bad state: " + myPc.iceConnectionState);
        myTest.next();
      }
    }
  ],
  [
    'PC_REMOTE_WAIT_FOR_ICE_CONNECTED',
    function (test) {
      var myTest = test;
      var myPc = myTest.pcRemote;

      function onIceConnectedSuccess () {
        info("pcRemote ICE connection state log: " + test.pcRemote.iceConnectionLog);
        ok(true, "pc_remote: ICE switched to 'connected' state");
        myTest.next();
      };
      function onIceConnectedFailed () {
        dumpSdp(myTest);
        ok(false, "pc_remote: ICE failed to switch to 'connected' state: " + myPc.iceConnectionState);
        myTest.next();
      };

      if (myPc.isIceConnected()) {
        info("pcRemote ICE connection state log: " + test.pcRemote.iceConnectionLog);
        ok(true, "pc_remote: ICE is in connected state");
        myTest.next();
      } else if (myPc.isIceConnectionPending()) {
        myPc.waitForIceConnected(onIceConnectedSuccess, onIceConnectedFailed);
      } else {
        dumpSdp(myTest);
        ok(false, "pc_remote: ICE is already in bad state: " + myPc.iceConnectionState);
        myTest.next();
      }
    }
  ],
  [
    'PC_LOCAL_CHECK_MEDIA_TRACKS',
    function (test) {
      test.pcLocal.checkMediaTracks(test._answer_constraints, function () {
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_CHECK_MEDIA_TRACKS',
    function (test) {
      test.pcRemote.checkMediaTracks(test._offer_constraints, function () {
        test.next();
      });
    }
  ],
  [
    'PC_LOCAL_CHECK_MEDIA_FLOW_PRESENT',
    function (test) {
      test.pcLocal.checkMediaFlowPresent(function () {
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_CHECK_MEDIA_FLOW_PRESENT',
    function (test) {
      test.pcRemote.checkMediaFlowPresent(function () {
        test.next();
      });
    }
  ],
  [
    'PC_LOCAL_CHECK_STATS',
    function (test) {
      test.pcLocal.getStats(null, function(stats) {
        test.pcLocal.checkStats(stats);
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_CHECK_STATS',
    function (test) {
      test.pcRemote.getStats(null, function(stats) {
        test.pcRemote.checkStats(stats);
        test.next();
      });
    }
  ]
];


/**
 * Default list of commands to execute for a Datachannel test.
 */
var commandsDataChannel = [
  [
    'PC_LOCAL_SETUP_ICE_LOGGER',
    function (test) {
      test.pcLocal.logIceConnectionState();
      test.next();
    }
  ],
  [
    'PC_REMOTE_SETUP_ICE_LOGGER',
    function (test) {
      test.pcRemote.logIceConnectionState();
      test.next();
    }
  ],
  [
    'PC_LOCAL_SETUP_SIGNALING_LOGGER',
    function (test) {
      test.pcLocal.logSignalingState();
      test.next();
    }
  ],
  [
    'PC_REMOTE_SETUP_SIGNALING_LOGGER',
    function (test) {
      test.pcRemote.logSignalingState();
      test.next();
    }
  ],
  [
    'PC_LOCAL_GUM',
    function (test) {
      test.pcLocal.getAllUserMedia(function () {
        test.next();
      });
    }
  ],
  [
    'PC_LOCAL_INITIAL_SIGNALINGSTATE',
    function (test) {
      is(test.pcLocal.signalingState, STABLE,
         "Initial local signalingState is stable");
      test.next();
    }
  ],
  [
    'PC_LOCAL_CHECK_INITIAL_ICE_STATE',
    function (test) {
      is(test.pcLocal.iceConnectionState, ICE_NEW,
        "Initial local ICE connection state is 'new'");
      test.next();
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
    'PC_REMOTE_INITIAL_SIGNALINGSTATE',
    function (test) {
      is(test.pcRemote.signalingState, STABLE,
         "Initial remote signalingState is stable");
      test.next();
    }
  ],
  [
    'PC_REMOTE_CHECK_INITIAL_ICE_STATE',
    function (test) {
      is(test.pcRemote.iceConnectionState, ICE_NEW,
        "Initial remote ICE connection state is 'new'");
      test.next();
    }
  ],
  [
    'PC_LOCAL_CREATE_DATA_CHANNEL',
    function (test) {
      var channel = test.pcLocal.createDataChannel({});

      is(channel.binaryType, "blob", channel + " is of binary type 'blob'");
      is(channel.readyState, "connecting", channel + " is in state: 'connecting'");

      is(test.pcLocal.signalingState, STABLE,
         "Create datachannel does not change signaling state");

      test.next();
    }
  ],
  [
    'PC_LOCAL_CREATE_OFFER',
    function (test) {
      test.pcLocal.createOffer(function (offer) {
        is(test.pcLocal.signalingState, STABLE,
           "Local create offer does not change signaling state");
        ok(!offer.sdp.contains(LOOPBACK_ADDR),
           "loopback interface is absent from SDP");
        ok(offer.sdp.contains("m=application"),
           "m=application is contained in the SDP");
        test.next();
      });
    }
  ],
  [
    'PC_LOCAL_SET_LOCAL_DESCRIPTION',
    function (test) {
      test.setLocalDescription(test.pcLocal, test.pcLocal._last_offer, HAVE_LOCAL_OFFER,
        function () {
        is(test.pcLocal.signalingState, HAVE_LOCAL_OFFER,
           "signalingState after local setLocalDescription is 'have-local-offer'");
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_SET_REMOTE_DESCRIPTION',
    function (test) {
      test.setRemoteDescription(test.pcRemote, test.pcLocal._last_offer, HAVE_REMOTE_OFFER,
        function () {
        is(test.pcRemote.signalingState, HAVE_REMOTE_OFFER,
           "signalingState after remote setRemoteDescription is 'have-remote-offer'");
        test.next();
      });
    }
  ],
  [
    'PC_LOCAL_SANE_LOCAL_SDP',
    function (test) {
      test.pcLocal.verifySdp(test.pcLocal.localDescription, "offer", test.pcLocal.constraints);
      test.next();
    }
  ],
  [
    'PC_REMOTE_SANE_REMOTE_SDP',
    function (test) {
      test.pcRemote.verifySdp(test.pcRemote.remoteDescription, "offer", test.pcLocal.constraints);
      test.next();
    }
  ],
  [
    'PC_REMOTE_CREATE_ANSWER',
    function (test) {
      test.createAnswer(test.pcRemote, function (answer) {
        is(test.pcRemote.signalingState, HAVE_REMOTE_OFFER,
           "Remote create offer does not change signaling state");
        ok(!answer.sdp.contains(LOOPBACK_ADDR),
           "loopback interface is absent in SDP");
        test.next();
      });
    }
  ],
  [
    'PC_LOCAL_SETUP_DATA_CHANNEL_CALLBACK',
    function (test) {
      test.waitForInitialDataChannel(test.pcLocal, function () {
        ok(true, test.pcLocal + " dataChannels[0] switched to 'open'");
      },
      // At this point a timeout failure will be of no value
      null);
      test.next();
    }
  ],
  [
    'PC_REMOTE_SETUP_DATA_CHANNEL_CALLBACK',
    function (test) {
      test.waitForInitialDataChannel(test.pcRemote, function () {
        ok(true, test.pcRemote + " dataChannels[0] switched to 'open'");
      },
      // At this point a timeout failure will be of no value
      null);
      test.next();
    }
  ],
  [
    'PC_REMOTE_SET_LOCAL_DESCRIPTION',
    function (test) {
      test.setLocalDescription(test.pcRemote, test.pcRemote._last_answer, STABLE,
        function () {
          is(test.pcRemote.signalingState, STABLE,
             "signalingState after remote setLocalDescription is 'stable'");
          test.next();
        }
      );
    }
  ],
  [
    'PC_LOCAL_SET_REMOTE_DESCRIPTION',
    function (test) {
      test.setRemoteDescription(test.pcLocal, test.pcRemote._last_answer, STABLE,
        function () {
        is(test.pcLocal.signalingState, STABLE,
           "signalingState after local setRemoteDescription is 'stable'");
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_SANE_LOCAL_SDP',
    function (test) {
      test.pcRemote.verifySdp(test.pcRemote.localDescription, "answer", test.pcRemote.constraints);
      test.next();
    }
  ],
  [
    'PC_LOCAL_SANE_REMOTE_SDP',
    function (test) {
      test.pcLocal.verifySdp(test.pcLocal.remoteDescription, "answer", test.pcRemote.constraints);
      test.next();
    }
  ],
  [
    'PC_LOCAL_WAIT_FOR_ICE_CONNECTED',
    function (test) {
      var myTest = test;
      var myPc = myTest.pcLocal;

      function onIceConnectedSuccess () {
        info("pcLocal ICE connection state log: " + test.pcLocal.iceConnectionLog);
        ok(true, "pc_local: ICE switched to 'connected' state");
        myTest.next();
      };
      function onIceConnectedFailed () {
        dumpSdp(myTest);
        ok(false, "pc_local: ICE failed to switch to 'connected' state: " + myPc.iceConnectionState);
        myTest.next();
      };

      if (myPc.isIceConnected()) {
        info("pcLocal ICE connection state log: " + test.pcLocal.iceConnectionLog);
        ok(true, "pc_local: ICE is in connected state");
        myTest.next();
      } else if (myPc.isIceConnectionPending()) {
        myPc.waitForIceConnected(onIceConnectedSuccess, onIceConnectedFailed);
      } else {
        dumpSdp(myTest);
        ok(false, "pc_local: ICE is already in bad state: " + myPc.iceConnectionState);
        myTest.next();
      }
    }
  ],
  [
    'PC_REMOTE_WAIT_FOR_ICE_CONNECTED',
    function (test) {
      var myTest = test;
      var myPc = myTest.pcRemote;

      function onIceConnectedSuccess () {
        info("pcRemote ICE connection state log: " + test.pcRemote.iceConnectionLog);
        ok(true, "pc_remote: ICE switched to 'connected' state");
        myTest.next();
      };
      function onIceConnectedFailed () {
        dumpSdp(myTest);
        ok(false, "pc_remote: ICE failed to switch to 'connected' state: " + myPc.iceConnectionState);
        myTest.next();
      };

      if (myPc.isIceConnected()) {
        info("pcRemote ICE connection state log: " + test.pcRemote.iceConnectionLog);
        ok(true, "pc_remote: ICE is in connected state");
        myTest.next();
      } else if (myPc.isIceConnectionPending()) {
        myPc.waitForIceConnected(onIceConnectedSuccess, onIceConnectedFailed);
      } else {
        dumpSdp(myTest);
        ok(false, "pc_remote: ICE is already in bad state: " + myPc.iceConnectionState);
        myTest.next();
      }
    }
  ],
  [
    'PC_LOCAL_VERIFY_DATA_CHANNEL_STATE',
    function (test) {
      test.waitForInitialDataChannel(test.pcLocal, function() {
        test.next();
      }, function() {
        ok(false, test.pcLocal + " initial dataChannels[0] failed to switch to 'open'");
        //TODO: use stopAndExit() once bug 1019323 has landed
        unexpectedEventAndFinish(this, 'timeout')
        // to prevent test framework timeouts
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_VERIFY_DATA_CHANNEL_STATE',
    function (test) {
      test.waitForInitialDataChannel(test.pcRemote, function() {
        test.next();
      }, function() {
        ok(false, test.pcRemote + " initial dataChannels[0] failed to switch to 'open'");
        //TODO: use stopAndExit() once bug 1019323 has landed
        unexpectedEventAndFinish(this, 'timeout');
        // to prevent test framework timeouts
        test.next();
      });
    }
  ],
  [
    'PC_LOCAL_CHECK_MEDIA_TRACKS',
    function (test) {
      test.pcLocal.checkMediaTracks(test.pcRemote.constraints, function () {
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_CHECK_MEDIA_TRACKS',
    function (test) {
      test.pcRemote.checkMediaTracks(test.pcLocal.constraints, function () {
        test.next();
      });
    }
  ],
  [
    'PC_LOCAL_CHECK_MEDIA_FLOW_PRESENT',
    function (test) {
      test.pcLocal.checkMediaFlowPresent(function () {
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_CHECK_MEDIA_FLOW_PRESENT',
    function (test) {
      test.pcRemote.checkMediaFlowPresent(function () {
        test.next();
      });
    }
  ],
  [
    'SEND_MESSAGE',
    function (test) {
      var message = "Lorem ipsum dolor sit amet";

      test.send(message, function (channel, data) {
        is(data, message, "Message correctly transmitted from pcLocal to pcRemote.");

        test.next();
      });
    }
  ],
  [
    'SEND_BLOB',
    function (test) {
      var contents = ["At vero eos et accusam et justo duo dolores et ea rebum."];
      var blob = new Blob(contents, { "type" : "text/plain" });

      test.send(blob, function (channel, data) {
        ok(data instanceof Blob, "Received data is of instance Blob");
        is(data.size, blob.size, "Received data has the correct size.");

        getBlobContent(data, function (recv_contents) {
          is(recv_contents, contents, "Received data has the correct content.");

          test.next();
        });
      });
    }
  ],
  [
    'CREATE_SECOND_DATA_CHANNEL',
    function (test) {
      test.createDataChannel({ }, function (sourceChannel, targetChannel) {
        is(sourceChannel.readyState, "open", sourceChannel + " is in state: 'open'");
        is(targetChannel.readyState, "open", targetChannel + " is in state: 'open'");

        is(targetChannel.binaryType, "blob", targetChannel + " is of binary type 'blob'");
        is(targetChannel.readyState, "open", targetChannel + " is in state: 'open'");

        test.next();
      });
    }
  ],
  [
    'SEND_MESSAGE_THROUGH_LAST_OPENED_CHANNEL',
    function (test) {
      var channels = test.pcRemote.dataChannels;
      var message = "Lorem ipsum dolor sit amet";

      test.send(message, function (channel, data) {
        is(channels.indexOf(channel), channels.length - 1, "Last channel used");
        is(data, message, "Received message has the correct content.");

        test.next();
      });
    }
  ],
  [
    'SEND_MESSAGE_THROUGH_FIRST_CHANNEL',
    function (test) {
      var message = "Message through 1st channel";
      var options = {
        sourceChannel: test.pcLocal.dataChannels[0],
        targetChannel: test.pcRemote.dataChannels[0]
      };

      test.send(message, function (channel, data) {
        is(test.pcRemote.dataChannels.indexOf(channel), 0, "1st channel used");
        is(data, message, "Received message has the correct content.");

        test.next();
      }, options);
    }
  ],
  [
    'SEND_MESSAGE_BACK_THROUGH_FIRST_CHANNEL',
    function (test) {
      var message = "Return a message also through 1st channel";
      var options = {
        sourceChannel: test.pcRemote.dataChannels[0],
        targetChannel: test.pcLocal.dataChannels[0]
      };

      test.send(message, function (channel, data) {
        is(test.pcLocal.dataChannels.indexOf(channel), 0, "1st channel used");
        is(data, message, "Return message has the correct content.");

        test.next();
      }, options);
    }
  ],
  [
    'CREATE_NEGOTIATED_DATA_CHANNEL',
    function (test) {
      var options = {negotiated:true, id: 5, protocol:"foo/bar", ordered:false,
        maxRetransmits:500};
      test.createDataChannel(options, function (sourceChannel2, targetChannel2) {
        is(sourceChannel2.readyState, "open", sourceChannel2 + " is in state: 'open'");
        is(targetChannel2.readyState, "open", targetChannel2 + " is in state: 'open'");

        is(targetChannel2.binaryType, "blob", targetChannel2 + " is of binary type 'blob'");
        is(targetChannel2.readyState, "open", targetChannel2 + " is in state: 'open'");

        if (options.id != undefined) {
          is(sourceChannel2.id, options.id, sourceChannel2 + " id is:" + sourceChannel2.id);
        }
        else {
          options.id = sourceChannel2.id;
        }
        var reliable = !options.ordered ? false : (options.maxRetransmits || options.maxRetransmitTime);
        is(sourceChannel2.protocol, options.protocol, sourceChannel2 + " protocol is:" + sourceChannel2.protocol);
        is(sourceChannel2.reliable, reliable, sourceChannel2 + " reliable is:" + sourceChannel2.reliable);
/*
  These aren't exposed by IDL yet
        is(sourceChannel2.ordered, options.ordered, sourceChannel2 + " ordered is:" + sourceChannel2.ordered);
        is(sourceChannel2.maxRetransmits, options.maxRetransmits, sourceChannel2 + " maxRetransmits is:" +
	   sourceChannel2.maxRetransmits);
        is(sourceChannel2.maxRetransmitTime, options.maxRetransmitTime, sourceChannel2 + " maxRetransmitTime is:" +
	   sourceChannel2.maxRetransmitTime);
*/

        is(targetChannel2.id, options.id, targetChannel2 + " id is:" + targetChannel2.id);
        is(targetChannel2.protocol, options.protocol, targetChannel2 + " protocol is:" + targetChannel2.protocol);
        is(targetChannel2.reliable, reliable, targetChannel2 + " reliable is:" + targetChannel2.reliable);
/*
  These aren't exposed by IDL yet
       is(targetChannel2.ordered, options.ordered, targetChannel2 + " ordered is:" + targetChannel2.ordered);
        is(targetChannel2.maxRetransmits, options.maxRetransmits, targetChannel2 + " maxRetransmits is:" +
	   targetChannel2.maxRetransmits);
        is(targetChannel2.maxRetransmitTime, options.maxRetransmitTime, targetChannel2 + " maxRetransmitTime is:" +
	   targetChannel2.maxRetransmitTime);
*/

        test.next();
      });
    }
  ],
  [
    'SEND_MESSAGE_THROUGH_LAST_OPENED_CHANNEL2',
    function (test) {
      var channels = test.pcRemote.dataChannels;
      var message = "Lorem ipsum dolor sit amet";

      test.send(message, function (channel, data) {
        is(channels.indexOf(channel), channels.length - 1, "Last channel used");
        is(data, message, "Received message has the correct content.");

        test.next();
      });
    }
  ]
];

