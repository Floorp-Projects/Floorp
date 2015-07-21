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

function waitForIceConnected(test, pc) {
  if (!pc.iceCheckingRestartExpected) {
    if (pc.isIceConnected()) {
      info(pc + ": ICE connection state log: " + pc.iceConnectionLog);
      ok(true, pc + ": ICE is in connected state");
      return Promise.resolve();
    }

    if (!pc.isIceConnectionPending()) {
      dumpSdp(test);
      var details = pc + ": ICE is already in bad state: " + pc.iceConnectionState;
      ok(false, details);
      return Promise.reject(new Error(details));
    }
  }

  return pc.waitForIceConnected()
    .then(() => {
      info(pc + ": ICE connection state log: " + pc.iceConnectionLog);
      ok(pc.isIceConnected(), pc + ": ICE switched to 'connected' state");
    });
}

// We need to verify that at least one candidate has been (or will be) gathered.
function waitForAnIceCandidate(pc) {
  return new Promise(resolve => {
    if (!pc.localRequiresTrickleIce ||
        pc._local_ice_candidates.length > 0) {
      resolve();
    } else {
      // In some circumstances, especially when both PCs are on the same
      // browser, even though we are connected, the connection can be
      // established without receiving a single candidate from one or other
      // peer.  So we wait for at least one...
      pc._pc.addEventListener('icecandidate', resolve);
    }
  }).then(() => {
    ok(pc._local_ice_candidates.length > 0,
       pc + " received local trickle ICE candidates");
    isnot(pc._pc.iceGatheringState, GATH_NEW,
          pc + " ICE gathering state is not 'new'");
  });
}

function checkTrackStats(pc, rtpSenderOrReceiver, outbound) {
  var track = rtpSenderOrReceiver.track;
  var audio = (track.kind == "audio");
  var msg = pc + " stats " + (outbound ? "outbound " : "inbound ") +
      (audio ? "audio" : "video") + " rtp track id " + track.id;
  return pc.getStats(track).then(stats => {
    ok(pc.hasStat(stats, {
      type: outbound ? "outboundrtp" : "inboundrtp",
      isRemote: false,
      mediaType: audio ? "audio" : "video"
    }), msg + " - found expected stats");
    ok(!pc.hasStat(stats, {
      type: outbound ? "inboundrtp" : "outboundrtp",
      isRemote: false
    }), msg + " - did not find extra stats with wrong direction");
    ok(!pc.hasStat(stats, {
      mediaType: audio ? "video" : "audio"
    }), msg + " - did not find extra stats with wrong media type");
  });
}

var checkAllTrackStats = pc => {
  return Promise.all([].concat(
    pc._pc.getSenders().map(sender => checkTrackStats(pc, sender, true)),
    pc._pc.getReceivers().map(receiver => checkTrackStats(pc, receiver, false))));
}

// Commands run once at the beginning of each test, even when performing a
// renegotiation test.
var commandsPeerConnectionInitial = [
  function PC_SETUP_SIGNALING_CLIENT(test) {
    if (test.steeplechase) {
      test.setupSignalingClient();
      test.registerSignalingCallback("ice_candidate", function (message) {
        var pc = test.pcRemote ? test.pcRemote : test.pcLocal;
        pc.storeOrAddIceCandidate(new mozRTCIceCandidate(message.ice_candidate));
      });
      test.registerSignalingCallback("end_of_trickle_ice", function (message) {
        test.signalingMessagesFinished();
      });
    }
  },

  function PC_LOCAL_SETUP_ICE_LOGGER(test) {
    test.pcLocal.logIceConnectionState();
  },

  function PC_REMOTE_SETUP_ICE_LOGGER(test) {
    test.pcRemote.logIceConnectionState();
  },

  function PC_LOCAL_SETUP_SIGNALING_LOGGER(test) {
    test.pcLocal.logSignalingState();
  },

  function PC_REMOTE_SETUP_SIGNALING_LOGGER(test) {
    test.pcRemote.logSignalingState();
  },

  function PC_LOCAL_SETUP_ADDSTREAM_HANDLER(test) {
    test.pcLocal.setupAddStreamEventHandler();
  },

  function PC_REMOTE_SETUP_ADDSTREAM_HANDLER(test) {
    test.pcRemote.setupAddStreamEventHandler();
  },

  function PC_LOCAL_CHECK_INITIAL_SIGNALINGSTATE(test) {
    is(test.pcLocal.signalingState, STABLE,
       "Initial local signalingState is 'stable'");
  },

  function PC_REMOTE_CHECK_INITIAL_SIGNALINGSTATE(test) {
    is(test.pcRemote.signalingState, STABLE,
       "Initial remote signalingState is 'stable'");
  },

  function PC_LOCAL_CHECK_INITIAL_ICE_STATE(test) {
    is(test.pcLocal.iceConnectionState, ICE_NEW,
       "Initial local ICE connection state is 'new'");
  },

  function PC_REMOTE_CHECK_INITIAL_ICE_STATE(test) {
    is(test.pcRemote.iceConnectionState, ICE_NEW,
       "Initial remote ICE connection state is 'new'");
  },

];

