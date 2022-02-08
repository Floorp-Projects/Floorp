/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is mostly so test_peerConnection_gatherWithStun300.html and
// test_peerConnection_gatherWithStun300IPv6 can share this code. I would have
// put the ipv6 test code in the same file, but our ipv6 tester support is
// inconsistent enough that we need to be able to track the ipv6 test
// separately.

const findStatsRelayCandidates = async (pc, protocol) => {
  const stats = await pc.getStats();
  return [...stats.values()].filter(
    v =>
      v.type == "local-candidate" &&
      v.candidateType == "relay" &&
      v.relayProtocol == protocol
  );
};

// Trickles candidates if pcDst is set, and resolves the candidate list
const trickleIce = async (pc, pcDst) => {
  const candidates = [],
    addCandidatePromises = [];
  while (true) {
    const { candidate } = await new Promise(r =>
      pc.addEventListener("icecandidate", r, { once: true })
    );
    if (!candidate) {
      break;
    }
    candidates.push(candidate);
    if (pcDst) {
      addCandidatePromises.push(pcDst.addIceCandidate(candidate));
    }
  }
  await Promise.all(addCandidatePromises);
  return candidates;
};

const gather = async pc => {
  if (pc.signalingState == "stable") {
    await pc.setLocalDescription(
      await pc.createOffer({ offerToReceiveAudio: true })
    );
  } else if (pc.signalingState == "have-remote-offer") {
    await pc.setLocalDescription();
  }

  return trickleIce(pc);
};

const gatherWithTimeout = async (pc, timeout, context) => {
  const throwOnTimeout = async () => {
    await wait(timeout);
    throw new Error(
      `Gathering did not complete within ${timeout} ms with ${context}`
    );
  };

  return Promise.race([gather(pc), throwOnTimeout()]);
};

const iceConnected = async pc => {
  return new Promise((resolve, reject) => {
    pc.addEventListener("iceconnectionstatechange", () => {
      if (["connected", "completed"].includes(pc.iceConnectionState)) {
        resolve();
      } else if (pc.iceConnectionState == "failed") {
        reject(new Error(`ICE failed`));
      }
    });
  });
};

const connect = async (offerer, answerer) => {
  const trickle1 = trickleIce(offerer, answerer);
  const trickle2 = trickleIce(answerer, offerer);
  const offer = await offerer.createOffer({ offerToReceiveAudio: true });
  await offerer.setLocalDescription(offer);
  await answerer.setRemoteDescription(offer);
  const answer = await answerer.createAnswer();
  await Promise.all([
    trickle1,
    trickle2,
    offerer.setRemoteDescription(answer),
    answerer.setLocalDescription(answer),
    iceConnected(offerer),
    iceConnected(answerer),
  ]);
};

const connectWithTimeout = async (offerer, answerer, timeout, context) => {
  const throwOnTimeout = async () => {
    await wait(timeout);
    throw new Error(
      `ICE did not complete within ${timeout} ms with ${context}`
    );
  };

  return Promise.race([connect(offerer, answerer), throwOnTimeout()]);
};

const isV6HostCandidate = candidate => {
  const fields = candidate.candidate.split(" ");
  const type = fields[7];
  const ipAddress = fields[4];
  return type == "host" && ipAddress.includes(":");
};

const ipv6Supported = async () => {
  const pc = new RTCPeerConnection();
  const candidates = await gatherWithTimeout(pc, 8000);
  info(`baseline candidates: ${JSON.stringify(candidates)}`);
  pc.close();
  return candidates.some(isV6HostCandidate);
};

const makeContextString = iceServers => {
  const currentRedirectAddress = SpecialPowers.getCharPref(
    "media.peerconnection.nat_simulator.redirect_address",
    ""
  );
  const currentRedirectTargets = SpecialPowers.getCharPref(
    "media.peerconnection.nat_simulator.redirect_targets",
    ""
  );
  return `redirect rule: ${currentRedirectAddress}=>${currentRedirectTargets} iceServers: ${JSON.stringify(
    iceServers
  )}`;
};

const checkSrflx = async iceServers => {
  const context = makeContextString(iceServers);
  info(`checkSrflx ${context}`);
  const pc = new RTCPeerConnection({
    iceServers,
    bundlePolicy: "max-bundle", // Avoids extra candidates
  });
  const candidates = await gatherWithTimeout(pc, 8000, context);
  const srflxCandidates = candidates.filter(c => c.candidate.includes("srflx"));
  info(`candidates: ${JSON.stringify(srflxCandidates)}`);
  // TODO(bug 1339203): Once we support rtcpMuxPolicy, set it to "require" to
  // result in a single srflx candidate
  is(
    srflxCandidates.length,
    2,
    `Should have two srflx candidates with ${context}`
  );
  pc.close();
};

