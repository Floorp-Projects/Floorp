/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { prepareMessage } = require("devtools/client/webconsole/utils/messages");

const STUBS_FOLDER = "/devtools/client/webconsole/test/node/fixtures/stubs/";
const STUBS_UPDATE_ENV = "WEBCONSOLE_STUBS_UPDATE";

const {
  stubPackets,
} = require("devtools/client/webconsole/test/node/fixtures/stubs/index.js");

const cachedPackets = {};

/* eslint-disable complexity */
function getCleanedPacket(key, packet) {
  if (Object.keys(cachedPackets).includes(key)) {
    return cachedPackets[key];
  }

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

          if (existingArgument) {
            // Clean actor ids on each message.arguments item.
            if (newArgument.actor) {
              newArgument.actor = existingArgument.actor;
            }

            // `window`'s properties count can vary from OS to OS, so we
            // clean the `ownPropertyLength` property from the grip.
            if (newArgument.class === "Window") {
              newArgument.ownPropertyLength =
                existingArgument.ownPropertyLength;
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

    if (res.result && existingPacket.result) {
      // Clean actor ids on evaluation result messages.
      res.result.actor = existingPacket.result.actor;
      if (res.result.preview) {
        if (res.result.preview.timestamp) {
          // Clean timestamp there too.
          res.result.preview.timestamp =
            existingPacket.result.preview.timestamp;
        }
      }
    }

    if (res.exception && existingPacket.exception) {
      // Clean actor ids on exception messages.
      if (existingPacket.exception.actor) {
        res.exception.actor = existingPacket.exception.actor;
      }

      if (res.exception.preview) {
        if (res.exception.preview.timestamp) {
          // Clean timestamp there too.
          res.exception.preview.timestamp =
            existingPacket.exception.preview.timestamp;
        }

        if (
          typeof res.exception.preview.message === "object" &&
          res.exception.preview.message.type === "longString"
        ) {
          res.exception.preview.message.actor =
            existingPacket.exception.preview.message.actor;
        }
      }

      if (
        typeof res.exceptionMessage === "object" &&
        res.exceptionMessage.type === "longString"
      ) {
        res.exceptionMessage.actor = existingPacket.exceptionMessage.actor;
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
        res.pageError.errorMessage.type === "longString"
      ) {
        res.pageError.errorMessage.actor =
          existingPacket.pageError.errorMessage.actor;
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

    if (res.helperResult) {
      if (res.helperResult.object) {
        res.helperResult.object.actor =
          existingPacket.helperResult.object.actor;
      }
    }
  } else {
    res = packet;
  }

  cachedPackets[key] = res;
  return res;
}
/* eslint-enable complexity */

function formatPacket(key, packet) {
  const stringifiedPacket = JSON.stringify(
    getCleanedPacket(key, packet),
    null,
    2
  );
  return `stubPackets.set(\`${key}\`, ${stringifiedPacket});`;
}

function formatStub(key, packet) {
  const prepared = prepareMessage(getCleanedPacket(key, packet), {
    getNextId: () => "1",
  });
  const stringifiedMessage = JSON.stringify(prepared, null, 2);
  return `stubPreparedMessages.set(\`${key}\`, new ConsoleMessage(${stringifiedMessage}));`;
}

function formatNetworkEventStub(key, packet) {
  const cleanedPacket = getCleanedPacket(key, packet);
  const networkInfo = cleanedPacket.networkInfo
    ? cleanedPacket.networkInfo
    : cleanedPacket;

  const prepared = prepareMessage(networkInfo, { getNextId: () => "1" });
  const stringifiedMessage = JSON.stringify(prepared, null, 2);
  return (
    `stubPreparedMessages.set("${key}", ` +
    `new NetworkEventMessage(${stringifiedMessage}));`
  );
}

function formatFile(stubs, type) {
  return `/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable max-len */

"use strict";

/*
 * THIS FILE IS AUTOGENERATED. DO NOT MODIFY BY HAND. RUN TESTS IN FIXTURES/ TO UPDATE.
 */

const { ${type} } =
  require("devtools/client/webconsole/types");

const stubPreparedMessages = new Map();
const stubPackets = new Map();
${stubs.preparedMessages.join("\n\n")}

${stubs.packets.join("\n\n")}

module.exports = {
  stubPreparedMessages,
  stubPackets,
};
`;
}

function getStubFilePath(fileName, env) {
  return env.get("MOZ_DEVELOPER_REPO_DIR") + STUBS_FOLDER + fileName;
}

module.exports = {
  STUBS_FOLDER,
  STUBS_UPDATE_ENV,
  formatFile,
  formatNetworkEventStub,
  formatPacket,
  formatStub,
  getStubFilePath,
};
