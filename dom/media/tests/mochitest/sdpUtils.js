/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var sdputils = {

checkSdpAfterEndOfTrickle: function(sdp, testOptions, label) {
  info("EOC-SDP: " + JSON.stringify(sdp));

  ok(sdp.sdp.includes("a=end-of-candidates"), label + ": SDP contains end-of-candidates");
  sdputils.checkSdpCLineNotDefault(sdp.sdp, label);

  if (testOptions.rtcpmux) {
    ok(sdp.sdp.includes("a=rtcp-mux"), label + ": SDP contains rtcp-mux");
  } else {
    ok(sdp.sdp.includes("a=rtcp:"), label + ": SDP contains rtcp port");
  }
},

// takes sdp in string form (or possibly a fragment, say an m-section), and
// verifies that the default 0.0.0.0 addr is not present.
checkSdpCLineNotDefault: function(sdpStr, label) {
  info("CLINE-NO-DEFAULT-ADDR-SDP: " + JSON.stringify(sdpStr));
  ok(!sdpStr.includes("c=IN IP4 0.0.0.0"), label + ": SDP contains non-zero IP c line");
},

// Also remove mode 0 if it's offered
// Note, we don't bother removing the fmtp lines, which makes a good test
// for some SDP parsing issues.
removeVP8: function(sdp) {
  var updated_sdp = sdp.replace("a=rtpmap:120 VP8/90000\r\n","");
  updated_sdp = updated_sdp.replace("RTP/SAVPF 120 126 97\r\n","RTP/SAVPF 126 97\r\n");
  updated_sdp = updated_sdp.replace("RTP/SAVPF 120 126\r\n","RTP/SAVPF 126\r\n");
  updated_sdp = updated_sdp.replace("a=rtcp-fb:120 nack\r\n","");
  updated_sdp = updated_sdp.replace("a=rtcp-fb:120 nack pli\r\n","");
  updated_sdp = updated_sdp.replace("a=rtcp-fb:120 ccm fir\r\n","");
  return updated_sdp;
},

removeRtcpMux: function(sdp) {
  return sdp.replace(/a=rtcp-mux\r\n/g,"");
},

removeBundle: function(sdp) {
  return sdp.replace(/a=group:BUNDLE .*\r\n/g, "");
},

reduceAudioMLineToPcmuPcma: function(sdp) {
  return sdp.replace(/m=audio .*\r\n/g, "m=audio 9 UDP/TLS/RTP/SAVPF 0 8\r\n");
},

setAllMsectionsInactive: function(sdp) {
  return sdp.replace(/\r\na=sendrecv/g, "\r\na=inactive")
            .replace(/\r\na=sendonly/g, "\r\na=inactive")
            .replace(/\r\na=recvonly/g, "\r\na=inactive");
},

removeAllRtpMaps: function(sdp) {
  return sdp.replace(/a=rtpmap:.*\r\n/g, "");
},

reduceAudioMLineToDynamicPtAndOpus: function(sdp) {
  return sdp.replace(/m=audio .*\r\n/g, "m=audio 9 UDP/TLS/RTP/SAVPF 101 109\r\n");
},

transferSimulcastProperties: function(offer_sdp, answer_sdp) {
  if (!offer_sdp.includes("a=simulcast:")) {
    return answer_sdp;
  }
  var o_simul = offer_sdp.match(/simulcast: send rid=(.*)([\n$])*/i);
  var o_rids = offer_sdp.match(/a=rid:(.*)/ig);
  var new_answer_sdp = answer_sdp + "a=simulcast: recv rid=" + o_simul[1] + "\r\n";
  o_rids.forEach((o_rid) => {
    new_answer_sdp = new_answer_sdp + o_rid.replace(/send/, "recv") + "\r\n";
  });
  return new_answer_sdp;
},

verifySdp: function(desc, expectedType, offerConstraintsList, offerOptions,
                    testOptions) {
  info("Examining this SessionDescription: " + JSON.stringify(desc));
  info("offerConstraintsList: " + JSON.stringify(offerConstraintsList));
  info("offerOptions: " + JSON.stringify(offerOptions));
  ok(desc, "SessionDescription is not null");
  is(desc.type, expectedType, "SessionDescription type is " + expectedType);
  ok(desc.sdp.length > 10, "SessionDescription body length is plausible");
  ok(desc.sdp.includes("a=ice-ufrag"), "ICE username is present in SDP");
  ok(desc.sdp.includes("a=ice-pwd"), "ICE password is present in SDP");
  ok(desc.sdp.includes("a=fingerprint"), "ICE fingerprint is present in SDP");
  //TODO: update this for loopback support bug 1027350
  ok(!desc.sdp.includes(LOOPBACK_ADDR), "loopback interface is absent from SDP");
  var requiresTrickleIce = !desc.sdp.includes("a=candidate");
  if (requiresTrickleIce) {
    info("at least one ICE candidate is present in SDP");
  } else {
    info("No ICE candidate in SDP -> requiring trickle ICE");
  }

  //TODO: how can we check for absence/presence of m=application?

  var audioTracks =
      sdputils.countTracksInConstraint('audio', offerConstraintsList) ||
      ((offerOptions && offerOptions.offerToReceiveAudio) ? 1 : 0);

  info("expected audio tracks: " + audioTracks);
  if (audioTracks == 0) {
    ok(!desc.sdp.includes("m=audio"), "audio m-line is absent from SDP");
  } else {
    ok(desc.sdp.includes("m=audio"), "audio m-line is present in SDP");
    is(testOptions.opus, desc.sdp.includes("a=rtpmap:109 opus/48000/2"), "OPUS codec is present in SDP");
    //TODO: ideally the rtcp-mux should be for the m=audio, and not just
    //      anywhere in the SDP (JS SDP parser bug 1045429)
    is(testOptions.rtcpmux, desc.sdp.includes("a=rtcp-mux"), "RTCP Mux is offered in SDP");
  }

  var videoTracks =
      sdputils.countTracksInConstraint('video', offerConstraintsList) ||
      ((offerOptions && offerOptions.offerToReceiveVideo) ? 1 : 0);

  info("expected video tracks: " + videoTracks);
  if (videoTracks == 0) {
    ok(!desc.sdp.includes("m=video"), "video m-line is absent from SDP");
  } else {
    ok(desc.sdp.includes("m=video"), "video m-line is present in SDP");
    if (testOptions.h264) {
      ok(desc.sdp.includes("a=rtpmap:126 H264/90000"), "H.264 codec is present in SDP");
    } else {
      ok(desc.sdp.includes("a=rtpmap:120 VP8/90000"), "VP8 codec is present in SDP");
    }
    is(testOptions.rtcpmux, desc.sdp.includes("a=rtcp-mux"), "RTCP Mux is offered in SDP");
  }

  return requiresTrickleIce;
},

/**
 * Counts the amount of audio tracks in a given media constraint.
 *
 * @param constraints
 *        The contraint to be examined.
 */
countTracksInConstraint: function(type, constraints) {
  if (!Array.isArray(constraints)) {
    return 0;
  }
  return constraints.reduce((sum, c) => sum + (c[type] ? 1 : 0), 0);
},

};
