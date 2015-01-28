/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function makeOffererNonTrickle(chain) {
  chain.replace('PC_LOCAL_SETUP_ICE_HANDLER', [
    ['PC_LOCAL_SETUP_NOTRICKLE_ICE_HANDLER',
      function (test) {
        test.pcLocalWaitingForEndOfTrickleIce = false;
        // We need to install this callback before calling setLocalDescription
        // otherwise we might miss callbacks
        test.pcLocal.setupIceCandidateHandler(test, function () {
            // We ignore ICE candidates because we want the full offer
          } , function (label) {
            if (test.pcLocalWaitingForEndOfTrickleIce) {
              // This callback is needed for slow environments where ICE
              // trickling has not finished before the other side needs the
              // full SDP. In this case, this call to test.next() will complete
              // the PC_REMOTE_WAIT_FOR_OFFER step (see below).
              info("Looks like we were still waiting for Trickle to finish");
              // TODO replace this with a Promise
              test.next();
            }
          });
        // We can't wait for trickle to finish here as it will only start once
        // we have called setLocalDescription in the next step
        test.next();
      }
    ]
  ]);
  chain.replace('PC_REMOTE_GET_OFFER', [
    ['PC_REMOTE_WAIT_FOR_OFFER',
      function (test) {
        if (test.pcLocal.endOfTrickleIce) {
          info("Trickle ICE finished already");
          test.next();
        } else {
          info("Waiting for trickle ICE to finish");
          test.pcLocalWaitingForEndOfTrickleIce = true;
          // In this case we rely on the callback from
          // PC_LOCAL_SETUP_NOTRICKLE_ICE_HANDLER above to proceed to the next
          // step once trickle is finished.
        }
      }
    ],
    ['PC_REMOTE_GET_FULL_OFFER',
      function (test) {
        test._local_offer = test.pcLocal.localDescription;
        test._offer_constraints = test.pcLocal.constraints;
        test._offer_options = test.pcLocal.offerOptions;
        test.next();
      }
    ]
  ]);
  chain.insertAfter('PC_REMOTE_SANE_REMOTE_SDP', [
    ['PC_REMOTE_REQUIRE_REMOTE_SDP_CANDIDATES',
      function (test) {
        info("test.pcLocal.localDescription.sdp: " + JSON.stringify(test.pcLocal.localDescription.sdp));
        info("test._local_offer.sdp" + JSON.stringify(test._local_offer.sdp));
        ok(!test.localRequiresTrickleIce, "Local does NOT require trickle");
        ok(test._local_offer.sdp.contains("a=candidate"), "offer has ICE candidates")
        // TODO check for a=end-of-candidates once implemented
        test.next();
      }
    ]
  ]);
}

function makeAnswererNonTrickle(chain) {
  chain.replace('PC_REMOTE_SETUP_ICE_HANDLER', [
    ['PC_REMOTE_SETUP_NOTRICKLE_ICE_HANDLER',
      function (test) {
        test.pcRemoteWaitingForEndOfTrickleIce = false;
        // We need to install this callback before calling setLocalDescription
        // otherwise we might miss callbacks
        test.pcRemote.setupIceCandidateHandler(test, function () {
          // We ignore ICE candidates because we want the full answer
          }, function (label) {
            if (test.pcRemoteWaitingForEndOfTrickleIce) {
              // This callback is needed for slow environments where ICE
              // trickling has not finished before the other side needs the
              // full SDP. In this case this callback will call the step after
              // PC_LOCAL_WAIT_FOR_ANSWER
              info("Looks like we were still waiting for Trickle to finish");
              // TODO replace this with a Promise
              test.next();
            }
          });
        // We can't wait for trickle to finish here as it will only start once
        // we have called setLocalDescription in the next step
        test.next();
      }
    ]
  ]);
  chain.replace('PC_LOCAL_GET_ANSWER', [
    ['PC_LOCAL_WAIT_FOR_ANSWER',
      function (test) {
        if (test.pcRemote.endOfTrickleIce) {
          info("Trickle ICE finished already");
          test.next();
        } else {
          info("Waiting for trickle ICE to finish");
          test.pcRemoteWaitingForEndOfTrickleIce = true;
          // In this case we rely on the callback from
          // PC_REMOTE_SETUP_NOTRICKLE_ICE_HANDLER above to proceed to the next
          // step once trickle is finished.
        }
      }
    ],
    ['PC_LOCAL_GET_FULL_ANSWER',
      function (test) {
        test._remote_answer = test.pcRemote.localDescription;
        test._answer_constraints = test.pcRemote.constraints;
        test.next();
      }
    ]
  ]);
  chain.insertAfter('PC_LOCAL_SANE_REMOTE_SDP', [
    ['PC_LOCAL_REQUIRE_REMOTE_SDP_CANDIDATES',
      function (test) {
        info("test.pcRemote.localDescription.sdp: " + JSON.stringify(test.pcRemote.localDescription.sdp));
        info("test._remote_answer.sdp" + JSON.stringify(test._remote_answer.sdp));
        ok(!test.remoteRequiresTrickleIce, "Remote does NOT require trickle");
        ok(test._remote_answer.sdp.contains("a=candidate"), "answer has ICE candidates")
        // TODO check for a=end-of-candidates once implemented
        test.next();
      }
    ]
  ]);
}
