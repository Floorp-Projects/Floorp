/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var PeerConnection = {
  pc1_offer : null,
  pc2_answer : null,

  /**
   * Establishes the peer connection between two clients
   *
   * @param {object) aPCLocal Local client
   * @param {object} aPCRemote Remote client
   * @param {function} aSuccessCallback Method to call when the connection has been established
   */
  handShake: function PC_handShake(aPCLocal, aPCRemote, aSuccessCallback) {

    function onCreateOfferSuccess(aOffer) {
      pc1_offer = aOffer;
      info("Calling setLocalDescription on local peer connection");
      aPCLocal.setLocalDescription(aOffer, onSetLocalDescriptionSuccess1,
                                   unexpectedCallbackAndFinish);
    }

    function onSetLocalDescriptionSuccess1() {
      info("Calling setRemoteDescription on remote peer connection");
      aPCRemote.setRemoteDescription(pc1_offer, onSetRemoteDescriptionSuccess1,
                                     unexpectedCallbackAndFinish);
    }

    function onSetRemoteDescriptionSuccess1() {
      info("Calling createAnswer on remote peer connection");
      aPCRemote.createAnswer(onCreateAnswerSuccess, unexpectedCallbackAndFinish);
    }

    function onCreateAnswerSuccess(aAnswer) {
      pc2_answer = aAnswer;
      info("Calling setLocalDescription on remote peer connection");
      aPCRemote.setLocalDescription(aAnswer, onSetLocalDescriptionSuccess2,
                                    unexpectedCallbackAndFinish);
    }

    function onSetLocalDescriptionSuccess2() {
      info("Calling setRemoteDescription on local peer connection");
      aPCLocal.setRemoteDescription(pc2_answer, onSetRemoteDescriptionSuccess2,
                                    unexpectedCallbackAndFinish);
    }

    function onSetRemoteDescriptionSuccess2() {
      aSuccessCallback();
    }

    info("Calling createOffer on local peer connection");
    aPCLocal.createOffer(onCreateOfferSuccess, unexpectedCallbackAndFinish);
  },

  /**
   * Finds the given media stream in the list of available streams.
   *
   * This function is necessary because localStreams and remoteStreams don't have
   * an indexOf method.
   *
   * @param {object} aMediaStreamList List of available media streams
   * @param {object} aMediaStream Media stream to check for
   * @return {number} Index in the media stream list
   */
  findStream: function PC_findStream(aMediaStreamList, aMediaStream) {
    for (var index = 0; index < aMediaStreamList.length; index++) {
      if (aMediaStreamList[index] === aMediaStream) {
        return index;
      }
    }

    return -1
  }
};
