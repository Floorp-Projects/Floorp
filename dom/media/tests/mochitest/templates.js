/**
 * Default list of commands to execute for a PeerConnection test.
 */

const STABLE = "stable";
const HAVE_LOCAL_OFFER = "have-local-offer";
const HAVE_REMOTE_OFFER = "have-remote-offer";
const CLOSED = "closed";

const ICE_NEW = "new";
const GATH_NEW = "new";
const GATH_GATH = "gathering";
const GATH_COMPLETE = "complete"

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

  if ((test.pcLocal) && (typeof test.pcLocal._local_ice_candidates !== 'undefined')) {
    dump("pcLocal._local_ice_candidates: " + JSON.stringify(test.pcLocal._local_ice_candidates) + "\n");
    dump("pcLocal._remote_ice_candidates: " + JSON.stringify(test.pcLocal._remote_ice_candidates) + "\n");
    dump("pcLocal._ice_candidates_to_add: " + JSON.stringify(test.pcLocal._ice_candidates_to_add) + "\n");
  }
  if ((test.pcRemote) && (typeof test.pcRemote._local_ice_candidates !== 'undefined')) {
    dump("pcRemote._local_ice_candidates: " + JSON.stringify(test.pcRemote._local_ice_candidates) + "\n");
    dump("pcRemote._remote_ice_candidates: " + JSON.stringify(test.pcRemote._remote_ice_candidates) + "\n");
    dump("pcRemote._ice_candidates_to_add: " + JSON.stringify(test.pcRemote._ice_candidates_to_add) + "\n");
  }

  if ((test.pcLocal) && (typeof test.pcLocal.iceConnectionLog !== 'undefined')) {
    dump("pcLocal ICE connection state log: " + test.pcLocal.iceConnectionLog + "\n");
  }
  if ((test.pcRemote) && (typeof test.pcRemote.iceConnectionLog !== 'undefined')) {
    dump("pcRemote ICE connection state log: " + test.pcRemote.iceConnectionLog + "\n");
  }

  if ((test.pcLocal) && (test.pcRemote) &&
    (typeof test.pcLocal.setRemoteDescDate !== 'undefined') &&
    (typeof test.pcRemote.setLocalDescDate !== 'undefined')) {
    var delta = deltaSeconds(test.pcLocal.setRemoteDescDate, test.pcRemote.setLocalDescDate);
    dump("Delay between pcLocal.setRemote <-> pcRemote.setLocal: " + delta + "\n");
  }
  if ((test.pcLocal) && (test.pcRemote) &&
    (typeof test.pcLocal.setRemoteDescDate !== 'undefined') &&
    (typeof test.pcLocal.setRemoteDescStableEventDate !== 'undefined')) {
    var delta = deltaSeconds(test.pcLocal.setRemoteDescDate, test.pcLocal.setRemoteDescStableEventDate);
    dump("Delay between pcLocal.setRemote <-> pcLocal.signalingStateStable: " + delta + "\n");
  }
  if ((test.pcLocal) && (test.pcRemote) &&
    (typeof test.pcRemote.setLocalDescDate !== 'undefined') &&
    (typeof test.pcRemote.setLocalDescStableEventDate !== 'undefined')) {
    var delta = deltaSeconds(test.pcRemote.setLocalDescDate, test.pcRemote.setLocalDescStableEventDate);
    dump("Delay between pcRemote.setLocal <-> pcRemote.signalingStateStable: " + delta + "\n");
  }
}

