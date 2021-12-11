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
const GATH_COMPLETE = "complete";

function deltaSeconds(date1, date2) {
  return (date2.getTime() - date1.getTime()) / 1000;
}

function dumpSdp(test) {
  if (typeof test._local_offer !== "undefined") {
    dump("ERROR: SDP offer: " + test._local_offer.sdp.replace(/[\r]/g, ""));
  }
  if (typeof test._remote_answer !== "undefined") {
    dump("ERROR: SDP answer: " + test._remote_answer.sdp.replace(/[\r]/g, ""));
  }

  if (
    test.pcLocal &&
    typeof test.pcLocal._local_ice_candidates !== "undefined"
  ) {
    dump(
      "pcLocal._local_ice_candidates: " +
        JSON.stringify(test.pcLocal._local_ice_candidates) +
        "\n"
    );
    dump(
      "pcLocal._remote_ice_candidates: " +
        JSON.stringify(test.pcLocal._remote_ice_candidates) +
        "\n"
    );
    dump(
      "pcLocal._ice_candidates_to_add: " +
        JSON.stringify(test.pcLocal._ice_candidates_to_add) +
        "\n"
    );
  }
  if (
    test.pcRemote &&
    typeof test.pcRemote._local_ice_candidates !== "undefined"
  ) {
    dump(
      "pcRemote._local_ice_candidates: " +
        JSON.stringify(test.pcRemote._local_ice_candidates) +
        "\n"
    );
    dump(
      "pcRemote._remote_ice_candidates: " +
        JSON.stringify(test.pcRemote._remote_ice_candidates) +
        "\n"
    );
    dump(
      "pcRemote._ice_candidates_to_add: " +
        JSON.stringify(test.pcRemote._ice_candidates_to_add) +
        "\n"
    );
  }

  if (test.pcLocal && typeof test.pcLocal.iceConnectionLog !== "undefined") {
    dump(
      "pcLocal ICE connection state log: " +
        test.pcLocal.iceConnectionLog +
        "\n"
    );
  }
  if (test.pcRemote && typeof test.pcRemote.iceConnectionLog !== "undefined") {
    dump(
      "pcRemote ICE connection state log: " +
        test.pcRemote.iceConnectionLog +
        "\n"
    );
  }

  if (
    test.pcLocal &&
    test.pcRemote &&
    typeof test.pcLocal.setRemoteDescDate !== "undefined" &&
    typeof test.pcRemote.setLocalDescDate !== "undefined"
  ) {
    var delta = deltaSeconds(
      test.pcLocal.setRemoteDescDate,
      test.pcRemote.setLocalDescDate
    );
    dump(
      "Delay between pcLocal.setRemote <-> pcRemote.setLocal: " + delta + "\n"
    );
  }
  if (
    test.pcLocal &&
    test.pcRemote &&
    typeof test.pcLocal.setRemoteDescDate !== "undefined" &&
    typeof test.pcLocal.setRemoteDescStableEventDate !== "undefined"
  ) {
    var delta = deltaSeconds(
      test.pcLocal.setRemoteDescDate,
      test.pcLocal.setRemoteDescStableEventDate
    );
    dump(
      "Delay between pcLocal.setRemote <-> pcLocal.signalingStateStable: " +
        delta +
        "\n"
    );
  }
  if (
    test.pcLocal &&
    test.pcRemote &&
    typeof test.pcRemote.setLocalDescDate !== "undefined" &&
    typeof test.pcRemote.setLocalDescStableEventDate !== "undefined"
  ) {
    var delta = deltaSeconds(
      test.pcRemote.setLocalDescDate,
      test.pcRemote.setLocalDescStableEventDate
    );
    dump(
      "Delay between pcRemote.setLocal <-> pcRemote.signalingStateStable: " +
        delta +
        "\n"
    );
  }
}

// We need to verify that at least one candidate has been (or will be) gathered.
function waitForAnIceCandidate(pc) {
  return new Promise(resolve => {
    if (!pc.localRequiresTrickleIce || pc._local_ice_candidates.length > 0) {
      resolve();
    } else {
      // In some circumstances, especially when both PCs are on the same
      // browser, even though we are connected, the connection can be
      // established without receiving a single candidate from one or other
      // peer.  So we wait for at least one...
      pc._pc.addEventListener("icecandidate", resolve);
    }
  }).then(() => {
    ok(
      pc._local_ice_candidates.length > 0,
      pc + " received local trickle ICE candidates"
    );
    isnot(
      pc._pc.iceGatheringState,
      GATH_NEW,
      pc + " ICE gathering state is not 'new'"
    );
  });
}

