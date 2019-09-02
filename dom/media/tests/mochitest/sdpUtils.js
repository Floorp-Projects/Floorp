/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var sdputils = {
  // Finds the codec id / payload type given a codec format
  // (e.g., "VP8", "VP9/90000"). `offset` tells us which one to use in case of
  // multiple matches.
  findCodecId: function(sdp, format, offset = 0) {
    let regex = new RegExp("rtpmap:([0-9]+) " + format, "gi");
    let match;
    for (let i = 0; i <= offset; ++i) {
      match = regex.exec(sdp);
      if (!match) {
        throw new Error(
          "Couldn't find offset " +
            i +
            " of codec " +
            format +
            " while looking for offset " +
            offset +
            " in sdp:\n" +
            sdp
        );
      }
    }
    // match[0] is the full matched string
    // match[1] is the first parenthesis group
    return match[1];
  },

  // Finds all the extmap ids in the given sdp.  Note that this does NOT
  // consider m-sections, so a more generic version would need to
  // look at each m-section separately.
  findExtmapIds: function(sdp) {
    var sdpExtmapIds = [];
    extmapRegEx = /^a=extmap:([0-9+])/gm;
    // must call exec on the regex to get each match in the string
    while ((searchResults = extmapRegEx.exec(sdp)) !== null) {
      // returned array has the matched text as the first item,
      // and then one item for each capturing parenthesis that
      // matched containing the text that was captured.
      sdpExtmapIds.push(searchResults[1]);
    }
    return sdpExtmapIds;
  },

  findExtmapIdsUrnsDirections: function(sdp) {
    var sdpExtmap = [];
    extmapRegEx = /^a=extmap:([0-9+])([A-Za-z/]*) ([A-Za-z0-9_:\-\/\.]+)/gm;
    // must call exec on the regex to get each match in the string
    while ((searchResults = extmapRegEx.exec(sdp)) !== null) {
      // returned array has the matched text as the first item,
      // and then one item for each capturing parenthesis that
      // matched containing the text that was captured.
      var idUrn = [];
      idUrn.push(searchResults[1]);
      idUrn.push(searchResults[3]);
      idUrn.push(searchResults[2].slice(1));
      sdpExtmap.push(idUrn);
    }
    return sdpExtmap;
  },

  verify_unique_extmap_ids: function(sdp) {
    const sdpExtmapIds = sdputils.findExtmapIdsUrnsDirections(sdp);

    return sdpExtmapIds.reduce(function(result, item, index) {
      const [id, urn, dir] = item;
      ok(
        !(id in result) || (result[id][0] === urn && result[id][1] === dir),
        "ID " + id + " is unique ID for " + urn + " and direction " + dir
      );
      result[id] = [urn, dir];
      return result;
    }, {});
  },

  getMSections: function(sdp) {
    return sdp
      .split(new RegExp("^m=", "gm"))
      .slice(1)
      .map(s => "m=" + s);
  },

  getAudioMSections: function(sdp) {
    return this.getMSections(sdp).filter(section =>
      section.startsWith("m=audio")
    );
  },

  getVideoMSections: function(sdp) {
    return this.getMSections(sdp).filter(section =>
      section.startsWith("m=video")
    );
  },

  checkSdpAfterEndOfTrickle: function(description, testOptions, label) {
    info("EOC-SDP: " + JSON.stringify(description));

    const checkForTransportAttributes = msection => {
      info("Checking msection: " + msection);
      ok(
        msection.includes("a=end-of-candidates"),
        label + ": SDP contains end-of-candidates"
      );

      if (!msection.startsWith("m=application")) {
        if (testOptions.rtcpmux) {
          ok(
            msection.includes("a=rtcp-mux"),
            label + ": SDP contains rtcp-mux"
          );
        } else {
          ok(msection.includes("a=rtcp:"), label + ": SDP contains rtcp port");
        }
      }
    };

    const hasOwnTransport = msection => {
      const port0Check = new RegExp(/^m=\S+ 0 /).exec(msection);
      if (port0Check) {
        return false;
      }
      const midMatch = new RegExp(/\r\na=mid:(\S+)/).exec(msection);
      if (!midMatch) {
        return true;
      }
      const mid = midMatch[1];
      const bundleGroupMatch = new RegExp(
        "\\r\\na=group:BUNDLE \\S.* " + mid + "\\s+"
      ).exec(description.sdp);
      return bundleGroupMatch == null;
    };

    const msectionsWithOwnTransports = this.getMSections(
      description.sdp
    ).filter(hasOwnTransport);

    ok(
      msectionsWithOwnTransports.length > 0,
      "SDP should contain at least one msection with a transport"
    );
    msectionsWithOwnTransports.forEach(checkForTransportAttributes);

    if (testOptions.ssrc) {
      ok(description.sdp.includes("a=ssrc"), label + ": SDP contains a=ssrc");
    } else {
      ok(
        !description.sdp.includes("a=ssrc"),
        label + ": SDP does not contain a=ssrc"
      );
    }
  },

  // Note, we don't bother removing the fmtp lines, which makes a good test
  // for some SDP parsing issues.
  removeCodec: function(sdp, codec) {
    var updated_sdp = sdp.replace(
      new RegExp("a=rtpmap:" + codec + ".*\\/90000\\r\\n", ""),
      ""
    );
    updated_sdp = updated_sdp.replace(
      new RegExp("(RTP\\/SAVPF.*)( " + codec + ")(.*\\r\\n)", ""),
      "$1$3"
    );
    updated_sdp = updated_sdp.replace(
      new RegExp("a=rtcp-fb:" + codec + " nack\\r\\n", ""),
      ""
    );
    updated_sdp = updated_sdp.replace(
      new RegExp("a=rtcp-fb:" + codec + " nack pli\\r\\n", ""),
      ""
    );
    updated_sdp = updated_sdp.replace(
      new RegExp("a=rtcp-fb:" + codec + " ccm fir\\r\\n", ""),
      ""
    );
    return updated_sdp;
  },

  removeAllButPayloadType: function(sdp, pt) {
    return sdp.replace(
      new RegExp("m=(\\w+ \\w+) UDP/TLS/RTP/SAVPF .*" + pt + ".*\\r\\n", "gi"),
      "m=$1 UDP/TLS/RTP/SAVPF " + pt + "\r\n"
    );
  },

  removeRtpMapForPayloadType: function(sdp, pt) {
    return sdp.replace(new RegExp("a=rtpmap:" + pt + ".*\\r\\n", "gi"), "");
  },

  removeRtcpMux: function(sdp) {
    return sdp.replace(/a=rtcp-mux\r\n/g, "");
  },

  removeSSRCs: function(sdp) {
    return sdp.replace(/a=ssrc.*\r\n/g, "");
  },

  removeBundle: function(sdp) {
    return sdp.replace(/a=group:BUNDLE .*\r\n/g, "");
  },

  reduceAudioMLineToPcmuPcma: function(sdp) {
    return sdp.replace(
      /m=audio .*\r\n/g,
      "m=audio 9 UDP/TLS/RTP/SAVPF 0 8\r\n"
    );
  },

  setAllMsectionsInactive: function(sdp) {
    return sdp
      .replace(/\r\na=sendrecv/g, "\r\na=inactive")
      .replace(/\r\na=sendonly/g, "\r\na=inactive")
      .replace(/\r\na=recvonly/g, "\r\na=inactive");
  },

  removeAllRtpMaps: function(sdp) {
    return sdp.replace(/a=rtpmap:.*\r\n/g, "");
  },

  reduceAudioMLineToDynamicPtAndOpus: function(sdp) {
    return sdp.replace(
      /m=audio .*\r\n/g,
      "m=audio 9 UDP/TLS/RTP/SAVPF 101 109\r\n"
    );
  },

  addTiasBps: function(sdp, bps) {
    return sdp.replace(/c=IN (.*)\r\n/g, "c=IN $1\r\nb=TIAS:" + bps + "\r\n");
  },

  removeSimulcastProperties: function(sdp) {
    return sdp
      .replace(/a=simulcast:.*\r\n/g, "")
      .replace(/a=rid:.*\r\n/g, "")
      .replace(
        /a=extmap:[^\s]* urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id.*\r\n/g,
        ""
      );
  },

  transferSimulcastProperties: function(offer_sdp, answer_sdp) {
    if (!offer_sdp.includes("a=simulcast:")) {
      return answer_sdp;
    }
    ok(
      offer_sdp.includes("a=simulcast: send rid"),
      "Offer contains simulcast attribute"
    );
    var o_simul = offer_sdp.match(/simulcast: send rid=(.*)([\n$])*/i);
    var new_answer_sdp =
      answer_sdp + "a=simulcast: recv rid=" + o_simul[1] + "\r\n";
    ok(offer_sdp.includes("a=rid:"), "Offer contains RID attribute");
    var o_rids = offer_sdp.match(/a=rid:(.*)/gi);
    o_rids.forEach(o_rid => {
      new_answer_sdp = new_answer_sdp + o_rid.replace(/send/, "recv") + "\r\n";
    });
    var extmap_id = offer_sdp.match(
      "a=extmap:([0-9+])/sendonly urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id"
    );
    ok(extmap_id != null, "Offer contains RID RTP header extension");
    new_answer_sdp =
      new_answer_sdp +
      "a=extmap:" +
      extmap_id[1] +
      "/recvonly urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id\r\n";
    return new_answer_sdp;
  },

  verifySdp: function(
    desc,
    expectedType,
    offerConstraintsList,
    offerOptions,
    testOptions
  ) {
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
    ok(
      !desc.sdp.includes(LOOPBACK_ADDR),
      "loopback interface is absent from SDP"
    );
    var requiresTrickleIce = !desc.sdp.includes("a=candidate");
    if (requiresTrickleIce) {
      info("No ICE candidate in SDP -> requiring trickle ICE");
    } else {
      info("at least one ICE candidate is present in SDP");
    }

    //TODO: how can we check for absence/presence of m=application?

    var audioTracks =
      sdputils.countTracksInConstraint("audio", offerConstraintsList) ||
      (offerOptions && offerOptions.offerToReceiveAudio ? 1 : 0);

    info("expected audio tracks: " + audioTracks);
    if (audioTracks == 0) {
      ok(!desc.sdp.includes("m=audio"), "audio m-line is absent from SDP");
    } else {
      ok(desc.sdp.includes("m=audio"), "audio m-line is present in SDP");
      is(
        testOptions.opus,
        desc.sdp.includes("a=rtpmap:109 opus/48000/2"),
        "OPUS codec is present in SDP"
      );
      //TODO: ideally the rtcp-mux should be for the m=audio, and not just
      //      anywhere in the SDP (JS SDP parser bug 1045429)
      is(
        testOptions.rtcpmux,
        desc.sdp.includes("a=rtcp-mux"),
        "RTCP Mux is offered in SDP"
      );
    }

    var videoTracks =
      sdputils.countTracksInConstraint("video", offerConstraintsList) ||
      (offerOptions && offerOptions.offerToReceiveVideo ? 1 : 0);

    info("expected video tracks: " + videoTracks);
    if (videoTracks == 0) {
      ok(!desc.sdp.includes("m=video"), "video m-line is absent from SDP");
    } else {
      ok(desc.sdp.includes("m=video"), "video m-line is present in SDP");
      if (testOptions.h264) {
        ok(
          desc.sdp.includes("a=rtpmap:126 H264/90000") ||
            desc.sdp.includes("a=rtpmap:97 H264/90000"),
          "H.264 codec is present in SDP"
        );
      } else {
        ok(
          desc.sdp.includes("a=rtpmap:120 VP8/90000") ||
            desc.sdp.includes("a=rtpmap:121 VP9/90000"),
          "VP8 or VP9 codec is present in SDP"
        );
      }
      is(
        testOptions.rtcpmux,
        desc.sdp.includes("a=rtcp-mux"),
        "RTCP Mux is offered in SDP"
      );
      is(
        testOptions.ssrc,
        desc.sdp.includes("a=ssrc"),
        "a=ssrc signaled in SDP"
      );
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
