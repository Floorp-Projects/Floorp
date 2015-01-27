/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
        test._local_offer = test.pcLocal.localDescription;
        test._offer_constraints = test.pcLocal.constraints;
        test._offer_options = test.pcLocal.offerOptions;
      });
    }
  ]);
  chain.insertAfter('PC_REMOTE_SANE_REMOTE_SDP', [
    function PC_REMOTE_REQUIRE_REMOTE_SDP_CANDIDATES(test) {
      info("test.pcLocal.localDescription.sdp: " + JSON.stringify(test.pcLocal.localDescription.sdp));
      info("test._local_offer.sdp" + JSON.stringify(test._local_offer.sdp));
      ok(!test.localRequiresTrickleIce, "Local does NOT require trickle");
      ok(test._local_offer.sdp.contains("a=candidate"), "offer has ICE candidates")
      ok(test._local_offer.sdp.contains("a=end-of-candidates"), "offer has end-of-candidates");
    }
  ]);
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
        test._remote_answer = test.pcRemote.localDescription;
        test._answer_constraints = test.pcRemote.constraints;
      });
    }
  ]);
  chain.insertAfter('PC_LOCAL_SANE_REMOTE_SDP', [
    function PC_LOCAL_REQUIRE_REMOTE_SDP_CANDIDATES(test) {
      info("test.pcRemote.localDescription.sdp: " + JSON.stringify(test.pcRemote.localDescription.sdp));
      info("test._remote_answer.sdp" + JSON.stringify(test._remote_answer.sdp));
      ok(!test.remoteRequiresTrickleIce, "Remote does NOT require trickle");
      ok(test._remote_answer.sdp.contains("a=candidate"), "answer has ICE candidates")
      ok(test._remote_answer.sdp.contains("a=end-of-candidates"), "answer has end-of-candidates");
    }
  ]);
}