async function checkTrackStats(pc, track, outbound) {
  const audio = track.kind == "audio";
  const msg =
    `${pc} stats ${outbound ? "outbound " : "inbound "}` +
    `${audio ? "audio" : "video"} rtp track id ${track.id}`;
  const stats = await pc.getStats(track);
  ok(
    pc.hasStat(stats, {
      type: outbound ? "outbound-rtp" : "inbound-rtp",
      kind: audio ? "audio" : "video",
    }),
    `${msg} - found expected stats`
  );
  ok(
    !pc.hasStat(stats, {
      type: outbound ? "inbound-rtp" : "outbound-rtp",
    }),
    `${msg} - did not find extra stats with wrong direction`
  );
  ok(
    !pc.hasStat(stats, {
      kind: audio ? "video" : "audio",
    }),
    `${msg} - did not find extra stats with wrong media type`
  );
}

function checkAllTrackStats(pc) {
  return Promise.all([
    ...pc
      .getExpectedActiveReceivers()
      .map(({ track }) => checkTrackStats(pc, track, false)),
    ...pc
      .getExpectedSenders()
      .map(({ track }) => checkTrackStats(pc, track, true)),
  ]);
}

// Commands run once at the beginning of each test, even when performing a
// renegotiation test.
var commandsPeerConnectionInitial = [
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

  function PC_LOCAL_SETUP_TRACK_HANDLER(test) {
    test.pcLocal.setupTrackEventHandler();
  },

  function PC_REMOTE_SETUP_TRACK_HANDLER(test) {
    test.pcRemote.setupTrackEventHandler();
  },

  function PC_LOCAL_CHECK_INITIAL_SIGNALINGSTATE(test) {
    is(
      test.pcLocal.signalingState,
      STABLE,
      "Initial local signalingState is 'stable'"
    );
  },

  function PC_REMOTE_CHECK_INITIAL_SIGNALINGSTATE(test) {
    is(
      test.pcRemote.signalingState,
      STABLE,
      "Initial remote signalingState is 'stable'"
    );
  },

  function PC_LOCAL_CHECK_INITIAL_ICE_STATE(test) {
    is(
      test.pcLocal.iceConnectionState,
      ICE_NEW,
      "Initial local ICE connection state is 'new'"
    );
  },

  function PC_REMOTE_CHECK_INITIAL_ICE_STATE(test) {
    is(
      test.pcRemote.iceConnectionState,
      ICE_NEW,
      "Initial remote ICE connection state is 'new'"
    );
  },

  function PC_LOCAL_CHECK_INITIAL_CAN_TRICKLE_SYNC(test) {
    is(
      test.pcLocal._pc.canTrickleIceCandidates,
      null,
      "Local trickle status should start out unknown"
    );
  },

  function PC_REMOTE_CHECK_INITIAL_CAN_TRICKLE_SYNC(test) {
    is(
      test.pcRemote._pc.canTrickleIceCandidates,
      null,
      "Remote trickle status should start out unknown"
    );
  },
];

var commandsGetUserMedia = [
  function PC_LOCAL_GUM(test) {
    return test.pcLocal.getAllUserMediaAndAddStreams(test.pcLocal.constraints);
  },

  function PC_REMOTE_GUM(test) {
    return test.pcRemote.getAllUserMediaAndAddStreams(
      test.pcRemote.constraints
    );
  },
];

