/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const statsExpectedByType = {
  "inbound-rtp": {
    expected: [
      "id",
      "timestamp",
      "type",
      "ssrc",
      "mediaType",
      "kind",
      "packetsReceived",
      "packetsLost",
      "packetsDiscarded",
      "bytesReceived",
      "jitter",
    ],
    optional: ["remoteId", "nackCount"],
    localVideoOnly: [
      "firCount",
      "pliCount",
      "framesDecoded",
      "discardedPackets",
    ],
    unimplemented: [
      "mediaTrackId",
      "transportId",
      "codecId",
      "associateStatsId",
      "sliCount",
      "packetsRepaired",
      "fractionLost",
      "burstPacketsLost",
      "burstLossCount",
      "burstDiscardCount",
      "gapDiscardRate",
      "gapLossRate",
      // Not yet implemented for inbound media, see bug 1519590
      "qpSum",
    ],
    deprecated: ["mozRtt", "isRemote"],
  },
  "outbound-rtp": {
    expected: [
      "id",
      "timestamp",
      "type",
      "ssrc",
      "mediaType",
      "kind",
      "packetsSent",
      "bytesSent",
      "remoteId",
    ],
    optional: ["nackCount"],
    localVideoOnly: ["framesEncoded", "firCount", "pliCount", "qpSum"],
    unimplemented: [
      "mediaTrackId",
      "transportId",
      "codecId",
      "sliCount",
      "targetBitrate",
    ],
    deprecated: ["isRemote"],
  },
  "remote-inbound-rtp": {
    expected: [
      "id",
      "timestamp",
      "type",
      "ssrc",
      "mediaType",
      "kind",
      "packetsLost",
      "jitter",
      "localId",
    ],
    optional: ["roundTripTime", "nackCount", "packetsReceived"],
    unimplemented: [
      "mediaTrackId",
      "transportId",
      "codecId",
      "packetsDiscarded",
      "associateStatsId",
      "sliCount",
      "packetsRepaired",
      "fractionLost",
      "burstPacketsLost",
      "burstLossCount",
      "burstDiscardCount",
      "gapDiscardRate",
      "gapLossRate",
    ],
    deprecated: ["mozRtt", "isRemote"],
  },
  "remote-outbound-rtp": {
    expected: [
      "id",
      "timestamp",
      "type",
      "ssrc",
      "mediaType",
      "kind",
      "packetsSent",
      "bytesSent",
      "localId",
      "remoteTimestamp",
    ],
    optional: ["nackCount"],
    unimplemented: [
      "mediaTrackId",
      "transportId",
      "codecId",
      "sliCount",
      "targetBitrate",
    ],
    deprecated: ["isRemote"],
  },
  csrc: { skip: true },
  codec: { skip: true },
  "peer-connection": { skip: true },
  "data-channel": { skip: true },
  track: { skip: true },
  transport: { skip: true },
  "candidate-pair": {
    expected: [
      "id",
      "timestamp",
      "type",
      "transportId",
      "localCandidateId",
      "remoteCandidateId",
      "state",
      "priority",
      "nominated",
      "writable",
      "readable",
      "bytesSent",
      "bytesReceived",
      "lastPacketSentTimestamp",
      "lastPacketReceivedTimestamp",
    ],
    optional: ["selected"],
    unimplemented: [
      "totalRoundTripTime",
      "currentRoundTripTime",
      "availableOutgoingBitrate",
      "availableIncomingBitrate",
      "requestsReceived",
      "requestsSent",
      "responsesReceived",
      "responsesSent",
      "retransmissionsReceived",
      "retransmissionsSent",
      "consentRequestsSent",
    ],
    deprecated: [],
  },
  "local-candidate": {
    expected: [
      "id",
      "timestamp",
      "type",
      "address",
      "protocol",
      "port",
      "candidateType",
      "priority",
    ],
    optional: ["relayProtocol", "proxied"],
    unimplemented: ["networkType", "url", "transportId"],
    deprecated: [
      "candidateId",
      "portNumber",
      "ipAddress",
      "componentId",
      "mozLocalTransport",
      "transport",
    ],
  },
  "remote-candidate": {
    expected: [
      "id",
      "timestamp",
      "type",
      "address",
      "protocol",
      "port",
      "candidateType",
      "priority",
    ],
    optional: ["relayProtocol", "proxied"],
    unimplemented: ["networkType", "url", "transportId"],
    deprecated: [
      "candidateId",
      "portNumber",
      "ipAddress",
      "componentId",
      "mozLocalTransport",
      "transport",
    ],
  },
  certificate: { skip: true },
};