var commandsGetUserMedia = [
  function PC_LOCAL_GUM(test) {
    return test.pcLocal.getAllUserMedia(test.pcLocal.constraints);
  },

  function PC_REMOTE_GUM(test) {
    return test.pcRemote.getAllUserMedia(test.pcRemote.constraints);
  },
];

var commandsPeerConnectionOfferAnswer = [
  function PC_LOCAL_SETUP_ICE_HANDLER(test) {
    test.pcLocal.setupIceCandidateHandler(test);
    if (test.steeplechase) {
      test.pcLocal.endOfTrickleIce.then(() => {
        send_message({"type": "end_of_trickle_ice"});
      });
    }
  },

  function PC_REMOTE_SETUP_ICE_HANDLER(test) {
    test.pcRemote.setupIceCandidateHandler(test);
    if (test.steeplechase) {
      test.pcRemote.endOfTrickleIce.then(() => {
        send_message({"type": "end_of_trickle_ice"});
      });
    }
  },

  function PC_LOCAL_STEEPLECHASE_SIGNAL_EXPECTED_LOCAL_TRACKS(test) {
    if (test.steeplechase) {
      send_message({"type": "local_expected_tracks",
                    "expected_tracks": test.pcLocal.expectedLocalTrackInfoById});
    }
  },

  function PC_REMOTE_STEEPLECHASE_SIGNAL_EXPECTED_LOCAL_TRACKS(test) {
    if (test.steeplechase) {
      send_message({"type": "remote_expected_tracks",
                    "expected_tracks": test.pcRemote.expectedLocalTrackInfoById});
    }
  },

  function PC_LOCAL_GET_EXPECTED_REMOTE_TRACKS(test) {
    if (test.steeplechase) {
      return test.getSignalingMessage("remote_expected_tracks").then(
          message => {
            test.pcLocal.expectedRemoteTrackInfoById = message.expected_tracks;
          });
    }

    // Deep copy, as similar to steeplechase as possible
    test.pcLocal.expectedRemoteTrackInfoById =
      JSON.parse(JSON.stringify(test.pcRemote.expectedLocalTrackInfoById));
  },

  function PC_REMOTE_GET_EXPECTED_REMOTE_TRACKS(test) {
    if (test.steeplechase) {
      return test.getSignalingMessage("local_expected_tracks").then(
          message => {
            test.pcRemote.expectedRemoteTrackInfoById = message.expected_tracks;
          });
    }

    // Deep copy, as similar to steeplechase as possible
    test.pcRemote.expectedRemoteTrackInfoById =
      JSON.parse(JSON.stringify(test.pcLocal.expectedLocalTrackInfoById));
  },

  function PC_LOCAL_CREATE_OFFER(test) {
    return test.createOffer(test.pcLocal).then(offer => {
      is(test.pcLocal.signalingState, STABLE,
         "Local create offer does not change signaling state");
    });
  },

  function PC_LOCAL_STEEPLECHASE_SIGNAL_OFFER(test) {
    if (test.steeplechase) {
      send_message({"type": "offer",
                    "offer": test.originalOffer,
                    "offer_constraints": test.pcLocal.constraints,
                    "offer_options": test.pcLocal.offerOptions});
      test._local_offer = test.originalOffer;
      test._offer_constraints = test.pcLocal.constraints;
      test._offer_options = test.pcLocal.offerOptions;
    }
  },

  function PC_LOCAL_SET_LOCAL_DESCRIPTION(test) {
    return test.setLocalDescription(test.pcLocal, test.originalOffer, HAVE_LOCAL_OFFER)
      .then(() => {
        is(test.pcLocal.signalingState, HAVE_LOCAL_OFFER,
           "signalingState after local setLocalDescription is 'have-local-offer'");
      });
  },

  function PC_REMOTE_GET_OFFER(test) {
    if (!test.steeplechase) {
      test._local_offer = test.originalOffer;
      test._offer_constraints = test.pcLocal.constraints;
      test._offer_options = test.pcLocal.offerOptions;
      return Promise.resolve();
    }
    return test.getSignalingMessage("offer")
      .then(message => {
        ok("offer" in message, "Got an offer message");
        test._local_offer = new mozRTCSessionDescription(message.offer);
        test._offer_constraints = message.offer_constraints;
        test._offer_options = message.offer_options;
      });
  },

  function PC_REMOTE_SET_REMOTE_DESCRIPTION(test) {
    return test.setRemoteDescription(test.pcRemote, test._local_offer, HAVE_REMOTE_OFFER)
      .then(() => {
        is(test.pcRemote.signalingState, HAVE_REMOTE_OFFER,
           "signalingState after remote setRemoteDescription is 'have-remote-offer'");
      });
  },

  function PC_LOCAL_SANE_LOCAL_SDP(test) {
    test.pcLocal.verifySdp(test._local_offer, "offer",
                           test._offer_constraints, test._offer_options,
                           true);
  },

  function PC_REMOTE_SANE_REMOTE_SDP(test) {
    test.pcRemote.verifySdp(test._local_offer, "offer",
                            test._offer_constraints, test._offer_options,
                            false);
  },

  function PC_REMOTE_CREATE_ANSWER(test) {
    return test.createAnswer(test.pcRemote)
      .then(answer => {
        is(test.pcRemote.signalingState, HAVE_REMOTE_OFFER,
           "Remote createAnswer does not change signaling state");
        if (test.steeplechase) {
          send_message({"type": "answer",
                        "answer": test.originalAnswer,
                        "answer_constraints": test.pcRemote.constraints});
          test._remote_answer = test.pcRemote._last_answer;
          test._answer_constraints = test.pcRemote.constraints;
        }
      });
  },

  function PC_REMOTE_CHECK_FOR_DUPLICATED_PORTS_IN_SDP(test) {
    var re = /a=candidate.* (UDP|TCP) [\d]+ ([\d\.]+) ([\d]+) typ host/g;

    var _sdpCandidatesIntoArray = sdp => {
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
        var triple = regexArray[1] + ":" + regexArray[2] + ":" + regexArray[3];
        info("triple: " + triple);
        if (resultArray.indexOf(triple) !== -1) {
          dump("SDP: " + sdp.replace(/[\r]/g, '') + "\n");
          ok(false, "This Transport:IP:Port " + triple + " appears twice in the SDP above!");
        }
        resultArray.push(triple);
      }
      return resultArray;
    };

    var offerTriples = _sdpCandidatesIntoArray(test._local_offer.sdp);
    info("Offer ICE host candidates: " + JSON.stringify(offerTriples));

    var answerTriples = _sdpCandidatesIntoArray(test.originalAnswer.sdp);
    info("Answer ICE host candidates: " + JSON.stringify(answerTriples));

    offerTriples.forEach(o => {
      if (answerTriples.indexOf(o) !== -1) {
        dump("SDP offer: " + test._local_offer.sdp.replace(/[\r]/g, '') + "\n");
        dump("SDP answer: " + test.originalAnswer.sdp.replace(/[\r]/g, '') + "\n");
        ok(false, "This IP:Port " + o + " appears in SDP offer and answer!");
      }
    });
  },

  function PC_REMOTE_SET_LOCAL_DESCRIPTION(test) {
    return test.setLocalDescription(test.pcRemote, test.originalAnswer, STABLE)
      .then(() => {
        is(test.pcRemote.signalingState, STABLE,
           "signalingState after remote setLocalDescription is 'stable'");
      });
  },

  function PC_LOCAL_GET_ANSWER(test) {
    if (!test.steeplechase) {
      test._remote_answer = test.originalAnswer;
      test._answer_constraints = test.pcRemote.constraints;
      return Promise.resolve();
    }

    return test.getSignalingMessage("answer").then(message => {
      ok("answer" in message, "Got an answer message");
      test._remote_answer = new mozRTCSessionDescription(message.answer);
      test._answer_constraints = message.answer_constraints;
    });
  },

  function PC_LOCAL_SET_REMOTE_DESCRIPTION(test) {
    return test.setRemoteDescription(test.pcLocal, test._remote_answer, STABLE)
      .then(() => {
        is(test.pcLocal.signalingState, STABLE,
           "signalingState after local setRemoteDescription is 'stable'");
      });
  },
  function PC_REMOTE_SANE_LOCAL_SDP(test) {
    test.pcRemote.verifySdp(test._remote_answer, "answer",
                            test._offer_constraints, test._offer_options,
                            true);
  },
  function PC_LOCAL_SANE_REMOTE_SDP(test) {
    test.pcLocal.verifySdp(test._remote_answer, "answer",
                           test._offer_constraints, test._offer_options,
                           false);
  },

  function PC_LOCAL_WAIT_FOR_ICE_CONNECTED(test) {
    return waitForIceConnected(test, test.pcLocal);
  },

  function PC_REMOTE_WAIT_FOR_ICE_CONNECTED(test) {
    return waitForIceConnected(test, test.pcRemote);
  },

  function PC_LOCAL_VERIFY_ICE_GATHERING(test) {
    return waitForAnIceCandidate(test.pcLocal);
  },

  function PC_REMOTE_VERIFY_ICE_GATHERING(test) {
    return waitForAnIceCandidate(test.pcRemote);
  },

  function PC_LOCAL_CHECK_MEDIA_TRACKS(test) {
    return test.pcLocal.checkMediaTracks();
  },

  function PC_REMOTE_CHECK_MEDIA_TRACKS(test) {
    return test.pcRemote.checkMediaTracks();
  },

  function PC_LOCAL_WAIT_FOR_MEDIA_FLOW(test) {
    return test.pcLocal.waitForMediaFlow();
  },

  function PC_REMOTE_WAIT_FOR_MEDIA_FLOW(test) {
    return test.pcRemote.waitForMediaFlow();
  },

  function PC_LOCAL_CHECK_STATS(test) {
    return test.pcLocal.getStats().then(stats => {
      test.pcLocal.checkStats(stats, test.steeplechase);
    });
  },

  function PC_REMOTE_CHECK_STATS(test) {
    test.pcRemote.getStats().then(stats => {
      test.pcRemote.checkStats(stats, test.steeplechase);
    });
  },

  function PC_LOCAL_CHECK_ICE_CONNECTION_TYPE(test) {
    test.pcLocal.getStats().then(stats => {
      test.pcLocal.checkStatsIceConnectionType(stats);
    });
  },

  function PC_REMOTE_CHECK_ICE_CONNECTION_TYPE(test) {
    test.pcRemote.getStats().then(stats => {
      test.pcRemote.checkStatsIceConnectionType(stats);
    });
  },

  function PC_LOCAL_CHECK_ICE_CONNECTIONS(test) {
    test.pcLocal.getStats().then(stats => {
      test.pcLocal.checkStatsIceConnections(stats,
                                            test._offer_constraints,
                                            test._offer_options,
                                            test._remote_answer);
    });
  },

  function PC_REMOTE_CHECK_ICE_CONNECTIONS(test) {
    test.pcRemote.getStats().then(stats => {
      test.pcRemote.checkStatsIceConnections(stats,
                                             test._offer_constraints,
                                             test._offer_options,
                                             test.originalAnswer);
    });
  },

  function PC_LOCAL_CHECK_MSID(test) {
    return test.pcLocal.checkMsids();
  },
  function PC_REMOTE_CHECK_MSID(test) {
    return test.pcRemote.checkMsids();
  },

  function PC_LOCAL_CHECK_STATS(test) {
    return checkAllTrackStats(test.pcLocal);
  },
  function PC_REMOTE_CHECK_STATS(test) {
    return checkAllTrackStats(test.pcRemote);
  },
  function PC_LOCAL_WAIT_FOR_END_OF_TRICKLE(test) {
    return test.pcLocal.endOfTrickleIce;
  },
  function PC_REMOTE_WAIT_FOR_END_OF_TRICKLE(test) {
    return test.pcRemote.endOfTrickleIce;
  }
];

function PC_LOCAL_REMOVE_BUNDLE_FROM_OFFER(test) {
  test.originalOffer.sdp = test.originalOffer.sdp.replace(
      /a=group:BUNDLE .*\r\n/g,
      ""
      );
  info("Updated no bundle offer: " + JSON.stringify(test.originalOffer));
};

var addRenegotiation = (chain, commands, checks) => {
  chain.append(commands);
  chain.append(commandsPeerConnectionOfferAnswer);
  if (checks) {
    chain.append(checks);
  }
};

var addRenegotiationAnswerer = (chain, commands, checks) => {
  chain.append(function SWAP_PC_LOCAL_PC_REMOTE(test) {
    var temp = test.pcLocal;
    test.pcLocal = test.pcRemote;
    test.pcRemote = temp;
  });
  addRenegotiation(chain, commands, checks);
};