var commandsPeerConnectionOfferAnswer = [
  function PC_LOCAL_SETUP_ICE_HANDLER(test) {
    test.pcLocal.setupIceCandidateHandler(test);
  },

  function PC_REMOTE_SETUP_ICE_HANDLER(test) {
    test.pcRemote.setupIceCandidateHandler(test);
  },

  function PC_LOCAL_CREATE_OFFER(test) {
    return test.createOffer(test.pcLocal).then(offer => {
      is(
        test.pcLocal.signalingState,
        STABLE,
        "Local create offer does not change signaling state"
      );
    });
  },

  function PC_LOCAL_SET_LOCAL_DESCRIPTION(test) {
    return test
      .setLocalDescription(test.pcLocal, test.originalOffer, HAVE_LOCAL_OFFER)
      .then(() => {
        is(
          test.pcLocal.signalingState,
          HAVE_LOCAL_OFFER,
          "signalingState after local setLocalDescription is 'have-local-offer'"
        );
      });
  },

  function PC_REMOTE_GET_OFFER(test) {
    test._local_offer = test.originalOffer;
    test._offer_constraints = test.pcLocal.constraints;
    test._offer_options = test.pcLocal.offerOptions;
    return Promise.resolve();
  },

  function PC_REMOTE_SET_REMOTE_DESCRIPTION(test) {
    return test
      .setRemoteDescription(test.pcRemote, test._local_offer, HAVE_REMOTE_OFFER)
      .then(() => {
        is(
          test.pcRemote.signalingState,
          HAVE_REMOTE_OFFER,
          "signalingState after remote setRemoteDescription is 'have-remote-offer'"
        );
      });
  },

  function PC_REMOTE_CHECK_CAN_TRICKLE_SYNC(test) {
    is(
      test.pcRemote._pc.canTrickleIceCandidates,
      true,
      "Remote thinks that local can trickle"
    );
  },

  function PC_LOCAL_SANE_LOCAL_SDP(test) {
    test.pcLocal.localRequiresTrickleIce = sdputils.verifySdp(
      test._local_offer,
      "offer",
      test._offer_constraints,
      test._offer_options,
      test.testOptions
    );
  },

  function PC_REMOTE_SANE_REMOTE_SDP(test) {
    test.pcRemote.remoteRequiresTrickleIce = sdputils.verifySdp(
      test._local_offer,
      "offer",
      test._offer_constraints,
      test._offer_options,
      test.testOptions
    );
  },

  function PC_REMOTE_CREATE_ANSWER(test) {
    return test.createAnswer(test.pcRemote).then(answer => {
      is(
        test.pcRemote.signalingState,
        HAVE_REMOTE_OFFER,
        "Remote createAnswer does not change signaling state"
      );
    });
  },

  function PC_REMOTE_SET_LOCAL_DESCRIPTION(test) {
    return test
      .setLocalDescription(test.pcRemote, test.originalAnswer, STABLE)
      .then(() => {
        is(
          test.pcRemote.signalingState,
          STABLE,
          "signalingState after remote setLocalDescription is 'stable'"
        );
      });
  },

  function PC_LOCAL_GET_ANSWER(test) {
    test._remote_answer = test.originalAnswer;
    test._answer_constraints = test.pcRemote.constraints;
    return Promise.resolve();
  },

  function PC_LOCAL_SET_REMOTE_DESCRIPTION(test) {
    return test
      .setRemoteDescription(test.pcLocal, test._remote_answer, STABLE)
      .then(() => {
        is(
          test.pcLocal.signalingState,
          STABLE,
          "signalingState after local setRemoteDescription is 'stable'"
        );
      });
  },

  function PC_REMOTE_SANE_LOCAL_SDP(test) {
    test.pcRemote.localRequiresTrickleIce = sdputils.verifySdp(
      test._remote_answer,
      "answer",
      test._offer_constraints,
      test._offer_options,
      test.testOptions
    );
  },
  function PC_LOCAL_SANE_REMOTE_SDP(test) {
    test.pcLocal.remoteRequiresTrickleIce = sdputils.verifySdp(
      test._remote_answer,
      "answer",
      test._offer_constraints,
      test._offer_options,
      test.testOptions
    );
  },

  function PC_LOCAL_CHECK_CAN_TRICKLE_SYNC(test) {
    is(
      test.pcLocal._pc.canTrickleIceCandidates,
      true,
      "Local thinks that remote can trickle"
    );
  },

  function PC_LOCAL_WAIT_FOR_ICE_CONNECTED(test) {
    return test.pcLocal.waitForIceConnected().then(() => {
      info(
        test.pcLocal +
          ": ICE connection state log: " +
          test.pcLocal.iceConnectionLog
      );
    });
  },

  function PC_REMOTE_WAIT_FOR_ICE_CONNECTED(test) {
    return test.pcRemote.waitForIceConnected().then(() => {
      info(
        test.pcRemote +
          ": ICE connection state log: " +
          test.pcRemote.iceConnectionLog
      );
    });
  },

  function PC_LOCAL_VERIFY_ICE_GATHERING(test) {
    return waitForAnIceCandidate(test.pcLocal);
  },

  function PC_REMOTE_VERIFY_ICE_GATHERING(test) {
    return waitForAnIceCandidate(test.pcRemote);
  },

  function PC_LOCAL_WAIT_FOR_MEDIA_FLOW(test) {
    return test.pcLocal.waitForMediaFlow();
  },

  function PC_REMOTE_WAIT_FOR_MEDIA_FLOW(test) {
    return test.pcRemote.waitForMediaFlow();
  },

  function PC_LOCAL_CHECK_STATS(test) {
    return test.pcLocal.getStats().then(stats => {
      test.pcLocal.checkStats(stats);
    });
  },

  function PC_REMOTE_CHECK_STATS(test) {
    return test.pcRemote.getStats().then(stats => {
      test.pcRemote.checkStats(stats);
    });
  },

  function PC_LOCAL_CHECK_ICE_CONNECTION_TYPE(test) {
    return test.pcLocal.getStats().then(stats => {
      test.pcLocal.checkStatsIceConnectionType(
        stats,
        test.testOptions.expectedLocalCandidateType
      );
    });
  },

  function PC_REMOTE_CHECK_ICE_CONNECTION_TYPE(test) {
    return test.pcRemote.getStats().then(stats => {
      test.pcRemote.checkStatsIceConnectionType(
        stats,
        test.testOptions.expectedRemoteCandidateType
      );
    });
  },

  function PC_LOCAL_CHECK_ICE_CONNECTIONS(test) {
    return test.pcLocal.getStats().then(stats => {
      test.pcLocal.checkStatsIceConnections(stats, test.testOptions);
    });
  },

  function PC_REMOTE_CHECK_ICE_CONNECTIONS(test) {
    return test.pcRemote.getStats().then(stats => {
      test.pcRemote.checkStatsIceConnections(stats, test.testOptions);
    });
  },

  function PC_LOCAL_CHECK_MSID(test) {
    return test.pcLocal.checkLocalMsids();
  },
  function PC_REMOTE_CHECK_MSID(test) {
    return test.pcRemote.checkLocalMsids();
  },

  function PC_LOCAL_CHECK_TRACK_STATS(test) {
    return checkAllTrackStats(test.pcLocal);
  },
  function PC_REMOTE_CHECK_TRACK_STATS(test) {
    return checkAllTrackStats(test.pcRemote);
  },
  function PC_LOCAL_VERIFY_SDP_AFTER_END_OF_TRICKLE(test) {
    if (test.pcLocal.endOfTrickleSdp) {
      /* In case the endOfTrickleSdp promise is resolved already it will win the
       * race because it gets evaluated first. But if endOfTrickleSdp is still
       * pending the rejection will win the race. */
      return Promise.race([
        test.pcLocal.endOfTrickleSdp,
        Promise.reject("No SDP"),
      ]).then(
        sdp =>
          sdputils.checkSdpAfterEndOfTrickle(
            sdp,
            test.testOptions,
            test.pcLocal.label
          ),
        () =>
          info(
            "pcLocal: Gathering is not complete yet, skipping post-gathering SDP check"
          )
      );
    }
  },
  function PC_REMOTE_VERIFY_SDP_AFTER_END_OF_TRICKLE(test) {
    if (test.pcRemote.endOfTrickleSdp) {
      /* In case the endOfTrickleSdp promise is resolved already it will win the
       * race because it gets evaluated first. But if endOfTrickleSdp is still
       * pending the rejection will win the race. */
      return Promise.race([
        test.pcRemote.endOfTrickleSdp,
        Promise.reject("No SDP"),
      ]).then(
        sdp =>
          sdputils.checkSdpAfterEndOfTrickle(
            sdp,
            test.testOptions,
            test.pcRemote.label
          ),
        () =>
          info(
            "pcRemote: Gathering is not complete yet, skipping post-gathering SDP check"
          )
      );
    }
  },
];