["in", "out"].forEach(pre => {
  let s = statsExpectedByType[pre + "bound-rtp"];
  s.optional = [...s.optional, ...s.localVideoOnly];
});

//
//  Checks that the fields in a report conform to the expectations in
// statExpectedByType
//
function checkExpectedFields(report) {
  report.forEach(stat => {
    let expectations = statsExpectedByType[stat.type];
    ok(expectations, "Stats type " + stat.type + " was expected");
    // If the type is not expected or if it is flagged for skipping continue to
    // the next
    if (!expectations || expectations.skip) {
      return;
    }
    // Check that all required fields exist
    expectations.expected.forEach(field => {
      ok(
        field in stat,
        "Expected stat field " + stat.type + "." + field + " exists"
      );
    });
    // Check that each field is either expected or optional
    let allowed = [...expectations.expected, ...expectations.optional];
    Object.keys(stat).forEach(field => {
      ok(
        allowed.includes(field),
        "Stat field " +
          stat.type +
          "." +
          field +
          ` is allowed. ${JSON.stringify(stat)}`
      );
    });

    //
    // Ensure that unimplemented fields are not implemented
    //   note: if a field is implemented it should be moved to expected or
    //   optional.
    //
    expectations.unimplemented.forEach(field => {
      ok(
        !Object.keys(stat).includes(field),
        "Unimplemented field " + stat.type + "." + field + " does not exist."
      );
    });

    //
    // Ensure that all deprecated fields are not present
    //
    expectations.deprecated.forEach(field => {
      ok(
        !Object.keys(stat).includes(field),
        "Deprecated field " + stat.type + "." + field + " does not exist."
      );
    });
  });
}

