/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function removeTrickleOption(desc) {
  var sdp = desc.sdp.replace(/\r\na=ice-options:trickle\r\n/, "\r\n");
  return new mozRTCSessionDescription({ type: desc.type, sdp: sdp });
}

function makeOffererNonTrickle(chain) {
  chain.replace('PC_LOCAL_SETUP_ICE_HANDLER', [
    function PC_LOCAL_SETUP_NOTRICKLE_ICE_HANDLER(test) {
      // We need to install this callback before calling setLocalDescription
      // otherwise we might miss callbacks
      test.pcLocal.setupIceCandidateHandler(test, () => {});
      // We ignore ICE candidates because we want the full offer
    }
  ]);
  chain.replace('PC_REMOTE_GET_OFFER', [
    function PC_REMOTE_GET_FULL_OFFER(test) {
      return test.pcLocal.endOfTrickleIce.then(() => {
        test._local_offer = removeTrickleOption(test.pcLocal.localDescription);
        test._offer_constraints = test.pcLocal.constraints;
        test._offer_options = test.pcLocal.offerOptions;
      });
    }
  ]);
  chain.insertAfter('PC_REMOTE_SANE_REMOTE_SDP', [
    function PC_REMOTE_REQUIRE_REMOTE_SDP_CANDIDATES(test) {
      info("test.pcLocal.localDescription.sdp: " + JSON.stringify(test.pcLocal.localDescription.sdp));
      info("test._local_offer.sdp" + JSON.stringify(test._local_offer.sdp));
      is(test.pcRemote._pc.canTrickleIceCandidates, false,
         "Remote thinks that trickle isn't supported");
      ok(!test.localRequiresTrickleIce, "Local does NOT require trickle");
      ok(test._local_offer.sdp.includes("a=candidate"), "offer has ICE candidates")
      ok(test._local_offer.sdp.includes("a=end-of-candidates"), "offer has end-of-candidates");
    }
  ]);
  chain.remove('PC_REMOTE_CHECK_CAN_TRICKLE_SYNC');
}

function makeAnswererNonTrickle(chain) {
  chain.replace('PC_REMOTE_SETUP_ICE_HANDLER', [
    function PC_REMOTE_SETUP_NOTRICKLE_ICE_HANDLER(test) {
      // We need to install this callback before calling setLocalDescription
      // otherwise we might miss callbacks
      test.pcRemote.setupIceCandidateHandler(test, () => {});
      // We ignore ICE candidates because we want the full offer
    }
  ]);
  chain.replace('PC_LOCAL_GET_ANSWER', [
    function PC_LOCAL_GET_FULL_ANSWER(test) {
      return test.pcRemote.endOfTrickleIce.then(() => {
        test._remote_answer = removeTrickleOption(test.pcRemote.localDescription);
        test._answer_constraints = test.pcRemote.constraints;
      });
    }
  ]);
  chain.insertAfter('PC_LOCAL_SANE_REMOTE_SDP', [
    function PC_LOCAL_REQUIRE_REMOTE_SDP_CANDIDATES(test) {
      info("test.pcRemote.localDescription.sdp: " + JSON.stringify(test.pcRemote.localDescription.sdp));
      info("test._remote_answer.sdp" + JSON.stringify(test._remote_answer.sdp));
      is(test.pcLocal._pc.canTrickleIceCandidates, false,
         "Local thinks that trickle isn't supported");
      ok(!test.remoteRequiresTrickleIce, "Remote does NOT require trickle");
      ok(test._remote_answer.sdp.includes("a=candidate"), "answer has ICE candidates")
      ok(test._remote_answer.sdp.includes("a=end-of-candidates"), "answer has end-of-candidates");
    }
  ]);
  chain.remove('PC_LOCAL_CHECK_CAN_TRICKLE_SYNC');
}
