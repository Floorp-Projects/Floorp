/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const ChromeUtils = require("ChromeUtils");
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

const {
  getAdHocFrontOrPrimitiveGrip,
} = require("devtools/shared/fronts/object");

const CHROME_PREFIX = "chrome://mochitests/content/browser/";
const STUBS_FOLDER = "devtools/client/webconsole/test/node/fixtures/stubs/";
const STUBS_UPDATE_ENV = "WEBCONSOLE_STUBS_UPDATE";

// eslint-disable-next-line complexity
function getCleanedPacket(key, packet) {
  const { stubPackets } = require(CHROME_PREFIX + STUBS_FOLDER + "index");

  // Strip escaped characters.
  const safeKey = key
    .replace(/\\n/g, "\n")
    .replace(/\\r/g, "\r")
    .replace(/\\\"/g, `\"`)
    .replace(/\\\'/g, `\'`);

  // If the stub already exist, we want to ignore irrelevant properties
  // (actor, timeStamp, timer, ...) that might changed and "pollute"
  // the diff resulting from this stub generation.
  let res;
  if (stubPackets.has(safeKey)) {
    const existingPacket = stubPackets.get(safeKey);
    res = Object.assign({}, packet, {
      from: existingPacket.from,
    });

    // Clean root timestamp.
    if (res.timestamp) {
      res.timestamp = existingPacket.timestamp;
    }

    if (res.timeStamp) {
      res.timeStamp = existingPacket.timeStamp;
    }

    if (res.innerWindowID) {
      res.innerWindowID = existingPacket.innerWindowID;
    }

    if (res.startedDateTime) {
      res.startedDateTime = existingPacket.startedDateTime;
    }

    if (res.actor) {
      res.actor = existingPacket.actor;
    }

    if (res.channelId) {
      res.channelId = existingPacket.channelId;
    }

    if (res.resultID) {
      res.resultID = existingPacket.resultID;
    }

    if (res.message) {
      // Clean timeStamp on the message prop.
      res.message.timeStamp = existingPacket.message.timeStamp;
      if (res.message.timer) {
        // Clean timer properties on the message.
        // Those properties are found on console.time, timeLog and timeEnd calls,
        // and those time can vary, which is why we need to clean them.
        if ("duration" in res.message.timer) {
          res.message.timer.duration = existingPacket.message.timer.duration;
        }
      }
      // Clean innerWindowId on the message prop.
      res.message.innerWindowID = existingPacket.message.innerWindowID;

      if (Array.isArray(res.message.arguments)) {
        res.message.arguments = res.message.arguments.map((argument, i) => {
          if (!argument || typeof argument !== "object") {
            return argument;
          }

          const newArgument = Object.assign({}, argument);
          const existingArgument = existingPacket.message.arguments[i];

          if (existingArgument && newArgument._grip) {
            // Clean actor ids on each message.arguments item.
            copyExistingActor(newArgument, existingArgument);

            // `window`'s properties count can vary from OS to OS, so we
            // clean the `ownPropertyLength` property from the grip.
            if (newArgument._grip.class === "Window") {
              newArgument._grip.ownPropertyLength =
                existingArgument._grip.ownPropertyLength;
            }
          }
          return newArgument;
        });
      }

      if (res.message.sourceId) {
        res.message.sourceId = existingPacket.message.sourceId;
      }

      if (Array.isArray(res.message.stacktrace)) {
        res.message.stacktrace = res.message.stacktrace.map((frame, i) => {
          const existingFrame = existingPacket.message.stacktrace[i];
          if (frame && existingFrame && frame.sourceId) {
            frame.sourceId = existingFrame.sourceId;
          }
          return frame;
        });
      }
    }

    if (res.result && res.result._grip && existingPacket.result) {
      // Clean actor ids on evaluation result messages.
      copyExistingActor(res.result, existingPacket.result);

      if (res.result._grip.preview) {
        if (res.result._grip.preview.timestamp) {
          // Clean timestamp there too.
          res.result._grip.preview.timestamp =
            existingPacket.result._grip.preview.timestamp;
        }
      }
    }

    if (res.exception && existingPacket.exception) {
      // Clean actor ids on exception messages.
      copyExistingActor(res.exception, existingPacket.exception);

      if (
        res.exception._grip &&
        res.exception._grip.preview &&
        existingPacket.exception._grip &&
        existingPacket.exception._grip.preview
      ) {
        if (res.exception._grip.preview.timestamp) {
          // Clean timestamp there too.
          res.exception._grip.preview.timestamp =
            existingPacket.exception._grip.preview.timestamp;
        }

        if (
          typeof res.exception._grip.preview.message === "object" &&
          res.exception._grip.preview.message._grip.type === "longString" &&
          typeof existingPacket.exception._grip.preview.message === "object" &&
          existingPacket.exception._grip.preview.message._grip.type ===
            "longString"
        ) {
          copyExistingActor(
            res.exception._grip.preview.message,
            existingPacket.exception._grip.preview.message
          );
        }
      }

      if (
        typeof res.exceptionMessage === "object" &&
        res.exceptionMessage._grip &&
        res.exceptionMessage._grip.type === "longString"
      ) {
        copyExistingActor(
          res.exceptionMessage,
          existingPacket.exceptionMessage
        );
      }
    }

    if (res.eventActor) {
      // Clean actor ids, timeStamp and startedDateTime on network messages.
      res.eventActor.actor = existingPacket.eventActor.actor;
      res.eventActor.startedDateTime =
        existingPacket.eventActor.startedDateTime;
      res.eventActor.timeStamp = existingPacket.eventActor.timeStamp;
    }

    if (res.pageError) {
      // Clean timeStamp and innerWindowID on pageError messages.
      res.pageError.timeStamp = existingPacket.pageError.timeStamp;
      res.pageError.innerWindowID = existingPacket.pageError.innerWindowID;

      if (
        typeof res.pageError.errorMessage === "object" &&
        res.pageError.errorMessage._grip &&
        res.pageError.errorMessage._grip.type === "longString"
      ) {
        copyExistingActor(
          res.pageError.errorMessage,
          existingPacket.pageError.errorMessage
        );
      }

      if (res.pageError.sourceId) {
        res.pageError.sourceId = existingPacket.pageError.sourceId;
      }

      if (Array.isArray(res.pageError.stacktrace)) {
        res.pageError.stacktrace = res.pageError.stacktrace.map((frame, i) => {
          const existingFrame = existingPacket.pageError.stacktrace[i];
          if (frame && existingFrame && frame.sourceId) {
            frame.sourceId = existingFrame.sourceId;
          }
          return frame;
        });
      }
    }

    if (Array.isArray(res.exceptionStack)) {
      res.exceptionStack = res.exceptionStack.map((frame, i) => {
        const existingFrame = existingPacket.exceptionStack[i];
        if (frame && existingFrame && frame.sourceId) {
          frame.sourceId = existingFrame.sourceId;
        }
        return frame;
      });
    }

    if (res.frame && existingPacket.frame) {
      res.frame.sourceId = existingPacket.frame.sourceId;
    }

    if (res.packet) {
      const override = {};
      const keys = ["totalTime", "from", "contentSize", "transferredSize"];
      keys.forEach(x => {
        if (res.packet[x] !== undefined) {
          override[x] = existingPacket.packet[key];
        }
      });
      res.packet = Object.assign({}, res.packet, override);
    }

    if (res.networkInfo) {
      if (res.networkInfo.timeStamp) {
        res.networkInfo.timeStamp = existingPacket.networkInfo.timeStamp;
      }

      if (res.networkInfo.startedDateTime) {
        res.networkInfo.startedDateTime =
          existingPacket.networkInfo.startedDateTime;
      }

      if (res.networkInfo.totalTime) {
        res.networkInfo.totalTime = existingPacket.networkInfo.totalTime;
      }

      if (res.networkInfo.actor) {
        res.networkInfo.actor = existingPacket.networkInfo.actor;
      }

      if (res.networkInfo.request && res.networkInfo.request.headersSize) {
        res.networkInfo.request.headersSize =
          existingPacket.networkInfo.request.headersSize;
      }

      if (
        res.networkInfo.response &&
        res.networkInfo.response.headersSize !== undefined
      ) {
        res.networkInfo.response.headersSize =
          existingPacket.networkInfo.response.headersSize;
      }
      if (
        res.networkInfo.response &&
        res.networkInfo.response.bodySize !== undefined
      ) {
        res.networkInfo.response.bodySize =
          existingPacket.networkInfo.response.bodySize;
      }
      if (
        res.networkInfo.response &&
        res.networkInfo.response.transferredSize !== undefined
      ) {
        res.networkInfo.response.transferredSize =
          existingPacket.networkInfo.response.transferredSize;
      }
    }

    if (res.updates && Array.isArray(res.updates)) {
      res.updates.sort();
    }

    if (res.helperResult) {
      copyExistingActor(
        res.helperResult.object,
        existingPacket.helperResult.object
      );
    }
  } else {
    res = packet;
  }

  return res;
}

function copyExistingActor(front1, front2) {
  if (!front1 || !front2) {
    return;
  }

  if (front1.actorID && front2.actorID) {
    front1.actorID = front2.actorID;
  }

  if (
    front1._grip &&
    front2._grip &&
    front1._grip.actor &&
    front2._grip.actor
  ) {
    front1._grip.actor = front2._grip.actor;
  }
}

/**
 * Write stubs to a given file
 *
 * @param {Object} env
 * @param {String} fileName: The file to write the stubs in.
 * @param {Map} packets: A Map of the packets.
 * @param {Boolean} isNetworkMessage: Is the packets are networkMessage packets
 */
async function writeStubsToFile(env, fileName, packets, isNetworkMessage) {
  const mozRepo = env.get("MOZ_DEVELOPER_REPO_DIR");
  const filePath = `${mozRepo}/${STUBS_FOLDER + fileName}`;

  const serializedPackets = Array.from(packets.entries()).map(
    ([key, packet]) => {
      const stringifiedPacket = getSerializedPacket(packet);
      return `rawPackets.set(\`${key}\`, ${stringifiedPacket});`;
    }
  );

  const fileContent = `/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable max-len */

"use strict";

/*
 * THIS FILE IS AUTOGENERATED. DO NOT MODIFY BY HAND. RUN TESTS IN FIXTURES/ TO UPDATE.
 */

const {
  parsePacketsWithFronts,
} = require("chrome://mochitests/content/browser/devtools/client/webconsole/test/browser/stub-generator-helpers");
const { prepareMessage } = require("devtools/client/webconsole/utils/messages");
const {
  ConsoleMessage,
  NetworkEventMessage,
} = require("devtools/client/webconsole/types");

const rawPackets = new Map();
${serializedPackets.join("\n\n")}


const stubPackets = parsePacketsWithFronts(rawPackets);

const stubPreparedMessages = new Map();
for (const [key, packet] of Array.from(stubPackets.entries())) {
  const transformedPacket = prepareMessage(${
    isNetworkMessage ? "packet.networkInfo || packet" : "packet"
  }, {
    getNextId: () => "1",
  });
  const message = ${
    isNetworkMessage
      ? "NetworkEventMessage(transformedPacket);"
      : "ConsoleMessage(transformedPacket);"
  }
  stubPreparedMessages.set(key, message);
}

module.exports = {
  rawPackets,
  stubPreparedMessages,
  stubPackets,
};
`;

  await OS.File.writeAtomic(filePath, fileContent);
}

function getStubFile(fileName) {
  return require(CHROME_PREFIX + STUBS_FOLDER + fileName);
}

function getSerializedPacket(packet) {
  return JSON.stringify(
    packet,
    function(_, value) {
      // The message can have fronts that we need to serialize
      if (value && value._grip) {
        return { _grip: value._grip, actorID: value.actorID };
      }

      return value;
    },
    2
  );
}

/**
 *
 * @param {Map} rawPackets
 */
function parsePacketsWithFronts(rawPackets) {
  const packets = new Map();
  for (const [key, packet] of rawPackets.entries()) {
    const newPacket = parsePacketAndCreateFronts(packet);
    packets.set(key, newPacket);
  }
  return packets;
}

function parsePacketAndCreateFronts(packet) {
  if (!packet) {
    return packet;
  }
  if (Array.isArray(packet)) {
    packet.forEach(parsePacketAndCreateFronts);
  }
  if (typeof packet === "object") {
    for (const [key, value] of Object.entries(packet)) {
      if (value && value._grip) {
        packet[key] = getAdHocFrontOrPrimitiveGrip(value._grip, {
          conn: {
            poolFor: () => {},
            addActorPool: () => {},
            getFrontByID: () => {},
          },
          manage: () => {},
        });
      } else {
        packet[key] = parsePacketAndCreateFronts(value);
      }
    }
  }

  return packet;
}

module.exports = {
  STUBS_UPDATE_ENV,
  getStubFile,
  getCleanedPacket,
  getSerializedPacket,
  parsePacketsWithFronts,
  writeStubsToFile,
};