function pedanticChecks(report) {
  // Check that report is only-maplike
  [...report.keys()].forEach(key =>
    is(
      report[key],
      undefined,
      `Report is not dictionary like, it lacks a property for key ${key}`
    )
  );
  report.forEach((statObj, mapKey) => {
    info(`"${mapKey} = ${JSON.stringify(statObj, null, 2)}`);
  });
  // eslint-disable-next-line complexity
  report.forEach((statObj, mapKey) => {
    let tested = {};
    // Record what fields get tested.
    // To access a field foo without marking it as tested use stat.inner.foo
    let stat = new Proxy(statObj, {
      get(stat, key) {
        if (key == "inner") {
          return stat;
        }
        tested[key] = true;
        return stat[key];
      },
    });

    let expectations = statsExpectedByType[stat.type];

    if (expectations.skip) {
      return;
    }

    // All stats share the following attributes inherited from RTCStats
    is(stat.id, mapKey, stat.type + ".id is the same as the report key.");

    // timestamp
    ok(stat.timestamp >= 0, stat.type + ".timestamp is not less than 0");
    // If the timebase for the timestamp is not properly set the timestamp
    // will appear relative to the year 1970; Bug 1495446
    const date = new Date(stat.timestamp);
    ok(
      date.getFullYear() > 1970,
      `${stat.type}.timestamp is relative to current time, date=${date}`
    );
    //
    // RTCStreamStats attributes with common behavior
    //
    // inbound-rtp, outbound-rtp, remote-inbound-rtp, remote-outbound-rtp
    // inherit from RTCStreamStats
    if (
      [
        "inbound-rtp",
        "outbound-rtp",
        "remote-inbound-rtp",
        "remote-outbound-rtp",
      ].includes(stat.type)
    ) {
      const isRemote = stat.type.startsWith("remote-");
      //
      // Common RTCStreamStats fields
      //

      // SSRC
      ok(stat.ssrc, stat.type + ".ssrc has a value");

      // kind
      ok(
        ["audio", "video"].includes(stat.kind),
        stat.type + ".kind is 'audio' or 'video'"
      );

      // mediaType, renamed to kind but remains for backward compability.
      ok(
        ["audio", "video"].includes(stat.mediaType),
        stat.type + ".mediaType is 'audio' or 'video'"
      );

      ok(stat.kind == stat.mediaType, "kind equals legacy mediaType");

      if (isRemote) {
        // local id
        if (stat.localId) {
          ok(
            report.has(stat.localId),
            `localId ${stat.localId} exists in report.`
          );
          is(
            report.get(stat.localId).ssrc,
            stat.ssrc,
            "remote ssrc and local ssrc match."
          );
          is(
            report.get(stat.localId).remoteId,
            stat.id,
            "local object has remote object as it's own remote object."
          );
        }
      } else {
        // remote id
        if (stat.remoteId) {
          ok(
            report.has(stat.remoteId),
            `remoteId ${stat.remoteId} exists in report.`
          );
          is(
            report.get(stat.remoteId).ssrc,
            stat.ssrc,
            "remote ssrc and local ssrc match."
          );
          is(
            report.get(stat.remoteId).localId,
            stat.id,
            "remote object has local object as it's own local object."
          );
        }
      }

      // nackCount
      if (stat.nackCount) {
        ok(
          stat.nackCount >= 0,
          `${stat.type}.nackCount is sane (${stat.kind}).`
        );
      }

      if (!isRemote && stat.inner.kind == "video") {
        // firCount
        ok(
          stat.firCount >= 0 && stat.firCount < 100,
          `${stat.type}.firCount is a sane number for a short ` +
            `${stat.kind} test. value=${stat.firCount}`
        );

        // pliCount
        ok(
          stat.pliCount >= 0 && stat.pliCount < 200,
          `${stat.type}.pliCount is a sane number for a short ` +
            `${stat.kind} test. value=${stat.pliCount}`
        );
      }
    }

    if (stat.type == "inbound-rtp") {
      //
      // Required fields
      //

      // packetsReceived
      ok(
        stat.packetsReceived >= 0 && stat.packetsReceived < 10 ** 5,
        `${stat.type}.packetsReceived is a sane number for a short ` +
          `${stat.kind} test. value=${stat.packetsReceived}`
      );

      // packetsDiscarded
      ok(
        stat.packetsDiscarded >= 0 && stat.packetsDiscarded < 100,
        `${stat.type}.packetsDiscarded is sane number for a short test. ` +
          `value=${stat.packetsDiscarded}`
      );

      // bytesReceived
      ok(
        stat.bytesReceived >= 0 && stat.bytesReceived < 10 ** 9, // Not a magic number, just a guess
        `${stat.type}.bytesReceived is a sane number for a short ` +
          `${stat.kind} test. value=${stat.bytesReceived}`
      );

      // packetsLost
      ok(
        stat.packetsLost < 100,
        `${stat.type}.packetsLost is a sane number for a short ` +
          `${stat.kind} test. value=${stat.packetsLost}`
      );

      // This should be much lower for audio, TODO: Bug 1330575
      let expectedJitter = stat.kind == "video" ? 0.5 : 1;
      // jitter
      ok(
        stat.jitter < expectedJitter,
        `${stat.type}.jitter is sane number for a ${stat.kind} ` +
          `local only test. value=${stat.jitter}`
      );

      //
      // Optional fields
      //

      //
      // Local video only stats
      //
      if (stat.inner.kind != "video") {
        expectations.localVideoOnly.forEach(field => {
          ok(
            stat[field] === undefined,
            `${stat.type} does not have field ${field}` +
              ` when kind is not 'video'`
          );
        });
      } else {
        expectations.localVideoOnly.forEach(field => {
          ok(
            stat.inner[field] !== undefined,
            stat.type + " has field " + field + " when kind is video"
          );
        });
        // discardedPackets
        ok(
          stat.discardedPackets < 100,
          `${stat.type}.discardedPackets is a sane number for a short test. ` +
            `value=${stat.discardedPackets}`
        );
        // framesDecoded
        ok(
          stat.framesDecoded > 0 && stat.framesDecoded < 1000000,
          `${stat.type}.framesDecoded is a sane number for a short ` +
            `${stat.kind} test. value=${stat.framesDecoded}`
        );
      }
    } else if (stat.type == "remote-inbound-rtp") {
      // roundTripTime
      ok(
        stat.roundTripTime >= 0,
        `${stat.type}.roundTripTime is sane with` +
          `value of: ${stat.roundTripTime} (${stat.kind})`
      );
      //
      // Required fields
      //

      // packetsLost
      ok(
        stat.packetsLost < 100,
        `${stat.type}.packetsLost is a sane number for a short ` +
          `${stat.kind} test. value=${stat.packetsLost}`
      );

      // jitter
      ok(
        stat.jitter >= 0,
        `${stat.type}.jitter is sane number (${stat.kind}). ` +
          `value=${stat.jitter}`
      );

      //
      // Optional fields
      //

      // packetsReceived
      if (stat.packetsReceived) {
        ok(
          stat.packetsReceived >= 0 && stat.packetsReceived < 10 ** 5,
          `${stat.type}.packetsReceived is a sane number for a short ` +
            `${stat.kind} test. value=${stat.packetsReceived}`
        );
      }
    } else if (stat.type == "outbound-rtp") {
      //
      // Required fields
      //

      // packetsSent
      ok(
        stat.packetsSent > 0 && stat.packetsSent < 10000,
        `${stat.type}.packetsSent is a sane number for a short ` +
          `${stat.kind} test. value=${stat.packetsSent}`
      );

      // bytesSent
      const audio1Min = 16000 * 60; // 128kbps
      const video1Min = 250000 * 60; // 2Mbps
      ok(
        stat.bytesSent > 0 &&
          stat.bytesSent < (stat.kind == "video" ? video1Min : audio1Min),
        `${stat.type}.bytesSent is a sane number for a short ` +
          `${stat.kind} test. value=${stat.bytesSent}`
      );

      //
      // Optional fields
      //

      //
      // Local video only stats
      //
      if (stat.inner.kind != "video") {
        expectations.localVideoOnly.forEach(field => {
          ok(
            stat[field] === undefined,
            `${stat.type} does not have field ` +
              `${field} when kind is not 'video'`
          );
        });
      } else {
        expectations.localVideoOnly.forEach(field => {
          ok(
            stat.inner[field] !== undefined,
            `${stat.type} has field ` +
              `${field} when kind is video and isRemote is false`
          );
        });

        // framesEncoded
        ok(
          stat.framesEncoded >= 0 && stat.framesEncoded < 100000,
          `${stat.type}.framesEncoded is a sane number for a short ` +
            `${stat.kind} test. value=${stat.framesEncoded}`
        );

        // qpSum
        // techinically optional but should be supported for all of our
        // codecs (on the encode side, see bug 1519590)
        ok(
          stat.qpSum >= 0,
          `${stat.type} qpSum is a sane number (${stat.kind}). ` +
            `value=${stat.qpSum}`
        );
      }
    } else if (stat.type == "remote-outbound-rtp") {
      //
      // Required fields
      //

      // packetsSent
      ok(
        stat.packetsSent > 0 && stat.packetsSent < 10000,
        `${stat.type}.packetsSent is a sane number for a short ` +
          `${stat.kind} test. value=${stat.packetsSent}`
      );

      // bytesSent
      const audio1Min = 16000 * 60; // 128kbps
      const video1Min = 250000 * 60; // 2Mbps
      ok(
        stat.bytesSent > 0 &&
          stat.bytesSent < (stat.kind == "video" ? video1Min : audio1Min),
        `${stat.type}.bytesSent is a sane number for a short ` +
          `${stat.kind} test. value=${stat.bytesSent}`
      );

      ok(
        stat.remoteTimestamp !== undefined,
        `${stat.type}.remoteTimestamp ` + `is not undefined (${stat.kind})`
      );
      const ageSeconds = (stat.timestamp - stat.remoteTimestamp) / 1000;
      // remoteTimestamp is exact (so it can be mapped to a packet), whereas
      // timestamp has reduced precision. It is possible that
      // remoteTimestamp occurs a millisecond into the future from
      // timestamp. We also subtract half a millisecond when reducing
      // precision on libwebrtc timestamps, to counteract the potential
      // rounding up that libwebrtc may do since it tends to round its
      // internal timestamps to whole milliseconds. In the worst case
      // remoteTimestamp may therefore occur 2 milliseconds ahead of
      // timestamp.
      ok(
        ageSeconds >= -0.002 && ageSeconds < 30,
        `${stat.type}.remoteTimestamp is on the same timeline as ` +
          `${stat.type}.timestamp, and no older than 30 seconds. ` +
          `difference=${ageSeconds}s`
      );
    } else if (stat.type == "candidate-pair") {
      info("candidate-pair is: " + JSON.stringify(stat));
      //
      // Required fields
      //

      // transportId
      ok(
        stat.transportId,
        `${stat.type}.transportId has a value. value=` +
          `${stat.transportId} (${stat.kind})`
      );

      // localCandidateId
      ok(
        stat.localCandidateId,
        `${stat.type}.localCandidateId has a value. value=` +
          `${stat.localCandidateId} (${stat.kind})`
      );

      // remoteCandidateId
      ok(
        stat.remoteCandidateId,
        `${stat.type}.remoteCandidateId has a value. value=` +
          `${stat.remoteCandidateId} (${stat.kind})`
      );

      // priority
      ok(
        stat.priority,
        `${stat.type}.priority has a value. value=` +
          `${stat.priority} (${stat.kind})`
      );

      // readable
      ok(
        stat.readable,
        `${stat.type}.readable is true. value=${stat.readable} ` +
          `(${stat.kind})`
      );

      // writable
      ok(
        stat.writable,
        `${stat.type}.writable is true. value=${stat.writable} ` +
          `(${stat.kind})`
      );

      // state
      if (
        stat.state == "succeeded" &&
        stat.selected !== undefined &&
        stat.selected
      ) {
        info("candidate-pair state is succeeded and selected is true");
        // nominated
        ok(
          stat.nominated,
          `${stat.type}.nominated is true. value=${stat.nominated} ` +
            `(${stat.kind})`
        );

        // bytesSent
        ok(
          stat.bytesSent > 5000,
          `${stat.type}.bytesSent is a sane number (>5,000) for a short ` +
            `${stat.kind} test. value=${stat.bytesSent}`
        );

        // bytesReceived
        ok(
          stat.bytesReceived > 500,
          `${stat.type}.bytesReceived is a sane number (>500) for a short ` +
            `${stat.kind} test. value=${stat.bytesReceived}`
        );

        // lastPacketSentTimestamp
        ok(
          stat.lastPacketSentTimestamp,
          `${stat.type}.lastPacketSentTimestamp has a value. value=` +
            `${stat.lastPacketSentTimestamp} (${stat.kind})`
        );

        // lastPacketReceivedTimestamp
        ok(
          stat.lastPacketReceivedTimestamp,
          `${stat.type}.lastPacketReceivedTimestamp has a value. value=` +
            `${stat.lastPacketReceivedTimestamp} (${stat.kind})`
        );
      } else {
        info("candidate-pair is _not_ both state == succeeded and selected");
        // nominated
        ok(
          stat.nominated !== undefined,
          `${stat.type}.nominated exists. value=${stat.nominated} ` +
            `(${stat.kind})`
        );
        ok(
          stat.bytesSent !== undefined,
          `${stat.type}.bytesSent exists. value=${stat.bytesSent} ` +
            `(${stat.kind})`
        );
        ok(
          stat.bytesReceived !== undefined,
          `${stat.type}.bytesReceived exists. value=${stat.bytesReceived} ` +
            `(${stat.kind})`
        );
        ok(
          stat.lastPacketSentTimestamp !== undefined,
          `${stat.type}.lastPacketSentTimestamp exists. value=` +
            `${stat.lastPacketSentTimestamp} (${stat.kind})`
        );
        ok(
          stat.lastPacketReceivedTimestamp !== undefined,
          `${stat.type}.lastPacketReceivedTimestamp exists. value=` +
            `${stat.lastPacketReceivedTimestamp} (${stat.kind})`
        );
      }

      //
      // Optional fields
      //
      // selected
      ok(
        stat.selected === undefined ||
          (stat.state == "succeeded" && stat.selected) ||
          !stat.selected,
        `${stat.type}.selected is undefined, true when state is succeeded, ` +
          `or false. value=${stat.selected} (${stat.kind})`
      );
    } else if (
      stat.type == "local-candidate" ||
      stat.type == "remote-candidate"
    ) {
      info(`candidate is ${JSON.stringify(stat)}`);

      // address
      ok(
        stat.address,
        `${stat.type} has address. value=${stat.address} ` + `(${stat.kind})`
      );

      // protocol
      ok(
        stat.protocol,
        `${stat.type} has protocol. value=${stat.protocol} ` + `(${stat.kind})`
      );

      // port
      ok(
        stat.port >= 0,
        `${stat.type} has port >= 0. value=${stat.port} ` + `(${stat.kind})`
      );
      ok(
        stat.port <= 65535,
        `${stat.type} has port <= 65535. value=${stat.port} ` + `(${stat.kind})`
      );

      // candidateType
      ok(
        stat.candidateType,
        `${stat.type} has candidateType. value=${stat.candidateType} ` +
          `(${stat.kind})`
      );

      // priority
      ok(
        stat.priority > 0 && stat.priority < 2 ** 32 - 1,
        `${stat.type} has priority between 1 and 2^32 - 1 inc. ` +
          `value=${stat.priority} (${stat.kind})`
      );

      // relayProtocol
      if (stat.type == "local-candidate" && stat.candidateType == "relay") {
        ok(
          stat.relayProtocol,
          `relay ${stat.type} has relayProtocol. value=${stat.relayProtocol} ` +
            `(${stat.kind})`
        );
      } else {
        is(
          stat.relayProtocol,
          undefined,
          `relayProtocol is undefined for candidates that are not relay and ` +
            `local. value=${stat.relayProtocol} (${stat.kind})`
        );
      }

      // proxied
      if (stat.proxied) {
        ok(
          stat.proxied == "proxied" || stat.proxied == "non-proxied",
          `${stat.type} has proxied. value=${stat.proxied} (${stat.kind})`
        );
      }
    }

    //
    // Ensure everything was tested
    //
    [...expectations.expected, ...expectations.optional].forEach(field => {
      ok(
        Object.keys(tested).includes(field),
        `${stat.type}.${field} was tested.`
      );
    });
  });
}

