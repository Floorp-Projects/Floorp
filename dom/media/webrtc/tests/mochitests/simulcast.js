"use strict";
/* Helper functions to munge SDP and split the sending track into
 * separate tracks on the receiving end. This can be done in a number
 * of ways, the one used here uses the fact that the MID and RID header
 * extensions which are used for packet routing share the same wire
 * format. The receiver interprets the rids from the sender as mids
 * which allows receiving the different spatial resolutions on separate
 * m-lines and tracks.
 */

// Adapted from wpt to improve handling of cases where answerer is the
// simulcast sender, better handling of a=setup and direction attributes, and
// some simplification. Will probably end up merging back at some point.

const ridExtensions = [
  "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id",
  "urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id",
];

function ridToMid(sdpString) {
  const sections = SDPUtils.splitSections(sdpString);
  const dtls = SDPUtils.getDtlsParameters(sections[1], sections[0]);
  const ice = SDPUtils.getIceParameters(sections[1], sections[0]);
  const rtpParameters = SDPUtils.parseRtpParameters(sections[1]);
  const setupValue = sdpString.match(/a=setup:(.*)/)[1];
  const directionValue =
    sdpString.match(/a=sendrecv|a=sendonly|a=recvonly|a=inactive/) ||
    "a=sendrecv";

  // Skip mid extension; we are replacing it with the rid extmap
  rtpParameters.headerExtensions = rtpParameters.headerExtensions.filter(
    ext => {
      return ext.uri != "urn:ietf:params:rtp-hdrext:sdes:mid";
    }
  );

  rtpParameters.headerExtensions.forEach(ext => {
    if (ext.uri == "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id") {
      ext.uri = "urn:ietf:params:rtp-hdrext:sdes:mid";
    }
  });

  let rids = Array.from(sdpString.matchAll(/a=rid:(.*) send/g)).map(r => r[1]);

  let sdp =
    SDPUtils.writeSessionBoilerplate() +
    SDPUtils.writeDtlsParameters(dtls, setupValue) +
    SDPUtils.writeIceParameters(ice) +
    "a=group:BUNDLE " +
    rids.join(" ") +
    "\r\n";
  const baseRtpDescription = SDPUtils.writeRtpDescription(
    "video",
    rtpParameters
  );
  rids.forEach(rid => {
    sdp +=
      baseRtpDescription +
      "a=mid:" +
      rid +
      "\r\n" +
      "a=msid:rid-" +
      rid +
      " rid-" +
      rid +
      "\r\n";
    sdp += directionValue + "\r\n";
  });

  return sdp;
}

function midToRid(sdpString) {
  const sections = SDPUtils.splitSections(sdpString);
  const dtls = SDPUtils.getDtlsParameters(sections[1], sections[0]);
  const ice = SDPUtils.getIceParameters(sections[1], sections[0]);
  const rtpParameters = SDPUtils.parseRtpParameters(sections[1]);
  const setupValue = sdpString.match(/a=setup:(.*)/)[1];
  const directionValue =
    sdpString.match(/a=sendrecv|a=sendonly|a=recvonly|a=inactive/) ||
    "a=sendrecv";

  // Skip rid extensions; we are replacing them with the mid extmap
  rtpParameters.headerExtensions = rtpParameters.headerExtensions.filter(
    ext => {
      return !ridExtensions.includes(ext.uri);
    }
  );

  rtpParameters.headerExtensions.forEach(ext => {
    if (ext.uri == "urn:ietf:params:rtp-hdrext:sdes:mid") {
      ext.uri = "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id";
    }
  });

  let mids = [];
  for (let i = 1; i < sections.length; i++) {
    mids.push(SDPUtils.getMid(sections[i]));
  }

  let sdp =
    SDPUtils.writeSessionBoilerplate() +
    SDPUtils.writeDtlsParameters(dtls, setupValue) +
    SDPUtils.writeIceParameters(ice) +
    "a=group:BUNDLE " +
    mids[0] +
    "\r\n";
  sdp += SDPUtils.writeRtpDescription("video", rtpParameters);
  // Although we are converting mids to rids, we still need a mid.
  // The first one will be consistent with trickle ICE candidates.
  sdp += "a=mid:" + mids[0] + "\r\n";
  sdp += directionValue + "\r\n";

  mids.forEach(mid => {
    sdp += "a=rid:" + mid + " recv\r\n";
  });
  sdp += "a=simulcast:recv " + mids.join(";") + "\r\n";

  return sdp;
}

// This would be useful for cases other than simulcast, but we do not use it
// anywhere else right now, nor do we have a place for wpt-friendly helpers at
// the moment.
function createPlaybackElement(track) {
  const elem = document.createElement(track.kind);
  elem.autoplay = true;
  elem.srcObject = new MediaStream([track]);
  elem.id = track.id;
  return elem;
}

async function getPlaybackWithLoadedMetadata(track) {
  const elem = createPlaybackElement(track);
  return new Promise(resolve => {
    elem.addEventListener("loadedmetadata", () => {
      resolve(elem);
    });
  });
}
