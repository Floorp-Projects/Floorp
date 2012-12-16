/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var PeerConnection = {
  pc1_offer : null,
  pc2_answer : null,

  handShake: function PC_handShake(aPCLocal, aPCRemote, aSuccessCallback) {

    function onCreateOfferSuccess(aOffer) {
      pc1_offer = aOffer;
      aPCLocal.setLocalDescription(aOffer, onSetLocalDescriptionSuccess1,
                                   unexpectedCallbackAndFinish);
    }

    function onSetLocalDescriptionSuccess1() {
      aPCRemote.setRemoteDescription(pc1_offer, onSetRemoteDescriptionSuccess1,
                                     unexpectedCallbackAndFinish);
    }

    function onSetRemoteDescriptionSuccess1() {
      aPCRemote.createAnswer(onCreateAnswerSuccess, unexpectedCallbackAndFinish);
    }

    function onCreateAnswerSuccess(aAnswer) {
      pc2_answer = aAnswer;
      aPCRemote.setLocalDescription(aAnswer, onSetLocalDescriptionSuccess2,
                                    unexpectedCallbackAndFinish);
    }

    function onSetLocalDescriptionSuccess2() {
      aPCLocal.setRemoteDescription(pc2_answer, onSetRemoteDescriptionSuccess2,
                                    unexpectedCallbackAndFinish);
    }

    function onSetRemoteDescriptionSuccess2() {
      aSuccessCallback();
    }

    aPCLocal.createOffer(onCreateOfferSuccess, unexpectedCallbackAndFinish);
  }
};