function dumpStats(stats) {
  const dict = {};
  for (const [k, v] of stats.entries()) {
    dict[k] = v;
  }
  info(`Got stats: ${JSON.stringify(dict)}`);
}

async function waitForSyncedRtcp(pc) {
  // Ensures that RTCP is present
  let ensureSyncedRtcp = async () => {
    let report = await pc.getStats();
    for (const v of report.values()) {
      if (v.type.endsWith("bound-rtp") && !(v.remoteId || v.localId)) {
        info(`${v.id} is missing remoteId or localId: ${JSON.stringify(v)}`);
        return null;
      }
      if (v.type == "remote-inbound-rtp" && v.roundTripTime === undefined) {
        info(`${v.id} is missing roundTripTime: ${JSON.stringify(v)}`);
        return null;
      }
    }
    return report;
  };
  // Returns true if there is proof in aStats of rtcp flow for all remote stats
  // objects, compared to baseStats.
  const hasAllRtcpUpdated = (baseStats, stats) => {
    let hasRtcpStats = false;
    for (const v of stats.values()) {
      if (v.type == "remote-outbound-rtp") {
        hasRtcpStats = true;
        if (!v.remoteTimestamp) {
          // `remoteTimestamp` is 0 or not present.
          return false;
        }
        if (v.remoteTimestamp <= baseStats.get(v.id)?.remoteTimestamp) {
          // `remoteTimestamp` has not advanced further than the base stats,
          // i.e., no new sender report has been received.
          return false;
        }
      } else if (v.type == "remote-inbound-rtp") {
        hasRtcpStats = true;
        // The ideal thing here would be to check `reportsReceived`, but it's
        // not yet implemented.
        if (!v.packetsReceived) {
          // `packetsReceived` is 0 or not present.
          return false;
        }
        if (v.packetsReceived <= baseStats.get(v.id)?.packetsReceived) {
          // `packetsReceived` has not advanced further than the base stats,
          // i.e., no new receiver report has been received.
          return false;
        }
      }
    }
    return hasRtcpStats;
  };
  let attempts = 0;
  const baseStats = await pc.getStats();
  // Time-units are MS
  const waitPeriod = 100;
  const maxTime = 20000;
  for (let totalTime = maxTime; totalTime > 0; totalTime -= waitPeriod) {
    try {
      let syncedStats = await ensureSyncedRtcp();
      if (syncedStats && hasAllRtcpUpdated(baseStats, syncedStats)) {
        dumpStats(syncedStats);
        return syncedStats;
      }
    } catch (e) {
      info(e);
      info(e.stack);
      throw e;
    }
    attempts += 1;
    info(`waitForSyncedRtcp: no sync on attempt ${attempts}, retrying.`);
    await wait(waitPeriod);
  }
  throw Error(
    "Waiting for synced RTCP timed out after at least " + maxTime + "ms"
  );
}

