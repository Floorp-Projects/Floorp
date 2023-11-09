/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const statsExpectedByType = {
  "inbound-rtp": {
    expected: [
      "trackIdentifier",
      "id",
      "timestamp",
      "type",
      "ssrc",
      "mediaType",
      "kind",
      "codecId",
      "packetsReceived",
      "packetsLost",
      "packetsDiscarded",
      "bytesReceived",
      "jitter",
      "lastPacketReceivedTimestamp",
      "headerBytesReceived",
      // Always missing from libwebrtc stats
      // "estimatedPlayoutTimestamp",
      "jitterBufferDelay",
      "jitterBufferEmittedCount",
    ],
    optional: ["remoteId", "nackCount", "qpSum"],
    localVideoOnly: [
      "firCount",
      "pliCount",
      "framesDecoded",
      "framesDropped",
      "discardedPackets",
      "framesPerSecond",
      "frameWidth",
      "frameHeight",
      "framesReceived",
      "totalDecodeTime",
      "totalInterFrameDelay",
      "totalProcessingDelay",
      "totalSquaredInterFrameDelay",
    ],
    localAudioOnly: [
      "totalSamplesReceived",
      // libwebrtc doesn't seem to do FEC for video
      "fecPacketsReceived",
      "fecPacketsDiscarded",
      "concealedSamples",
      "silentConcealedSamples",
      "concealmentEvents",
      "insertedSamplesForDeceleration",
      "removedSamplesForAcceleration",
      "audioLevel",
      "totalAudioEnergy",
      "totalSamplesDuration",
    ],
    unimplemented: [
      "mediaTrackId",
      "transportId",
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
  "outbound-rtp": {
    expected: [
      "id",
      "timestamp",
      "type",
      "ssrc",
      "mediaType",
      "kind",
      "codecId",
      "packetsSent",
      "bytesSent",
      "remoteId",
      "headerBytesSent",
      "retransmittedPacketsSent",
      "retransmittedBytesSent",
    ],
    optional: ["nackCount", "qpSum"],
    localAudioOnly: [],
    localVideoOnly: [
      "framesEncoded",
      "firCount",
      "pliCount",
      "frameWidth",
      "frameHeight",
      "framesPerSecond",
      "framesSent",
      "hugeFramesSent",
      "totalEncodeTime",
      "totalEncodedBytesTarget",
    ],
    unimplemented: ["mediaTrackId", "transportId", "sliCount", "targetBitrate"],
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
      "codecId",
      "packetsLost",
      "jitter",
      "localId",
      "totalRoundTripTime",
      "fractionLost",
      "roundTripTimeMeasurements",
    ],
    optional: ["roundTripTime", "nackCount", "packetsReceived"],
    unimplemented: [
      "mediaTrackId",
      "transportId",
      "packetsDiscarded",
      "associateStatsId",
      "sliCount",
      "packetsRepaired",
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
      "codecId",
      "packetsSent",
      "bytesSent",
      "localId",
      "remoteTimestamp",
    ],
    optional: ["nackCount"],
    unimplemented: ["mediaTrackId", "transportId", "sliCount", "targetBitrate"],
    deprecated: ["isRemote"],
  },
  "media-source": {
    expected: ["id", "timestamp", "type", "trackIdentifier", "kind"],
    unimplemented: [
      "audioLevel",
      "totalAudioEnergy",
      "totalSamplesDuration",
      "echoReturnLoss",
      "echoReturnLossEnhancement",
      "droppedSamplesDuration",
      "droppedSamplesEvents",
      "totalCaptureDelay",
      "totalSamplesCaptured",
    ],
    localAudioOnly: [],
    localVideoOnly: ["frames", "framesPerSecond", "width", "height"],
    optional: [],
    deprecated: [],
  },
  csrc: { skip: true },
  codec: {
    expected: [
      "timestamp",
      "type",
      "id",
      "payloadType",
      "transportId",
      "mimeType",
      "clockRate",
      "sdpFmtpLine",
    ],
    optional: ["codecType", "channels"],
    unimplemented: [],
    deprecated: [],
  },
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

["inbound-rtp", "outbound-rtp", "media-source"].forEach(type => {
  let s = statsExpectedByType[type];
  s.optional = [...s.optional, ...s.localVideoOnly, ...s.localAudioOnly];
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

      // codecId
      ok(stat.codecId, `${stat.type}.codecId has a value`);
      ok(report.has(stat.codecId), `codecId ${stat.codecId} exists in report`);
      is(
        report.get(stat.codecId).type,
        "codec",
        `codecId ${stat.codecId} in report is codec type`
      );
      is(
        report.get(stat.codecId).mimeType.slice(0, 5),
        stat.kind,
        `codecId ${stat.codecId} in report is for a mimeType of the same ` +
          `media type as the referencing rtp stream stat`
      );

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

        // qpSum
        if (stat.qpSum !== undefined) {
          ok(
            stat.qpSum >= 0,
            `${stat.type}.qpSum is at least 0 ` +
              `${stat.kind} test. value=${stat.qpSum}`
          );
        }
      } else {
        is(
          stat.qpSum,
          undefined,
          `${stat.type}.qpSum does not exist when stat.kind != video`
        );
      }
    }

    if (stat.type == "inbound-rtp") {
      //
      // Required fields
      //

      // trackIdentifier
      is(typeof stat.trackIdentifier, "string");
      isnot(stat.trackIdentifier, "");

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

      // lastPacketReceivedTimestamp
      ok(
        stat.lastPacketReceivedTimestamp !== undefined,
        `${stat.type}.lastPacketReceivedTimestamp has a value`
      );

      // headerBytesReceived
      ok(
        stat.headerBytesReceived >= 0 && stat.headerBytesReceived < 50000,
        `${stat.type}.headerBytesReceived is sane for a short test. ` +
          `value=${stat.headerBytesReceived}`
      );

      // Always missing from libwebrtc stats
      // estimatedPlayoutTimestamp
      // ok(
      //   stat.estimatedPlayoutTimestamp !== undefined,
      //   `${stat.type}.estimatedPlayoutTimestamp has a value`
      // );

      // jitterBufferEmittedCount
      let expectedJitterBufferEmmitedCount = stat.kind == "video" ? 7 : 1000;
      ok(
        stat.jitterBufferEmittedCount > expectedJitterBufferEmmitedCount,
        `${stat.type}.jitterBufferEmittedCount is a sane number for a short ` +
          `${stat.kind} test. value=${stat.jitterBufferEmittedCount}`
      );

      // jitterBufferDelay
      let avgJitterBufferDelay =
        stat.jitterBufferDelay / stat.jitterBufferEmittedCount;
      ok(
        avgJitterBufferDelay > 0 && avgJitterBufferDelay < 10,
        `${stat.type}.jitterBufferDelay is a sane number for a short ` +
          `${stat.kind} test. value=${stat.jitterBufferDelay}/${stat.jitterBufferEmittedCount}=${avgJitterBufferDelay}`
      );

      //
      // Optional fields
      //

      //
      // Local audio only stats
      //
      if (stat.inner.kind != "audio") {
        expectations.localAudioOnly.forEach(field => {
          ok(
            stat[field] === undefined,
            `${stat.type} does not have field ${field}` +
              ` when kind is not 'audio'`
          );
        });
      } else {
        expectations.localAudioOnly.forEach(field => {
          ok(
            stat.inner[field] !== undefined,
            stat.type + " has field " + field + " when kind is video"
          );
        });
        // totalSamplesReceived
        ok(
          stat.totalSamplesReceived > 1000,
          `${stat.type}.totalSamplesReceived is a sane number for a short ` +
            `${stat.kind} test. value=${stat.totalSamplesReceived}`
        );

        // fecPacketsReceived
        ok(
          stat.fecPacketsReceived >= 0 && stat.fecPacketsReceived < 10 ** 5,
          `${stat.type}.fecPacketsReceived is a sane number for a short ` +
            `${stat.kind} test. value=${stat.fecPacketsReceived}`
        );

        // fecPacketsDiscarded
        ok(
          stat.fecPacketsDiscarded >= 0 && stat.fecPacketsDiscarded < 100,
          `${stat.type}.fecPacketsDiscarded is sane number for a short test. ` +
            `value=${stat.fecPacketsDiscarded}`
        );
        // concealedSamples
        ok(
          stat.concealedSamples >= 0 &&
            stat.concealedSamples <= stat.totalSamplesReceived,
          `${stat.type}.concealedSamples is a sane number for a short ` +
            `${stat.kind} test. value=${stat.concealedSamples}`
        );

        // silentConcealedSamples
        ok(
          stat.silentConcealedSamples >= 0 &&
            stat.silentConcealedSamples <= stat.concealedSamples,
          `${stat.type}.silentConcealedSamples is a sane number for a short ` +
            `${stat.kind} test. value=${stat.silentConcealedSamples}`
        );

        // concealmentEvents
        ok(
          stat.concealmentEvents >= 0 &&
            stat.concealmentEvents <= stat.packetsReceived,
          `${stat.type}.concealmentEvents is a sane number for a short ` +
            `${stat.kind} test. value=${stat.concealmentEvents}`
        );

        // insertedSamplesForDeceleration
        ok(
          stat.insertedSamplesForDeceleration >= 0 &&
            stat.insertedSamplesForDeceleration <= stat.totalSamplesReceived,
          `${stat.type}.insertedSamplesForDeceleration is a sane number for a short ` +
            `${stat.kind} test. value=${stat.insertedSamplesForDeceleration}`
        );

        // removedSamplesForAcceleration
        ok(
          stat.removedSamplesForAcceleration >= 0 &&
            stat.removedSamplesForAcceleration <= stat.totalSamplesReceived,
          `${stat.type}.removedSamplesForAcceleration is a sane number for a short ` +
            `${stat.kind} test. value=${stat.removedSamplesForAcceleration}`
        );

        // audioLevel
        ok(
          stat.audioLevel >= 0 && stat.audioLevel <= 128,
          `${stat.type}.bytesReceived is a sane number for a short ` +
            `${stat.kind} test. value=${stat.audioLevel}`
        );

        // totalAudioEnergy
        ok(
          stat.totalAudioEnergy >= 0 && stat.totalAudioEnergy <= 128,
          `${stat.type}.totalAudioEnergy is a sane number for a short ` +
            `${stat.kind} test. value=${stat.totalAudioEnergy}`
        );

        // totalSamplesDuration
        ok(
          stat.totalSamplesDuration >= 0 && stat.totalSamplesDuration <= 300,
          `${stat.type}.totalSamplesDuration is a sane number for a short ` +
            `${stat.kind} test. value=${stat.totalSamplesDuration}`
        );
      }

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
        // framesPerSecond
        ok(
          stat.framesPerSecond > 0 && stat.framesPerSecond < 70,
          `${stat.type}.framesPerSecond is a sane number for a short ` +
            `${stat.kind} test. value=${stat.framesPerSecond}`
        );

        // framesDecoded
        ok(
          stat.framesDecoded > 0 && stat.framesDecoded < 1000000,
          `${stat.type}.framesDecoded is a sane number for a short ` +
            `${stat.kind} test. value=${stat.framesDecoded}`
        );

        // framesDropped
        ok(
          stat.framesDropped >= 0 && stat.framesDropped < 100,
          `${stat.type}.framesDropped is a sane number for a short ` +
            `${stat.kind} test. value=${stat.framesDropped}`
        );

        // frameWidth
        ok(
          stat.frameWidth > 0 && stat.frameWidth < 100000,
          `${stat.type}.frameWidth is a sane number for a short ` +
            `${stat.kind} test. value=${stat.frameWidth}`
        );

        // frameHeight
        ok(
          stat.frameHeight > 0 && stat.frameHeight < 100000,
          `${stat.type}.frameHeight is a sane number for a short ` +
            `${stat.kind} test. value=${stat.frameHeight}`
        );

        // totalDecodeTime
        ok(
          stat.totalDecodeTime >= 0 && stat.totalDecodeTime < 300,
          `${stat.type}.totalDecodeTime is sane for a short test. ` +
            `value=${stat.totalDecodeTime}`
        );

        // totalProcessingDelay
        ok(
          stat.totalProcessingDelay < 100,
          `${stat.type}.totalProcessingDelay is sane number for a short test ` +
            `local only test. value=${stat.totalProcessingDelay}`
        );

        // totalInterFrameDelay
        ok(
          stat.totalInterFrameDelay >= 0 && stat.totalInterFrameDelay < 100,
          `${stat.type}.totalInterFrameDelay is sane for a short test. ` +
            `value=${stat.totalInterFrameDelay}`
        );

        // totalSquaredInterFrameDelay
        ok(
          stat.totalSquaredInterFrameDelay >= 0 &&
            stat.totalSquaredInterFrameDelay < 100,
          `${stat.type}.totalSquaredInterFrameDelay is sane for a short test. ` +
            `value=${stat.totalSquaredInterFrameDelay}`
        );

        // framesReceived
        ok(
          stat.framesReceived >= 0 && stat.framesReceived < 100000,
          `${stat.type}.framesReceived is a sane number for a short ` +
            `${stat.kind} test. value=${stat.framesReceived}`
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

      // totalRoundTripTime
      ok(
        stat.totalRoundTripTime < 50000,
        `${stat.type}.totalRoundTripTime is a sane number for a short ` +
          `${stat.kind} test. value=${stat.totalRoundTripTime}`
      );

      // fractionLost
      ok(
        stat.fractionLost < 0.2,
        `${stat.type}.fractionLost is a sane number for a short ` +
          `${stat.kind} test. value=${stat.fractionLost}`
      );

      // roundTripTimeMeasurements
      ok(
        stat.roundTripTimeMeasurements >= 1 &&
          stat.roundTripTimeMeasurements < 500,
        `${stat.type}.roundTripTimeMeasurements is a sane number for a short ` +
          `${stat.kind} test. value=${stat.roundTripTimeMeasurements}`
      );
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

      // headerBytesSent
      ok(
        stat.headerBytesSent > 0 &&
          stat.headerBytesSent < (stat.kind == "video" ? video1Min : audio1Min),
        `${stat.type}.headerBytesSent is a sane number for a short ` +
          `${stat.kind} test. value=${stat.headerBytesSent}`
      );

      // retransmittedPacketsSent
      ok(
        stat.retransmittedPacketsSent >= 0 &&
          stat.retransmittedPacketsSent <
            (stat.kind == "video" ? video1Min : audio1Min),
        `${stat.type}.retransmittedPacketsSent is a sane number for a short ` +
          `${stat.kind} test. value=${stat.retransmittedPacketsSent}`
      );

      // retransmittedBytesSent
      ok(
        stat.retransmittedBytesSent >= 0 &&
          stat.retransmittedBytesSent <
            (stat.kind == "video" ? video1Min : audio1Min),
        `${stat.type}.retransmittedBytesSent is a sane number for a short ` +
          `${stat.kind} test. value=${stat.retransmittedBytesSent}`
      );

      //
      // Optional fields
      //

      // qpSum
      // This is supported for all of our vpx codecs (on the encode side, see
      // bug 1519590)
      const mimeType = report.get(stat.codecId).mimeType;
      if (mimeType.includes("VP")) {
        ok(
          stat.qpSum >= 0,
          `${stat.type}.qpSum is a sane number (${stat.kind}) ` +
            `for ${report.get(stat.codecId).mimeType}. value=${stat.qpSum}`
        );
      } else if (mimeType.includes("H264")) {
        // OpenH264 encoder records QP so we check for either condition.
        if (!stat.qpSum && !("qpSum" in stat)) {
          ok(
            !stat.qpSum && !("qpSum" in stat),
            `${stat.type}.qpSum absent for ${report.get(stat.codecId).mimeType}`
          );
        } else {
          ok(
            stat.qpSum >= 0,
            `${stat.type}.qpSum is a sane number (${stat.kind}) ` +
              `for ${report.get(stat.codecId).mimeType}. value=${stat.qpSum}`
          );
        }
      } else {
        ok(
          !stat.qpSum && !("qpSum" in stat),
          `${stat.type}.qpSum absent for ${report.get(stat.codecId).mimeType}`
        );
      }

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

        // frameWidth
        ok(
          stat.frameWidth >= 0 && stat.frameWidth < 100000,
          `${stat.type}.frameWidth is a sane number for a short ` +
            `${stat.kind} test. value=${stat.frameWidth}`
        );

        // frameHeight
        ok(
          stat.frameHeight >= 0 && stat.frameHeight < 100000,
          `${stat.type}.frameHeight is a sane number for a short ` +
            `${stat.kind} test. value=${stat.frameHeight}`
        );

        // framesPerSecond
        ok(
          stat.framesPerSecond >= 0 && stat.framesPerSecond < 60,
          `${stat.type}.framesPerSecond is a sane number for a short ` +
            `${stat.kind} test. value=${stat.framesPerSecond}`
        );

        // framesSent
        ok(
          stat.framesSent >= 0 && stat.framesSent < 100000,
          `${stat.type}.framesSent is a sane number for a short ` +
            `${stat.kind} test. value=${stat.framesSent}`
        );

        // hugeFramesSent
        ok(
          stat.hugeFramesSent >= 0 && stat.hugeFramesSent < 100000,
          `${stat.type}.hugeFramesSent is a sane number for a short ` +
            `${stat.kind} test. value=${stat.hugeFramesSent}`
        );

        // totalEncodeTime
        ok(
          stat.totalEncodeTime >= 0,
          `${stat.type}.totalEncodeTime is a sane number for a short ` +
            `${stat.kind} test. value=${stat.totalEncodeTime}`
        );

        // totalEncodedBytesTarget
        ok(
          stat.totalEncodedBytesTarget > 1000,
          `${stat.type}.totalEncodedBytesTarget is a sane number for a short ` +
            `${stat.kind} test. value=${stat.totalEncodedBytesTarget}`
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
    } else if (stat.type == "media-source") {
      // trackIdentifier
      is(typeof stat.trackIdentifier, "string");
      isnot(stat.trackIdentifier, "");

      // kind
      is(typeof stat.kind, "string");
      ok(stat.kind == "audio" || stat.kind == "video");
      if (stat.inner.kind == "video") {
        expectations.localVideoOnly.forEach(field => {
          ok(
            stat.inner[field] !== undefined,
            `${stat.type} has field ` +
              `${field} when kind is video and isRemote is false`
          );
        });

        // frames
        ok(
          stat.frames >= 0 && stat.frames < 100000,
          `${stat.type}.frames is a sane number for a short ` +
            `${stat.kind} test. value=${stat.frames}`
        );

        // framesPerSecond
        ok(
          stat.framesPerSecond >= 0 && stat.framesPerSecond < 100,
          `${stat.type}.framesPerSecond is a sane number for a short ` +
            `${stat.kind} test. value=${stat.framesPerSecond}`
        );

        // width
        ok(
          stat.width >= 0 && stat.width < 1000000,
          `${stat.type}.width is a sane number for a ` +
            `${stat.kind} test. value=${stat.width}`
        );

        // height
        ok(
          stat.height >= 0 && stat.height < 1000000,
          `${stat.type}.height is a sane number for a ` +
            `${stat.kind} test. value=${stat.height}`
        );
      } else {
        expectations.localVideoOnly.forEach(field => {
          ok(
            stat[field] === undefined,
            `${stat.type} does not have field ` +
              `${field} when kind is not 'video'`
          );
        });
      }
    } else if (stat.type == "codec") {
      //
      // Required fields
      //

      // mimeType & payloadType
      switch (stat.mimeType) {
        case "audio/opus":
          is(stat.payloadType, 109, "codec.payloadType for opus");
          break;
        case "video/VP8":
          is(stat.payloadType, 120, "codec.payloadType for VP8");
          break;
        case "video/VP9":
          is(stat.payloadType, 121, "codec.payloadType for VP9");
          break;
        case "video/H264":
          ok(
            stat.payloadType == 97 || stat.payloadType == 126,
            `codec.payloadType for H264 was ${stat.payloadType}, exp. 97 or 126`
          );
          break;
        default:
          ok(
            false,
            `Unexpected codec.mimeType ${stat.mimeType} for payloadType ` +
              `${stat.payloadType}`
          );
          break;
      }

      // transportId
      // (no transport stats yet)
      ok(stat.transportId, "codec.transportId is set");

      // clockRate
      if (stat.mimeType.startsWith("audio")) {
        is(stat.clockRate, 48000, "codec.clockRate for audio/opus");
      } else if (stat.mimeType.startsWith("video")) {
        is(stat.clockRate, 90000, "codec.clockRate for video");
      }

      // sdpFmtpLine
      // (not technically mandated by spec, but expected here)
      ok(stat.sdpFmtpLine, "codec.sdpFmtpLine is set");
      const opusParams = [
        "maxplaybackrate",
        "maxaveragebitrate",
        "usedtx",
        "stereo",
        "useinbandfec",
        "cbr",
        "ptime",
        "minptime",
        "maxptime",
      ];
      const vpxParams = ["max-fs", "max-fr"];
      const h264Params = [
        "packetization-mode",
        "level-asymmetry-allowed",
        "profile-level-id",
        "max-fs",
        "max-cpb",
        "max-dpb",
        "max-br",
        "max-mbps",
      ];
      for (const param of stat.sdpFmtpLine.split(";")) {
        const [key, value] = param.split("=");
        if (stat.payloadType == 109) {
          ok(
            opusParams.includes(key),
            `codec.sdpFmtpLine param ${key}=${value} for opus`
          );
        } else if (stat.payloadType == 120 || stat.payloadType == 121) {
          ok(
            vpxParams.includes(key),
            `codec.sdpFmtpLine param ${key}=${value} for VPx`
          );
        } else if (stat.payloadType == 97 || stat.payloadType == 126) {
          ok(
            h264Params.includes(key),
            `codec.sdpFmtpLine param ${key}=${value} for H264`
          );
          if (key == "packetization-mode") {
            if (stat.payloadType == 97) {
              is(value, "0", "codec.sdpFmtpLine: H264 (97) packetization-mode");
            } else if (stat.payloadType == 126) {
              is(
                value,
                "1",
                "codec.sdpFmtpLine: H264 (126) packetization-mode"
              );
            }
          }
          if (key == "profile-level-id") {
            is(value, "42e01f", "codec.sdpFmtpLine: H264 profile-level-id");
          }
        }
      }

      //
      // Optional fields
      //

      // codecType
      ok(
        !Object.keys(stat).includes("codecType") ||
          stat.codecType == "encode" ||
          stat.codecType == "decode",
        "codec.codecType (${codec.codecType}) is an expected value or absent"
      );
      let numRecvStreams = 0;
      let numSendStreams = 0;
      const counts = {
        "inbound-rtp": 0,
        "outbound-rtp": 0,
        "remote-inbound-rtp": 0,
        "remote-outbound-rtp": 0,
      };
      const [kind] = stat.mimeType.split("/");
      report.forEach(other => {
        if (other.type == "inbound-rtp" && other.kind == kind) {
          numRecvStreams += 1;
        } else if (other.type == "outbound-rtp" && other.kind == kind) {
          numSendStreams += 1;
        }
        if (other.codecId == stat.id) {
          counts[other.type] += 1;
        }
      });
      const expectedCounts = {
        encode: {
          "inbound-rtp": 0,
          "outbound-rtp": numSendStreams,
          "remote-inbound-rtp": numSendStreams,
          "remote-outbound-rtp": 0,
        },
        decode: {
          "inbound-rtp": numRecvStreams,
          "outbound-rtp": 0,
          "remote-inbound-rtp": 0,
          "remote-outbound-rtp": numRecvStreams,
        },
        absent: {
          "inbound-rtp": numRecvStreams,
          "outbound-rtp": numSendStreams,
          "remote-inbound-rtp": numSendStreams,
          "remote-outbound-rtp": numRecvStreams,
        },
      };
      // Note that the logic above assumes at most one sender and at most one
      // receiver was used to generate this stats report. If more senders or
      // receivers are present, they'd be referring to not only this codec stat,
      // skewing `numSendStreams` and `numRecvStreams` above.
      // This could be fixed when we support `senderId` and `receiverId` in
      // RTCOutboundRtpStreamStats and RTCInboundRtpStreamStats respectively.
      for (const [key, value] of Object.entries(counts)) {
        is(
          value,
          expectedCounts[stat.codecType || "absent"][key],
          `codec.codecType ${stat.codecType || "absent"} ref from ${key} stat`
        );
      }

      // channels
      if (stat.mimeType.startsWith("audio")) {
        ok(stat.channels, "codec.channels should exist for audio");
        if (stat.channels) {
          if (stat.sdpFmtpLine.includes("stereo=1")) {
            is(stat.channels, 2, "codec.channels for stereo audio");
          } else {
            is(stat.channels, 1, "codec.channels for mono audio");
          }
        }
      } else {
        ok(!stat.channels, "codec.channels should not exist for video");
      }
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
          stat.bytesSent > 100,
          `${stat.type}.bytesSent is a sane number (>100) if media is flowing. ` +
            `value=${stat.bytesSent}`
        );

        // bytesReceived
        ok(
          stat.bytesReceived > 100,
          `${stat.type}.bytesReceived is a sane number (>100) if media is flowing. ` +
            `value=${stat.bytesReceived}`
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
          s.track ? Math.max(1, s.getParameters()?.encodings?.length ?? 0) : 0
        );
      }),
    ]);
  });
}