const checkNoSrflx = async iceServers => {
  const context = makeContextString(iceServers);
  info(`checkNoSrflx ${context}`);
  const pc = new RTCPeerConnection({
    iceServers,
    bundlePolicy: "max-bundle", // Avoids extra candidates
  });
  const candidates = await gatherWithTimeout(pc, 8000, context);
  const srflxCandidates = candidates.filter(c => c.candidate.includes("srflx"));
  info(`candidates: ${JSON.stringify(srflxCandidates)}`);
  is(
    srflxCandidates.length,
    0,
    `Should have no srflx candidates with ${context}`
  );
  pc.close();
};

const checkRelayUdp = async iceServers => {
  const context = makeContextString(iceServers);
  info(`checkRelayUdp ${context}`);
  const pc = new RTCPeerConnection({
    iceServers,
    bundlePolicy: "max-bundle", // Avoids extra candidates
  });
  const candidates = await gatherWithTimeout(pc, 8000, context);
  const relayCandidates = candidates.filter(c => c.candidate.includes("relay"));
  info(`candidates: ${JSON.stringify(relayCandidates)}`);
  // TODO(bug 1339203): Once we support rtcpMuxPolicy, set it to "require" to
  // result in a single relay candidate
  is(
    relayCandidates.length,
    2,
    `Should have two relay candidates with ${context}`
  );
  // It would be nice if RTCIceCandidate had a field telling us what the
  // "related protocol" is (similar to relatedAddress and relatedPort).
  // Because there is no such thing, we need to go through the stats API,
  // which _does_ have that information.
  is(
    (await findStatsRelayCandidates(pc, "tcp")).length,
    0,
    `No TCP relay candidates should be present with ${context}`
  );
  pc.close();
};

const checkRelayTcp = async iceServers => {
  const context = makeContextString(iceServers);
  info(`checkRelayTcp ${context}`);
  const pc = new RTCPeerConnection({
    iceServers,
    bundlePolicy: "max-bundle", // Avoids extra candidates
  });
  const candidates = await gatherWithTimeout(pc, 8000, context);
  const relayCandidates = candidates.filter(c => c.candidate.includes("relay"));
  info(`candidates: ${JSON.stringify(relayCandidates)}`);
  // TODO(bug 1339203): Once we support rtcpMuxPolicy, set it to "require" to
  // result in a single relay candidate
  is(
    relayCandidates.length,
    2,
    `Should have two relay candidates with ${context}`
  );
  // It would be nice if RTCIceCandidate had a field telling us what the
  // "related protocol" is (similar to relatedAddress and relatedPort).
  // Because there is no such thing, we need to go through the stats API,
  // which _does_ have that information.
  is(
    (await findStatsRelayCandidates(pc, "udp")).length,
    0,
    `No UDP relay candidates should be present with ${context}`
  );
  pc.close();
};

const checkRelayUdpTcp = async iceServers => {
  const context = makeContextString(iceServers);
  info(`checkRelayUdpTcp ${context}`);
  const pc = new RTCPeerConnection({
    iceServers,
    bundlePolicy: "max-bundle", // Avoids extra candidates
  });
  const candidates = await gatherWithTimeout(pc, 8000, context);
  const relayCandidates = candidates.filter(c => c.candidate.includes("relay"));
  info(`candidates: ${JSON.stringify(relayCandidates)}`);
  // TODO(bug 1339203): Once we support rtcpMuxPolicy, set it to "require" to
  // result in a single relay candidate each for UDP and TCP
  is(
    relayCandidates.length,
    4,
    `Should have two relay candidates for each protocol with ${context}`
  );
  // It would be nice if RTCIceCandidate had a field telling us what the
  // "related protocol" is (similar to relatedAddress and relatedPort).
  // Because there is no such thing, we need to go through the stats API,
  // which _does_ have that information.
  is(
    (await findStatsRelayCandidates(pc, "udp")).length,
    2,
    `Two UDP relay candidates should be present with ${context}`
  );
  // TODO(bug 1705563): This is 1 because of bug 1705563
  is(
    (await findStatsRelayCandidates(pc, "tcp")).length,
    1,
    `One TCP relay candidates should be present with ${context}`
  );
  pc.close();
};

const checkNoRelay = async iceServers => {
  const context = makeContextString(iceServers);
  info(`checkNoRelay ${context}`);
  const pc = new RTCPeerConnection({
    iceServers,
    bundlePolicy: "max-bundle", // Avoids extra candidates
  });
  const candidates = await gatherWithTimeout(pc, 8000, context);
  const relayCandidates = candidates.filter(c => c.candidate.includes("relay"));
  info(`candidates: ${JSON.stringify(relayCandidates)}`);
  is(
    relayCandidates.length,
    0,
    `Should have no relay candidates with ${context}`
  );
  pc.close();
};