var commandsPeerConnection = [
  [
    'PC_SETUP_SIGNALING_CLIENT',
    function (test) {
      if (test.steeplechase) {
        test.setTimeout(30000);
        test.setupSignalingClient();
        test.registerSignalingCallback("ice_candidate", function (message) {
          var pc = test.pcRemote ? test.pcRemote : test.pcLocal;
          pc.storeOrAddIceCandidate(new mozRTCIceCandidate(message.ice_candidate));
        });
        test.registerSignalingCallback("end_of_trickle_ice", function (message) {
          test.signalingMessagesFinished();
        });
      }
      test.next();
    }
  ],
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
      test.pcLocal.getAllUserMedia(test.pcLocal.constraints, function () {
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_GUM',
    function (test) {
      test.pcRemote.getAllUserMedia(test.pcRemote.constraints, function () {
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
    'PC_LOCAL_SETUP_ICE_HANDLER',
    function (test) {
      test.pcLocal.setupIceCandidateHandler(test);
      test.next();
    }
  ],
  [
    'PC_REMOTE_SETUP_ICE_HANDLER',
    function (test) {
      test.pcRemote.setupIceCandidateHandler(test);
      test.next();
    }
  ],
  [
    'PC_LOCAL_CREATE_OFFER',
    function (test) {
      test.createOffer(test.pcLocal, function (offer) {
        is(test.pcLocal.signalingState, STABLE,
           "Local create offer does not change signaling state");
        test.next();
      });
    }
  ],
  [
    'PC_LOCAL_STEEPLECHASE_SIGNAL_OFFER',
    function (test) {
      if (test.steeplechase) {
        send_message({"type": "offer",
          "offer": test.originalOffer,
          "offer_constraints": test.pcLocal.constraints,
          "offer_options": test.pcLocal.offerOptions});
        test._local_offer = test.originalOffer;
        test._offer_constraints = test.pcLocal.constraints;
        test._offer_options = test.pcLocal.offerOptions;
      }
      test.next();
    }
  ],
  [
    'PC_LOCAL_SET_LOCAL_DESCRIPTION',
    function (test) {
      test.setLocalDescription(test.pcLocal, test.originalOffer, HAVE_LOCAL_OFFER, function () {
        is(test.pcLocal.signalingState, HAVE_LOCAL_OFFER,
           "signalingState after local setLocalDescription is 'have-local-offer'");
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_GET_OFFER',
    function (test) {
      if (!test.steeplechase) {
        test._local_offer = test.originalOffer;
        test._offer_constraints = test.pcLocal.constraints;
        test._offer_options = test.pcLocal.offerOptions;
        test.next();
      } else {
        test.getSignalingMessage("offer", function (message) {
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
      test.pcLocal.verifySdp(test._local_offer, "offer",
        test._offer_constraints, test._offer_options,
        function(trickle) {
          test.pcLocal.localRequiresTrickleIce = trickle;
        });
      test.next();
    }
  ],
  [
    'PC_REMOTE_SANE_REMOTE_SDP',
    function (test) {
      test.pcRemote.verifySdp(test._local_offer, "offer",
        test._offer_constraints, test._offer_options,
        function (trickle) {
          test.pcRemote.remoteRequiresTrickleIce = trickle;
        });
      test.next();
    }
  ],
  [
    'PC_REMOTE_CREATE_ANSWER',
    function (test) {
      test.createAnswer(test.pcRemote, function (answer) {
        is(test.pcRemote.signalingState, HAVE_REMOTE_OFFER,
           "Remote createAnswer does not change signaling state");
        if (test.steeplechase) {
          send_message({"type": "answer",
                        "answer": test.originalAnswer,
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

      const answerTriples = _sdpCandidatesIntoArray(test.originalAnswer.sdp);
      info("Answer ICE host candidates: " + JSON.stringify(answerTriples));

      for (var i=0; i< offerTriples.length; i++) {
        if (answerTriples.indexOf(offerTriples[i]) !== -1) {
          dump("SDP offer: " + test._local_offer.sdp.replace(/[\r]/g, '') + "\n");
          dump("SDP answer: " + test.originalAnswer.sdp.replace(/[\r]/g, '') + "\n");
          ok(false, "This IP:Port " + offerTriples[i] + " appears in SDP offer and answer!");
        }
      }

      test.next();
    }
  ],
  [
    'PC_REMOTE_SET_LOCAL_DESCRIPTION',
    function (test) {
      test.setLocalDescription(test.pcRemote, test.originalAnswer, STABLE,
        function () {
          is(test.pcRemote.signalingState, STABLE,
            "signalingState after remote setLocalDescription is 'stable'");
          test.next();
        }
      );
    }
  ],
  [
    'PC_LOCAL_GET_ANSWER',
    function (test) {
      if (!test.steeplechase) {
        test._remote_answer = test.originalAnswer;
        test._answer_constraints = test.pcRemote.constraints;
        test.next();
      } else {
        test.getSignalingMessage("answer", function (message) {
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
      test.setRemoteDescription(test.pcLocal, test._remote_answer, STABLE,
        function () {
          is(test.pcLocal.signalingState, STABLE,
            "signalingState after local setRemoteDescription is 'stable'");
          test.next();
        }
      );
    }
  ],
  [
    'PC_REMOTE_SANE_LOCAL_SDP',
    function (test) {
      test.pcRemote.verifySdp(test._remote_answer, "answer",
        test._offer_constraints, test._offer_options,
        function (trickle) {
          test.pcRemote.localRequiresTrickleIce = trickle;
        });
      test.next();
    }
  ],
  [
    'PC_LOCAL_SANE_REMOTE_SDP',
    function (test) {
      test.pcLocal.verifySdp(test._remote_answer, "answer",
        test._offer_constraints, test._offer_options,
        function (trickle) {
          test.pcLocal.remoteRequiresTrickleIce = trickle;
        });
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
    'PC_LOCAL_VERIFY_ICE_GATHERING',
    function (test) {
      if (test.pcLocal.localRequiresTrickleIce) {
        ok(test.pcLocal._local_ice_candidates.length > 0, "Received local trickle ICE candidates");
      }
      isnot(test.pcLocal._pc.iceGatheringState, GATH_NEW, "ICE gathering state is not 'new'");
      test.next();
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
    'PC_REMOTE_VERIFY_ICE_GATHERING',
    function (test) {
      if (test.pcRemote.localRequiresTrickleIce) {
        ok(test.pcRemote._local_ice_candidates.length > 0, "Received local trickle ICE candidates");
      }
      isnot(test.pcRemote._pc.iceGatheringState, GATH_NEW, "ICE gathering state is not 'new'");
      test.next();
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
    'PC_LOCAL_CHECK_MSID',
    function (test) {
      test.pcLocal.checkMsids();
      test.next();
    }
  ],
  [
    'PC_REMOTE_CHECK_MSID',
    function (test) {
      test.pcRemote.checkMsids();
      test.next();
    }
  ],
  [
    'PC_LOCAL_CHECK_STATS',
    function (test) {
      test.pcLocal.getStats(null, function(stats) {
        test.pcLocal.checkStats(stats, test.steeplechase);
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_CHECK_STATS',
    function (test) {
      test.pcRemote.getStats(null, function(stats) {
        test.pcRemote.checkStats(stats, test.steeplechase);
        test.next();
      });
    }
  ],
  [
    'PC_LOCAL_CHECK_ICE_CONNECTION_TYPE',
    function (test) {
      test.pcLocal.getStats(null, function(stats) {
        test.pcLocal.checkStatsIceConnectionType(stats);
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_CHECK_ICE_CONNECTION_TYPE',
    function (test) {
      test.pcRemote.getStats(null, function(stats) {
        test.pcRemote.checkStatsIceConnectionType(stats);
        test.next();
      });
    }
  ],
  [
    'PC_LOCAL_CHECK_ICE_CONNECTIONS',
    function (test) {
      test.pcLocal.getStats(null, function(stats) {
        test.pcLocal.checkStatsIceConnections(stats,
                                              test._offer_constraints,
                                              test._offer_options,
                                              test._remote_answer);
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_CHECK_ICE_CONNECTIONS',
    function (test) {
      test.pcRemote.getStats(null, function(stats) {
        test.pcRemote.checkStatsIceConnections(stats,
                                               test._offer_constraints,
                                               test._offer_options,
                                               test.originalAnswer);
        test.next();
      });
    }
  ],
  [
    'PC_LOCAL_CHECK_GETSTATS_AUDIOTRACK_OUTBOUND',
    function (test) {
      var pc = test.pcLocal;
      var stream = pc._pc.getLocalStreams()[0];
      var track = stream && stream.getAudioTracks()[0];
      if (track) {
        var msg = "pcLocal.HasStat outbound audio rtp ";
        pc.getStats(track, function(stats) {
          ok(pc.hasStat(stats,
                        { type:"outboundrtp", isRemote:false, mediaType:"audio" }),
             msg + "1");
          ok(!pc.hasStat(stats, { type:"inboundrtp", isRemote:false }), msg + "2");
          ok(!pc.hasStat(stats, { mediaType:"video" }), msg + "3");
          test.next();
        });
      } else {
        test.next();
      }
    }
  ],
  [
    'PC_LOCAL_CHECK_GETSTATS_VIDEOTRACK_OUTBOUND',
    function (test) {
      var pc = test.pcLocal;
      var stream = pc._pc.getLocalStreams()[0];
      var track = stream && stream.getVideoTracks()[0];
      if (track) {
        var msg = "pcLocal.HasStat outbound video rtp ";
        pc.getStats(track, function(stats) {
          ok(pc.hasStat(stats,
                        { type:"outboundrtp", isRemote:false, mediaType:"video" }),
             msg + "1");
          ok(!pc.hasStat(stats, { type:"inboundrtp", isRemote:false }), msg + "2");
          ok(!pc.hasStat(stats, { mediaType:"audio" }), msg + "3");
          test.next();
        });
      } else {
        test.next();
      }
    }
  ],
  [
    'PC_LOCAL_CHECK_GETSTATS_AUDIOTRACK_INBOUND',
    function (test) {
      var pc = test.pcLocal;
      var stream = pc._pc.getRemoteStreams()[0];
      var track = stream && stream.getAudioTracks()[0];
      if (track) {
        var msg = "pcLocal.HasStat inbound audio rtp ";
        pc.getStats(track, function(stats) {
          ok(pc.hasStat(stats,
                        { type:"inboundrtp", isRemote:false, mediaType:"audio" }),
             msg + "1");
          ok(!pc.hasStat(stats, { type:"outboundrtp", isRemote:false }), msg + "2");
          ok(!pc.hasStat(stats, { mediaType:"video" }), msg + "3");
          test.next();
        });
      } else {
        test.next();
      }
    }
  ],
  [
    'PC_LOCAL_CHECK_GETSTATS_VIDEOTRACK_INBOUND',
    function (test) {
      var pc = test.pcLocal;
      var stream = pc._pc.getRemoteStreams()[0];
      var track = stream && stream.getVideoTracks()[0];
      if (track) {
        var msg = "pcLocal.HasStat inbound video rtp ";
        pc.getStats(track, function(stats) {
          ok(pc.hasStat(stats,
                        { type:"inboundrtp", isRemote:false, mediaType:"video" }),
             msg + "1");
          ok(!pc.hasStat(stats, { type:"outboundrtp", isRemote:false }), msg + "2");
          ok(!pc.hasStat(stats, { mediaType:"audio" }), msg + "3");
          test.next();
        });
      } else {
        test.next();
      }
    }
  ],
  [
    'PC_REMOTE_CHECK_GETSTATS_AUDIOTRACK_OUTBOUND',
    function (test) {
      var pc = test.pcRemote;
      var stream = pc._pc.getLocalStreams()[0];
      var track = stream && stream.getAudioTracks()[0];
      if (track) {
        var msg = "pcRemote.HasStat outbound audio rtp ";
        pc.getStats(track, function(stats) {
          ok(pc.hasStat(stats,
                        { type:"outboundrtp", isRemote:false, mediaType:"audio" }),
             msg + "1");
          ok(!pc.hasStat(stats, { type:"inboundrtp", isRemote:false }), msg + "2");
          ok(!pc.hasStat(stats, { mediaType:"video" }), msg + "3");
          test.next();
        });
      } else {
        test.next();
      }
    }
  ],
  [
    'PC_REMOTE_CHECK_GETSTATS_VIDEOTRACK_OUTBOUND',
    function (test) {
      var pc = test.pcRemote;
      var stream = pc._pc.getLocalStreams()[0];
      var track = stream && stream.getVideoTracks()[0];
      if (track) {
        var msg = "pcRemote.HasStat outbound audio rtp ";
        pc.getStats(track, function(stats) {
          ok(pc.hasStat(stats,
                        { type:"outboundrtp", isRemote:false, mediaType:"video" }),
             msg + "1");
          ok(!pc.hasStat(stats, { type:"inboundrtp", isRemote:false }), msg + "2");
          ok(!pc.hasStat(stats, { mediaType:"audio" }), msg + "3");
          test.next();
        });
      } else {
        test.next();
      }
    }
  ],
  [
    'PC_REMOTE_CHECK_GETSTATS_AUDIOTRACK_INBOUND',
    function (test) {
      var pc = test.pcRemote;
      var stream = pc._pc.getRemoteStreams()[0];
      var track = stream && stream.getAudioTracks()[0];
      if (track) {
        var msg = "pcRemote.HasStat inbound audio rtp ";
        pc.getStats(track, function(stats) {
          ok(pc.hasStat(stats,
                        { type:"inboundrtp", isRemote:false, mediaType:"audio" }),
             msg + "1");
          ok(!pc.hasStat(stats, { type:"outboundrtp", isRemote:false }), msg + "2");
          ok(!pc.hasStat(stats, { mediaType:"video" }), msg + "3");
          test.next();
        });
      } else {
        test.next();
      }
    }
  ],
  [
    'PC_REMOTE_CHECK_GETSTATS_VIDEOTRACK_INBOUND',
    function (test) {
      var pc = test.pcRemote;
      var stream = pc._pc.getRemoteStreams()[0];
      var track = stream && stream.getVideoTracks()[0];
      if (track) {
        var msg = "pcRemote.HasStat inbound video rtp ";
        pc.getStats(track, function(stats) {
          ok(pc.hasStat(stats,
                        { type:"inboundrtp", isRemote:false, mediaType:"video" }),
             msg + "1");
          ok(!pc.hasStat(stats, { type:"outboundrtp", isRemote:false }), msg + "2");
          ok(!pc.hasStat(stats, { mediaType:"audio" }), msg + "3");
          test.next();
        });
      } else {
        test.next();
      }
    }
  ]
];

