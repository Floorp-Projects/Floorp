/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  getAdHocFrontOrPrimitiveGrip,
} = require("devtools/client/fronts/object");

const CHROME_PREFIX = "chrome://mochitests/content/browser/";
const STUBS_FOLDER = "devtools/client/webconsole/test/node/fixtures/stubs/";
const STUBS_UPDATE_ENV = "WEBCONSOLE_STUBS_UPDATE";

async function createCommandsForTab(tab) {
  const {
    CommandsFactory,
  } = require("devtools/shared/commands/commands-factory");
  const commands = await CommandsFactory.forTab(tab);
  return commands;
}

async function createCommandsForMainProcess() {
  const {
    CommandsFactory,
  } = require("devtools/shared/commands/commands-factory");
  const commands = await CommandsFactory.forMainProcess();
  return commands;
}

// eslint-disable-next-line complexity
function getCleanedPacket(key, packet) {
  const { stubPackets } = require(CHROME_PREFIX + STUBS_FOLDER + "index");

  // Strip escaped characters.
  const safeKey = key
    .replace(/\\n/g, "\n")
    .replace(/\\r/g, "\r")
    .replace(/\\\"/g, `\"`)
    .replace(/\\\'/g, `\'`);

  cleanTimeStamp(packet);
  // Remove the targetFront property that has a cyclical reference and that we don't need
  // in our node tests.
  delete packet.targetFront;

  if (!stubPackets.has(safeKey)) {
    return packet;
  }

  // If the stub already exist, we want to ignore irrelevant properties (generated id, timer, …)
  // that might changed and "pollute" the diff resulting from this stub generation.
  const existingPacket = stubPackets.get(safeKey);
  const res = Object.assign({}, packet, {
    from: existingPacket.from,
  });

  if (res.innerWindowID) {
    res.innerWindowID = existingPacket.innerWindowID;
  }

  if (res.startedDateTime) {
    res.startedDateTime = existingPacket.startedDateTime;
  }

  if (res.channelId) {
    res.channelId = existingPacket.channelId;
  }

  if (res.resultID) {
    res.resultID = existingPacket.resultID;
  }

  if (res.message) {
    if (res.message.timer) {
      // Clean timer properties on the message.
      // Those properties are found on console.time, timeLog and timeEnd calls,
      // and those time can vary, which is why we need to clean them.
      if ("duration" in res.message.timer) {
        res.message.timer.duration = existingPacket.message.timer.duration;
      }
    }
    // Clean innerWindowId on the message prop.
    if (existingPacket.message.innerWindowID) {
      res.message.innerWindowID = existingPacket.message.innerWindowID;
    }

    if (Array.isArray(res.message.arguments)) {
      res.message.arguments = res.message.arguments.map((argument, i) => {
        if (!argument || typeof argument !== "object") {
          return argument;
        }

        const newArgument = Object.assign({}, argument);
        const existingArgument = existingPacket.message.arguments[i];

        if (existingArgument && newArgument._grip) {
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

  if (res.eventActor) {
    // Clean startedDateTime on network messages.
    res.eventActor.startedDateTime = existingPacket.startedDateTime;
  }

  if (res.pageError) {
    // Clean innerWindowID on pageError messages.
    res.pageError.innerWindowID = existingPacket.pageError.innerWindowID;

    if (res.pageError.sourceId) {
      res.pageError.sourceId = existingPacket.pageError.sourceId;
    }

    if (
      Array.isArray(res.pageError.stacktrace) &&
      Array.isArray(existingPacket.pageError.stacktrace)
    ) {
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
      // We're replacing sourceId here even if the property in frame is null to avoid
      // a frequent intermittent. The sourceId is retrieved from the Debugger#findSources
      // API, which is not deterministic (See https://searchfox.org/mozilla-central/rev/b172dd415c475e8b2899560e6005b3a953bead2a/js/src/doc/Debugger/Debugger.md#367-375)
      // This should be fixed in Bug 1717037.
      if (frame && existingFrame && "sourceId" in frame) {
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

  if (res.startedDateTime) {
    res.startedDateTime = existingPacket.startedDateTime;
  }

  if (res.totalTime && existingPacket.totalTime) {
    res.totalTime = existingPacket.totalTime;
  }

  if (res.securityState && existingPacket.securityState) {
    res.securityState = existingPacket.securityState;
  }

  if (res.waitingTime && existingPacket.waitingTime) {
    res.waitingTime = existingPacket.waitingTime;
  }

  return res;
}

function cleanTimeStamp(packet) {
  // We want to have the same timestamp for every stub, so they won't be re-sorted when
  // adding them to the store.
  const uniqueTimeStamp = 1572867483805;
  // lowercased timestamp
  if (packet.timestamp) {
    packet.timestamp = uniqueTimeStamp;
  }

  // camelcased timestamp
  if (packet.timeStamp) {
    packet.timeStamp = uniqueTimeStamp;
  }

  if (packet.startTime) {
    packet.startTime = uniqueTimeStamp;
  }

  if (packet?.message?.timeStamp) {
    packet.message.timeStamp = uniqueTimeStamp;
  }

  if (packet?.result?._grip?.preview?.timestamp) {
    packet.result._grip.preview.timestamp = uniqueTimeStamp;
  }

  if (packet?.result?._grip?.promiseState?.creationTimestamp) {
    packet.result._grip.promiseState.creationTimestamp = uniqueTimeStamp;
  }

  if (packet?.exception?._grip?.preview?.timestamp) {
    packet.exception._grip.preview.timestamp = uniqueTimeStamp;
  }

  if (packet?.eventActor?.timeStamp) {
    packet.eventActor.timeStamp = uniqueTimeStamp;
  }

  if (packet?.pageError?.timeStamp) {
    packet.pageError.timeStamp = uniqueTimeStamp;
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
 * THIS FILE IS AUTOGENERATED. DO NOT MODIFY BY HAND. SEE devtools/client/webconsole/test/README.md.
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
  const transformedPacket = prepareMessage(${"packet"}, {
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

  await IOUtils.write(filePath, new TextEncoder().encode(fileContent));
}

function getStubFile(fileName) {
  return require(CHROME_PREFIX + STUBS_FOLDER + fileName);
}

function sortObjectKeys(obj) {
  const isArray = Array.isArray(obj);
  const isObject = Object.prototype.toString.call(obj) === "[object Object]";
  const isFront = obj?._grip;

  if (isObject && !isFront) {
    // Reorder keys for objects, but skip fronts to avoid infinite recursion.
    const sortedKeys = Object.keys(obj).sort((k1, k2) => k1.localeCompare(k2));
    const withSortedKeys = {};
    sortedKeys.forEach(k => {
      withSortedKeys[k] = k !== "stacktrace" ? sortObjectKeys(obj[k]) : obj[k];
    });
    return withSortedKeys;
  } else if (isArray) {
    return obj.map(item => sortObjectKeys(item));
  }
  return obj;
}

/**
 * @param {Object} packet
 *        The packet to serialize.
 * @param {Object} options
 * @param {Boolean} options.sortKeys
 *        Pass true to sort all keys alphabetically in the packet before serialization.
 *        For instance stub comparison should not fail if the order of properties changed.
 * @param {Boolean} options.replaceActorIds
 *        Pass true to replace actorIDs with a fake one so it's easier to compare stubs
 *        that includes grips.
 */
function getSerializedPacket(
  packet,
  { sortKeys = false, replaceActorIds = false } = {}
) {
  if (sortKeys) {
    packet = sortObjectKeys(packet);
  }

  const actorIdPlaceholder = "XXX";

  return JSON.stringify(
    packet,
    function(key, value) {
      // The message can have fronts that we need to serialize
      if (value && value._grip) {
        return {
          _grip: value._grip,
          actorID: replaceActorIds ? actorIdPlaceholder : value.actorID,
        };
      }

      if (
        replaceActorIds &&
        (key === "actor" || key === "actorID" || key === "sourceId") &&
        typeof value === "string"
      ) {
        return actorIdPlaceholder;
      }

      if (key === "resourceId") {
        return undefined;
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
      if (value?._grip) {
        // The message of an error grip might be a longString.
        if (value._grip?.preview?.message?._grip) {
          value._grip.preview.message = value._grip.preview.message._grip;
        }

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
  createCommandsForTab,
  createCommandsForMainProcess,
  getStubFile,
  getCleanedPacket,
  getSerializedPacket,
  parsePacketsWithFronts,
  parsePacketAndCreateFronts,
  writeStubsToFile,
};