function checkSenderStats(senderStats, streamCount) {
  const outboundRtpReports = [];
  const remoteInboundRtpReports = [];
  for (const v of senderStats.values()) {
    if (v.type == "outbound-rtp") {
      outboundRtpReports.push(v);
    } else if (v.type == "remote-inbound-rtp") {
      remoteInboundRtpReports.push(v);
    }
  }
  is(
    outboundRtpReports.length,
    streamCount,
    `Sender with ${streamCount} simulcast streams has ${streamCount} outbound-rtp reports`
  );
  is(
    remoteInboundRtpReports.length,
    streamCount,
    `Sender with ${streamCount} simulcast streams has ${streamCount} remote-inbound-rtp reports`
  );
  for (const outboundRtpReport of outboundRtpReports) {
    is(
      outboundRtpReports.filter(r => r.ssrc == outboundRtpReport.ssrc).length,
      1,
      "Simulcast send track SSRCs are distinct"
    );
    const remoteReports = remoteInboundRtpReports.filter(
      r => r.id == outboundRtpReport.remoteId
    );
    is(
      remoteReports.length,
      1,
      "Simulcast send tracks have exactly one remote counterpart"
    );
    const remoteInboundRtpReport = remoteReports[0];
    is(
      outboundRtpReport.ssrc,
      remoteInboundRtpReport.ssrc,
      "SSRC matches for outbound-rtp and remote-inbound-rtp"
    );
  }
}

function PC_LOCAL_TEST_LOCAL_STATS(test) {
  return waitForSyncedRtcp(test.pcLocal._pc).then(stats => {
    checkExpectedFields(stats);
    pedanticChecks(stats);
    return Promise.all([
      test.pcLocal._pc.getSenders().map(async s => {
        checkSenderStats(
          await s.getStats(),
          Math.max(1, s.getParameters()?.encodings?.length ?? 0)
        );
      }),
    ]);
  });
}

function PC_REMOTE_TEST_REMOTE_STATS(test) {
  return waitForSyncedRtcp(test.pcRemote._pc).then(stats => {
    checkExpectedFields(stats);
    pedanticChecks(stats);
    return Promise.all([
      test.pcRemote._pc.getSenders().map(async s => {
        checkSenderStats(
          await s.getStats(),
          Math.max(1, s.getParameters()?.encodings?.length ?? 0)
        );
      }),
    ]);
  });
}