function PC_LOCAL_REMOVE_ALL_BUT_H264_FROM_OFFER(test) {
  isnot(
    test.originalOffer.sdp.search("H264/90000"),
    -1,
    "H.264 should be present in the SDP offer"
  );
  test.originalOffer.sdp = sdputils.removeCodec(
    sdputils.removeCodec(
      sdputils.removeCodec(test.originalOffer.sdp, 120),
      121,
      97
    )
  );
  info("Updated H264 only offer: " + JSON.stringify(test.originalOffer));
}

function PC_LOCAL_REMOVE_BUNDLE_FROM_OFFER(test) {
  test.originalOffer.sdp = sdputils.removeBundle(test.originalOffer.sdp);
  info("Updated no bundle offer: " + JSON.stringify(test.originalOffer));
}

function PC_LOCAL_REMOVE_RTCPMUX_FROM_OFFER(test) {
  test.originalOffer.sdp = sdputils.removeRtcpMux(test.originalOffer.sdp);
  info("Updated no RTCP-Mux offer: " + JSON.stringify(test.originalOffer));
}

function PC_LOCAL_REMOVE_SSRC_FROM_OFFER(test) {
  test.originalOffer.sdp = sdputils.removeSSRCs(test.originalOffer.sdp);
  info("Updated no SSRCs offer: " + JSON.stringify(test.originalOffer));
}

function PC_REMOTE_REMOVE_SSRC_FROM_ANSWER(test) {
  test.originalAnswer.sdp = sdputils.removeSSRCs(test.originalAnswer.sdp);
  info("Updated no SSRCs answer: " + JSON.stringify(test.originalAnswer));
}

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
